param(
    [int]$Device = -1,
    [int]$Port = 6201,
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

function Get-QtBridgeOwnerStatePath {
    return Join-Path $ScriptDir ".qt_bridge_owner.json"
}

function Get-RepoVenvRoot {
    return [System.IO.Path]::GetFullPath((Join-Path $ScriptDir "..\..\venv_39"))
}

function Get-VenvBasePython {
    param([string]$VenvRoot)

    $cfgPath = Join-Path $VenvRoot "pyvenv.cfg"
    if (-not (Test-Path $cfgPath)) {
        return $null
    }

    try {
        $homeLine = Get-Content -Path $cfgPath | Where-Object { $_ -match '^\s*home\s*=' } | Select-Object -First 1
        if (-not $homeLine) {
            return $null
        }
        $baseHome = ($homeLine -split '=', 2)[1].Trim()
        if (-not $baseHome) {
            return $null
        }
        $basePython = Join-Path $baseHome "python.exe"
        if (Test-Path $basePython) {
            return [System.IO.Path]::GetFullPath($basePython)
        }
    } catch {}

    return $null
}

function Set-AuroraPythonEnvironment {
    param([string]$PythonExecutable)

    $venvRoot = Get-RepoVenvRoot
    $cfgPath = Join-Path $venvRoot "pyvenv.cfg"
    if (-not (Test-Path $cfgPath)) {
        return
    }

    $sitePackages = Join-Path $venvRoot "Lib\site-packages"
    $scriptsDir = Join-Path $venvRoot "Scripts"
    $pythonDir = Split-Path $PythonExecutable -Parent

    $env:VIRTUAL_ENV = $venvRoot
    if (Test-Path $sitePackages) {
        if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
            $env:PYTHONPATH = [System.IO.Path]::GetFullPath($sitePackages)
        } else {
            $env:PYTHONPATH = ([System.IO.Path]::GetFullPath($sitePackages)) + ";" + $env:PYTHONPATH
        }
    }

    $prependPath = @()
    if (Test-Path $scriptsDir) {
        $prependPath += [System.IO.Path]::GetFullPath($scriptsDir)
    }
    if ($pythonDir) {
        $prependPath += $pythonDir
    }
    if ($prependPath.Count -gt 0) {
        $env:PATH = ($prependPath -join ";") + ";" + $env:PATH
    }
}

function Get-PythonExecutable {
    $venvRoot = Get-RepoVenvRoot
    $venvPython = Join-Path $venvRoot "Scripts\python.exe"
    $basePython = Get-VenvBasePython -VenvRoot $venvRoot
    $candidates = @()
    if ($basePython) {
        $candidates += $basePython
    }
    if (Test-Path $venvPython) {
        $candidates += [System.IO.Path]::GetFullPath($venvPython)
    }
    $candidates += "python"

    foreach ($candidate in $candidates) {
        try {
            $version = & $candidate --version 2>&1
            if ($version -match "Python 3") {
                return $candidate
            }
        } catch {}
    }
    return "python"
}

function Stop-OwnedQtBridge {
    $statePath = Get-QtBridgeOwnerStatePath
    if (-not (Test-Path $statePath)) {
        return @()
    }

    try {
        $state = Get-Content -Raw -Path $statePath | ConvertFrom-Json
    } catch {
        Remove-Item $statePath -Force -ErrorAction SilentlyContinue
        return @()
    }

    $ownerPid = 0
    try { $ownerPid = [int]$state.pid } catch { $ownerPid = 0 }
    if ($ownerPid -le 0 -or $ownerPid -eq $PID) {
        Remove-Item $statePath -Force -ErrorAction SilentlyContinue
        return @()
    }

    $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$ownerPid" -ErrorAction SilentlyContinue
    if ($proc -and ([string]$proc.CommandLine) -match "qt_camera_bridge\.py") {
        Write-Host "[Aurora] Terminating owned Qt camera bridge (PID $ownerPid)"
        Stop-Process -Id $ownerPid -Force -ErrorAction SilentlyContinue
        Remove-Item $statePath -Force -ErrorAction SilentlyContinue
        return @($ownerPid)
    }

    Remove-Item $statePath -Force -ErrorAction SilentlyContinue
    return @()
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

function Resolve-FixedPort {
    param(
        [string]$BindHost,
        [int]$PreferredPort
    )
    if (Test-PortAvailable -BindHost $BindHost -BindPort $PreferredPort) {
        return $PreferredPort
    }
    throw "[Aurora] Fixed port $PreferredPort is not available. Free it or choose another explicit -Port value."
}

function Stop-StaleCompanionOnPort {
    param(
        [int]$BindPort
    )
    try {
        $connections = Get-NetTCPConnection -LocalPort $BindPort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $ownerPid = [int]$conn.OwningProcess
            if ($ownerPid -le 0 -or $ownerPid -eq $PID) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$ownerPid" -ErrorAction SilentlyContinue
            if ($proc -and ([string]$proc.CommandLine) -match "aurora_companion\.py") {
                Write-Host "[Aurora] Releasing stale Companion port $BindPort (PID $ownerPid)"
                Stop-Process -Id $ownerPid -Force -ErrorAction SilentlyContinue
            }
        }
    } catch {}
}

function Stop-StaleQtBridge {
    $QtBridgePort = 5911
    $killedPids = @(Stop-OwnedQtBridge)
    try {
        $connections = Get-NetTCPConnection -LocalPort $QtBridgePort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $pId = [int]$conn.OwningProcess
            if ($pId -le 0 -or $pId -eq $PID -or $killedPids -contains $pId) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pId" -ErrorAction SilentlyContinue
            if ($proc -and ([string]$proc.CommandLine) -match "qt_camera_bridge\.py") {
                Write-Host "[Aurora] Terminating stale Qt camera bridge (PID $pId)"
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
Set-AuroraPythonEnvironment -PythonExecutable $Python
$env:AURORA_PYTHON = $Python
Start-AuroraBootstrap -Disabled:$SkipAurora
Stop-StaleCompanionOnPort -BindPort $Port
Stop-StaleQtBridge
Wait-PortReleased -BindHost $ListenHost -BindPort $Port -TimeoutSeconds 5 | Out-Null
$ResolvedPort = Resolve-FixedPort -BindHost $ListenHost -PreferredPort $Port
$browserUrl = "http://127.0.0.1:$ResolvedPort"

Start-BrowserWhenReady -ReadyPort $ResolvedPort -Url $browserUrl
& $env:AURORA_PYTHON aurora_companion.py --device $Device --port $ResolvedPort --host $ListenHost --source $Source
