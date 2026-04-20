param(
    [ValidateSet("companion", "a1", "viewer", "probe", "capture")]
    [string]$Mode = "companion",
    [int]$Device = -1,
    [string]$Output = "",
    [int]$Port = 0,
    [switch]$ShowDriverLogs,
    [switch]$NoBrowser
)

$ErrorActionPreference = "Stop"

function Initialize-Utf8Console {
    try {
        & chcp.com 65001 | Out-Null
    }
    catch {
    }

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [Console]::InputEncoding = $utf8NoBom
    [Console]::OutputEncoding = $utf8NoBom
    $global:OutputEncoding = $utf8NoBom
    $env:PYTHONUTF8 = "1"
    $env:PYTHONIOENCODING = "utf-8"
}

function Get-PythonExecutable {
    $candidates = @(
        (Resolve-Path "..\..\\.venv39\Scripts\python.exe" -ErrorAction SilentlyContinue),
        (Resolve-Path "..\..\.venv39\Scripts\python.exe" -ErrorAction SilentlyContinue),
        "python"
    ) | Where-Object { $_ }

    foreach ($candidate in $candidates) {
        try {
            $version = & $candidate --version 2>&1
            if ($version -match "Python 3") {
                return $candidate
            }
        }
        catch {
        }
    }

    return "python"
}

function Get-DefaultPort {
    param([string]$LaunchMode)

    switch ($LaunchMode) {
        "companion" { return 5801 }
        "a1" { return 5803 }
        "viewer" { return 5802 }
        "probe" { return 5006 }
        "capture" { return 5000 }
        default { return 5801 }
    }
}

function Start-BrowserWhenReady {
    param(
        [int]$ReadyPort,
        [string]$Url
    )

    try {
        Start-Job -ScriptBlock {
            param([string]$TargetUrl, [int]$TargetPort)

            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            while ([DateTime]::UtcNow -lt $deadline) {
                try {
                    $client = New-Object System.Net.Sockets.TcpClient
                    $async = $client.BeginConnect("127.0.0.1", $TargetPort, $null, $null)
                    if ($async.AsyncWaitHandle.WaitOne(250)) {
                        $client.EndConnect($async)
                        $client.Close()
                        Start-Process $TargetUrl | Out-Null
                        return
                    }
                    $client.Close()
                }
                catch {
                }
                [System.Threading.Thread]::Sleep(250)
            }

            try {
                Start-Process $TargetUrl | Out-Null
            }
            catch {
            }
        } -ArgumentList $Url, $ReadyPort | Out-Null
    }
    catch {
        Write-Host "[WARN] Unable to open browser automatically. Please open $Url manually." -ForegroundColor DarkYellow
    }
}

function Invoke-PythonTool {
    param(
        [string]$Python,
        [string[]]$Arguments,
        [switch]$SuppressStderr
    )

    if ($SuppressStderr) {
        & $Python @Arguments 2>$null
    }
    else {
        & $Python @Arguments
    }
}

Initialize-Utf8Console

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

if (-not (Test-Path "aurora_companion.py")) {
    Write-Error "aurora_companion.py not found in $ScriptDir"
}

$Python = Get-PythonExecutable
Write-Host "[INFO] Python: $Python" -ForegroundColor DarkGray

$resolvedPort = if ($Port -gt 0) { $Port } else { Get-DefaultPort -LaunchMode $Mode }
$browserUrl = "http://127.0.0.1:$resolvedPort"

switch ($Mode) {
    "companion" {
        $args = @("aurora_companion.py", "--device", $Device, "--port", $resolvedPort)
        if ($Output -ne "") {
            $args += @("--output", $Output)
        }

        Write-Host "=== Aurora Companion ===" -ForegroundColor Cyan
        Write-Host "Starting Aurora Companion (camera + chassis debug)..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        if ($Device -eq -1) {
            Write-Host "Camera device: auto (prefer A1 SC132GS)" -ForegroundColor Yellow
        }
        else {
            Write-Host "Camera device: $Device" -ForegroundColor Yellow
        }
        Write-Host "Capture pipeline: sensor 1280x720 -> training 640x360" -ForegroundColor Yellow
        $tabCamera = -join ([char[]](0x6444, 0x50CF, 0x5934, 0x91C7, 0x96C6))
        $tabLink = -join ([char[]](0x8054, 0x901A, 0x6D4B, 0x8BD5))
        $tabChassis = -join ([char[]](0x5E95, 0x76D8, 0x901A, 0x4FE1, 0x8C03, 0x8BD5))
        Write-Host "Tabs: $tabCamera / A1-STM32 $tabLink / $tabChassis" -ForegroundColor Yellow
        if (-not $ShowDriverLogs) {
            Write-Host "Driver logs: hidden (use -ShowDriverLogs to enable)" -ForegroundColor Yellow
        }
        Write-Host "Model: best_a1_formal.onnx (switchable in the page)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        $hideStderr = -not $ShowDriverLogs
        Invoke-PythonTool -Python $Python -Arguments $args -SuppressStderr:$hideStderr
        break
    }
    "a1" {
        $args = @("a1_companion.py", "--port", $resolvedPort)
        Write-Host "=== A1 Companion ===" -ForegroundColor Cyan
        Write-Host "Starting A1 Companion (camera + OSD + chassis debug)..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        Write-Host "Model: best_a1_formal_head6.onnx (switchable in the page)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        $hideStderr = -not $ShowDriverLogs
        Invoke-PythonTool -Python $Python -Arguments $args -SuppressStderr:$hideStderr
        break
    }
    "viewer" {
        $args = @("a1_viewer.py", "--port", $resolvedPort)
        if ($Device -ge 0) {
            $args += @("--device", $Device)
        }

        Write-Host "=== A1 Viewer ===" -ForegroundColor Cyan
        Write-Host "Starting A1 Viewer (board-side OSD preview)..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        Invoke-PythonTool -Python $Python -Arguments $args
        break
    }
    "probe" {
        $args = @("stm32_port_probe.py", "--port", $resolvedPort)
        Write-Host "=== STM32 Port Probe ===" -ForegroundColor Cyan
        Write-Host "Starting COM port probe page..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        Invoke-PythonTool -Python $Python -Arguments $args
        break
    }
    "capture" {
        $args = @("aurora_capture.py", "--device", $Device, "--port", $resolvedPort)
        if ($Output -ne "") {
            $args += @("--output", $Output)
        }

        Write-Host "=== Aurora Capture ===" -ForegroundColor Cyan
        Write-Host "Starting Aurora Capture (image preview + dataset export)..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        Invoke-PythonTool -Python $Python -Arguments $args -SuppressStderr
        break
    }
}