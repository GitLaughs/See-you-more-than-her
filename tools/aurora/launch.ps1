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

function Start-AuroraDesktop {
    $auroraExe = Resolve-Path "..\..\Aurora-2.0.0-ciciec.16\Aurora.exe" -ErrorAction SilentlyContinue
    if (-not $auroraExe) {
        return
    }
    $alreadyRunning = Get-Process -Name "Aurora" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($alreadyRunning) {
        return
    }
    $auroraDir = Split-Path -Parent $auroraExe.Path
    Start-Process -FilePath $auroraExe.Path -WorkingDirectory $auroraDir | Out-Null
    Start-Sleep -Milliseconds 1200
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
    throw "无法找到可用端口（起始端口: $PreferredPort）"
}

function Stop-StaleCompanionOnPort {
    param(
        [int]$BindPort
    )
    try {
        $connections = Get-NetTCPConnection -LocalPort $BindPort -State Listen -ErrorAction SilentlyContinue
        foreach ($conn in $connections) {
            $pid = [int]$conn.OwningProcess
            if ($pid -le 0 -or $pid -eq $PID) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pid" -ErrorAction SilentlyContinue
            if (-not $proc) { continue }
            $cmd = [string]$proc.CommandLine
            if ($cmd -match "aurora_companion\.py") {
                Write-Host "[Aurora] 释放旧 Companion 端口 $BindPort (PID $pid)"
                Stop-Process -Id $pid -Force -ErrorAction SilentlyContinue
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
            $pid = [int]$conn.OwningProcess
            if ($pid -le 0 -or $pid -eq $PID) { continue }
            $proc = Get-CimInstance Win32_Process -Filter "ProcessId=$pid" -ErrorAction SilentlyContinue
            if (-not $proc) { continue }
            $cmd = [string]$proc.CommandLine
            if ($cmd -match "qt_camera_bridge\.py") {
                Write-Host "[Aurora] 终止旧 Qt 相机桥 (PID $pid)"
                Stop-Process -Id $pid -Force -ErrorAction SilentlyContinue
                $killedPids += $pid
            }
        }
    } catch {}
    try {
        $bridgeProcs = Get-CimInstance Win32_Process -Filter "Name='python.exe' OR Name='pythonw.exe'" -ErrorAction SilentlyContinue
        foreach ($proc in $bridgeProcs) {
            $pid = [int]$proc.ProcessId
            if ($pid -le 0 -or $pid -eq $PID -or $killedPids -contains $pid) { continue }
            $cmd = [string]$proc.CommandLine
            if ($cmd -match "qt_camera_bridge\.py") {
                Write-Host "[Aurora] 终止旧 Qt 相机桥进程 (PID $pid)"
                Stop-Process -Id $pid -Force -ErrorAction SilentlyContinue
                $killedPids += $pid
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

if (-not $SkipAurora) {
    Start-AuroraDesktop
}

Start-BrowserWhenReady -ReadyPort $ResolvedPort -Url $browserUrl
& $Python aurora_companion.py --device $Device --port $ResolvedPort --host $ListenHost --source $Source
