param(
    [int]$Device = -1,
    [int]$Port = 5801,
    [string]$Source = "auto",
    [string]$ListenHost = "127.0.0.1",
    [switch]$SkipAurora
)

$ErrorActionPreference = "Stop"

function Initialize-Utf8Console {
    try { & chcp.com 65001 | Out-Null } catch {}
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [Console]::InputEncoding = $utf8NoBom
    [Console]::OutputEncoding = $utf8NoBom
    $global:OutputEncoding = $utf8NoBom
    $env:PYTHONUTF8 = "1"
    $env:PYTHONIOENCODING = "utf-8"
    $loopbackBypass = "127.0.0.1,localhost,::1"
    $env:NO_PROXY = $loopbackBypass
    $env:no_proxy = $loopbackBypass
}

function Get-PythonExecutable {
    $candidates = @(
        (Join-Path $ScriptDir "..\..\venv_39\Scripts\python.exe"),
        "python"
    ) | Where-Object { Test-Path $_ }
    foreach ($candidate in $candidates) {
        try {
            $version = & $candidate --version 2>&1
            if ($version -match "Python 3") { return $candidate }
        } catch {}
    }
    return "python"
}

function Start-AuroraBootstrap {
    param([switch]$Disabled)

    if ($Disabled) {
        Write-Host "[Aurora] SkipAurora set. Not launching Aurora.exe"
        return
    }

    $repoRoot = Split-Path (Split-Path $ScriptDir -Parent) -Parent
    $auroraExe = Join-Path $repoRoot "Aurora-2.0.0-ciciec.16\Aurora.exe"
    if (-not (Test-Path $auroraExe)) {
        Write-Host "[Aurora] Aurora.exe not found. Continue without bootstrap: $auroraExe"
        return
    }

    $resolvedAuroraExe = [System.IO.Path]::GetFullPath($auroraExe)
    $auroraDir = Split-Path $resolvedAuroraExe -Parent
    $existingProcess = $null
    try {
        $auroraProcesses = Get-Process -Name "Aurora" -ErrorAction SilentlyContinue
        foreach ($proc in $auroraProcesses) {
            if ($proc.Path -and ([System.StringComparer]::OrdinalIgnoreCase.Equals($proc.Path, $resolvedAuroraExe))) {
                $existingProcess = $proc
                break
            }
        }
    } catch {}

    if ($existingProcess) {
        Write-Host "[Aurora] Aurora.exe already running (PID $($existingProcess.Id))"
        return
    }

    Write-Host "[Aurora] Launching Aurora.exe for camera initialization..."
    Start-Process -FilePath $resolvedAuroraExe -WorkingDirectory $auroraDir | Out-Null
    Start-Sleep -Seconds 3
}

function Start-BrowserWhenReady {
    param([int]$ReadyPort, [string]$Url)
    try {
        $browserWorker = @'
$deadline = [DateTime]::UtcNow.AddSeconds(180)
$baseUrl = "__URL__"
while ([DateTime]::UtcNow -lt $deadline) {
    try {
        $client = New-Object System.Net.Sockets.TcpClient
        $async = $client.BeginConnect("127.0.0.1", __PORT__, $null, $null)
        if ($async.AsyncWaitHandle.WaitOne(200)) {
            $client.EndConnect($async)
            $client.Close()
            $response = Invoke-WebRequest -Uri $baseUrl -UseBasicParsing -TimeoutSec 1 -ErrorAction SilentlyContinue
            if ($response.StatusCode -ge 200) { Start-Process $baseUrl | Out-Null; exit 0 }
        } else { $client.Close() }
    } catch {}
    Start-Sleep -Milliseconds 500
}
'@
        $browserWorker = $browserWorker.Replace("__PORT__", [string]$ReadyPort).Replace("__URL__", $Url)
        $encodedCommand = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($browserWorker))
        $shell = Get-Command pwsh -ErrorAction SilentlyContinue
        if (-not $shell) { $shell = Get-Command powershell -ErrorAction SilentlyContinue }
        if ($shell) {
            Start-Process -FilePath $shell.Source -WindowStyle Hidden -ArgumentList @("-NoProfile", "-EncodedCommand", $encodedCommand) | Out-Null
            return
        }
    } catch {}
}

function Test-PortAvailable {
    param(
        [string]$BindHost,
        [int]$BindPort
    )
    try {
        $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Parse($BindHost), $BindPort)
        $listener.Start()
        $listener.Stop()
        return $true
    } catch {
        return $false
    }
}

function Read-OwnerState {
    param([string]$StatePath)
    try {
        if (-not (Test-Path $StatePath)) { return $null }
        return Get-Content -Raw -Encoding UTF8 $StatePath | ConvertFrom-Json
    } catch {
        return $null
    }
}

function Clear-OwnerState {
    param([string]$StatePath)
    try { Remove-Item -Force $StatePath -ErrorAction SilentlyContinue } catch {}
}

function Stop-OwnedProcess {
    param(
        [string]$StatePath,
        [string]$ScriptName,
        [string]$Label
    )
    $state = Read-OwnerState -StatePath $StatePath
    if (-not $state -or -not $state.pid) {
        Clear-OwnerState -StatePath $StatePath
        return @()
    }
    $ownedPid = [int]$state.pid
    if ($ownedPid -le 0 -or $ownedPid -eq $PID) {
        Clear-OwnerState -StatePath $StatePath
        return @()
    }
    try {
        $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$ownedPid" -ErrorAction SilentlyContinue
        if ($proc -and ([string]$proc.CommandLine) -match [regex]::Escape($ScriptName)) {
            Write-Host "[Aurora] Releasing stale $Label owner (PID $ownedPid)"
            Stop-Process -Id $ownedPid -Force -ErrorAction SilentlyContinue
            Clear-OwnerState -StatePath $StatePath
            return @($ownedPid)
        }
    } catch {}
    Clear-OwnerState -StatePath $StatePath
    return @()
}

function Save-OwnerState {
    param(
        [string]$StatePath,
        [int]$OwnerPid,
        [int]$OwnerPort,
        [string]$ScriptPath
    )
    $payload = [ordered]@{
        pid = $OwnerPid
        port = $OwnerPort
        script = $ScriptPath
    }
    try {
        $payload | ConvertTo-Json -Compress | Set-Content -Encoding UTF8 $StatePath
    } catch {}
}

function Resolve-AvailablePort {
    param(
        [string]$BindHost,
        [int]$PreferredPort
    )
    if (Test-PortAvailable -BindHost $BindHost -BindPort $PreferredPort) {
        return $PreferredPort
    }
    for ($candidate = $PreferredPort + 1; $candidate -le $PreferredPort + 30; $candidate++) {
        if (Test-PortAvailable -BindHost $BindHost -BindPort $candidate) {
            return $candidate
        }
    }
    throw "Could not find available port (starting from: $PreferredPort)"
}

function Stop-StaleCompanionOnPort {
    param(
        [int]$BindPort,
        [string]$OwnerStatePath
    )
    $killedPids = @(Stop-OwnedProcess -StatePath $OwnerStatePath -ScriptName "aurora_companion.py" -Label "Companion")
    try {
        $connections = Get-NetTCPConnection -LocalPort $BindPort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $pId = [int]$conn.OwningProcess
            if ($pId -le 0 -or $pId -eq $PID -or $killedPids -contains $pId) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pId" -ErrorAction SilentlyContinue
            if ($proc -and ([string]$proc.CommandLine) -match "aurora_companion\.py") {
                Write-Host "[Aurora] Releasing stale Companion port $BindPort (PID $pId)"
                Stop-Process -Id $pId -Force -ErrorAction SilentlyContinue
                $killedPids += $pId
            }
        }
    } catch {}
}

function Stop-StaleQtBridge {
    param([string]$OwnerStatePath)
    $QtBridgePort = 5911
    $killedPids = @(Stop-OwnedProcess -StatePath $OwnerStatePath -ScriptName "qt_camera_bridge.py" -Label "Qt camera bridge")
    try {
        $connections = Get-NetTCPConnection -LocalPort $QtBridgePort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $pId = [int]$conn.OwningProcess
            if ($pId -le 0 -or $pId -eq $PID -or $killedPids -contains $pId) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pId" -ErrorAction SilentlyContinue
            if ($proc -and ([string]$proc.CommandLine) -match "qt_camera_bridge\.py") {
                Write-Host "[Aurora] Terminating stale Qt camera bridge on port $QtBridgePort (PID $pId)"
                Stop-Process -Id $pId -Force -ErrorAction SilentlyContinue
                $killedPids += $pId
            }
        }
    } catch {}
    if ($killedPids.Count -gt 0) {
        $deadline = [DateTime]::UtcNow.AddSeconds(5)
        while ([DateTime]::UtcNow -lt $deadline) {
            $allGone = $true
            foreach ($kpid in $killedPids) {
                if (Get-Process -Id $kpid -ErrorAction SilentlyContinue) {
                    $allGone = $false
                    break
                }
            }
            if ($allGone) { break }
            Start-Sleep -Milliseconds 200
        }
        Start-Sleep -Milliseconds 500
    }
}

function Wait-PortReleased {
    param(
        [string]$BindHost,
        [int]$BindPort,
        [int]$TimeoutSeconds = 5
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-PortAvailable -BindHost $BindHost -BindPort $BindPort) {
            return $true
        }
        Start-Sleep -Milliseconds 200
    }
    return $false
}

Initialize-Utf8Console
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$Python = Get-PythonExecutable
$env:AURORA_PYTHON = $Python
$CompanionOwnerState = Join-Path $ScriptDir ".companion_owner.json"
$QtBridgeOwnerState = Join-Path $ScriptDir ".qt_bridge_owner.json"
Start-AuroraBootstrap -Disabled:$SkipAurora
Stop-StaleCompanionOnPort -BindPort $Port -OwnerStatePath $CompanionOwnerState
Stop-StaleQtBridge -OwnerStatePath $QtBridgeOwnerState
Wait-PortReleased -BindHost $ListenHost -BindPort $Port -TimeoutSeconds 5 | Out-Null
$ResolvedPort = Resolve-AvailablePort -BindHost $ListenHost -PreferredPort $Port
$browserUrl = "http://127.0.0.1:$ResolvedPort"

Start-BrowserWhenReady -ReadyPort $ResolvedPort -Url $browserUrl
try {
    Save-OwnerState -StatePath $CompanionOwnerState -OwnerPid $PID -OwnerPort $ResolvedPort -ScriptPath (Join-Path $ScriptDir "aurora_companion.py")
    & $Python aurora_companion.py --device $Device --port $ResolvedPort --host $ListenHost --source $Source
} finally {
    Clear-OwnerState -StatePath $CompanionOwnerState
}
