param(
    [int]$Device = -1,
    [int]$Port = 5801,
    [string]$Source = "auto",
    [string]$ListenHost = "127.0.0.1"
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
}

function Get-PythonExecutable {
    $candidates = @(
        (Join-Path $ScriptDir "..\..\venv_39\Scripts\python.exe"),
        (Join-Path $ScriptDir "..\..\.venv39\Scripts\python.exe"),
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
        [int]$BindPort
    )
    try {
        $connections = Get-NetTCPConnection -LocalPort $BindPort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $pId = [int]$conn.OwningProcess
            if ($pId -le 0 -or $pId -eq $PID) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pId" -ErrorAction SilentlyContinue
            if ($proc -and ([string]$proc.CommandLine) -match "aurora_companion\.py") {
                Write-Host "[Aurora] Releasing stale Companion port $BindPort (PID $pId)"
                Stop-Process -Id $pId -Force -ErrorAction SilentlyContinue
            }
        }
    } catch {}
}

function Stop-StaleQtBridge {
    $QtBridgePort = 5911
    $killedPids = @()
    try {
        $connections = Get-NetTCPConnection -LocalPort $QtBridgePort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $pId = [int]$conn.OwningProcess
            if ($pId -le 0 -or $pId -eq $PID) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pId" -ErrorAction SilentlyContinue
            if ($proc -and ([string]$proc.CommandLine) -match "qt_camera_bridge\.py") {
                Write-Host "[Aurora] Terminating stale Qt camera bridge (PID $pId)"
                Stop-Process -Id $pId -Force -ErrorAction SilentlyContinue
                $killedPids += $pId
            }
        }
    } catch {}
    try {
        $bridgeProcs = Get-CimInstance Win32_Process -Filter "Name='python.exe' OR Name='pythonw.exe'" -ErrorAction SilentlyContinue
        foreach ($proc in $bridgeProcs) {
            $pId = [int]$proc.ProcessId
            if ($pId -le 0 -or $pId -eq $PID -or $killedPids -contains $pId) { continue }
            if (([string]$proc.CommandLine) -match "qt_camera_bridge\.py") {
                Write-Host "[Aurora] Terminating stale Qt camera bridge process (PID $pId)"
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
Stop-StaleCompanionOnPort -BindPort $Port
Stop-StaleQtBridge
Wait-PortReleased -BindHost $ListenHost -BindPort $Port -TimeoutSeconds 5 | Out-Null
$ResolvedPort = Resolve-AvailablePort -BindHost $ListenHost -PreferredPort $Port
$browserUrl = "http://127.0.0.1:$ResolvedPort"

Start-BrowserWhenReady -ReadyPort $ResolvedPort -Url $browserUrl
& $Python aurora_companion.py --device $Device --port $ResolvedPort --host $ListenHost --source $Source
