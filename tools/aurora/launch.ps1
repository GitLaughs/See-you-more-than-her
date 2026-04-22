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
        $browserWorker = @'
$deadline = [DateTime]::UtcNow.AddSeconds(180)
$statusUrl = "__URL__status"
$baseUrl = "__URL__"
while ([DateTime]::UtcNow -lt $deadline) {
    try {
        # Fast TCP check
        $client = New-Object System.Net.Sockets.TcpClient
        $async = $client.BeginConnect("127.0.0.1", __PORT__, $null, $null)
        $tcpOk = $async.AsyncWaitHandle.WaitOne(200)
        
        if ($tcpOk) {
            $client.EndConnect($async)
            $client.Close()
            # HTTP check
            $response = Invoke-WebRequest -Uri $baseUrl -UseBasicParsing -TimeoutSec 1 -ErrorAction SilentlyContinue
            if ($response.StatusCode -ge 200) {
                Start-Process $baseUrl | Out-Null
                exit 0
            }
        } else {
            $client.Close()
        }
    }
    catch {
    }
    Start-Sleep -Milliseconds 500
}
'@
        $browserWorker = $browserWorker.Replace("__PORT__", [string]$ReadyPort).Replace("__URL__", $Url)
        if (-not $browserWorker.Contains("__URL__status")) {
            # Ensure proper URL formatting
            if (-not $Url.EndsWith("/")) {
                $browserWorker = $browserWorker.Replace("__URL__status", "$Url/status")
            }
        }
        $encodedCommand = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($browserWorker))
        $shell = Get-Command pwsh -ErrorAction SilentlyContinue
        if (-not $shell) {
            $shell = Get-Command powershell -ErrorAction SilentlyContinue
        }

        if ($shell) {
            Start-Process -FilePath $shell.Source -WindowStyle Hidden -ArgumentList @("-NoProfile", "-EncodedCommand", $encodedCommand) | Out-Null
            return
        }

        throw "No PowerShell host found"
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
        $errFile = [System.IO.Path]::GetTempFileName()
        try {
            # Route stderr to file
            $proc = Start-Process -FilePath $Python -ArgumentList $Arguments -NoNewWindow -Wait -RedirectStandardError $errFile -PassThru
            if ($proc.ExitCode -ne 0) {
                Write-Host "`n[ERROR] Python script exited with code $($proc.ExitCode). Error output:`n" -ForegroundColor Red
                Get-Content $errFile | Out-String | Write-Host -ForegroundColor Red
            }
        } finally {
            if (Test-Path $errFile) { Remove-Item $errFile -Force }
        }
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

if (-not $PSBoundParameters.ContainsKey("Mode")) {
    Write-Host "=== Aurora Launch === " -ForegroundColor Cyan
    Write-Host "1. Aurora Companion (camera + chassis debug)"
    Write-Host "2. A1 Companion (OSD + chassis + camera)"
    Write-Host "3. A1 Viewer (board-side OSD preview)"
    Write-Host "4. STM32 Port Probe"
    Write-Host "5. Aurora Capture (image preview + dataset export)"
    Write-Host ""
    $choice = Read-Host "Select execution mode [1]"
    switch ($choice) {
        "2" { $Mode = "a1" }
        "3" { $Mode = "viewer" }
        "4" { $Mode = "probe" }
        "5" { $Mode = "capture" }
        default { $Mode = "companion" }
    }
    $resolvedPort = if ($Port -gt 0) { $Port } else { Get-DefaultPort -LaunchMode $Mode }
    $browserUrl = "http://127.0.0.1:$resolvedPort"
}

switch ($Mode) {
    "companion" {
        $launchArgs = @("aurora_companion.py", "--device", $Device, "--port", $resolvedPort)
        if ($Output -ne "") {
            $launchArgs += @("--output", $Output)
        }

        Write-Host "=== Aurora Companion ===" -ForegroundColor Cyan
        Write-Host "Starting Aurora Companion (camera + chassis debug)..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        if ($NoBrowser) {
            Write-Host "Browser: auto-open disabled" -ForegroundColor Yellow
        }
        else {
            Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        }
        if ($Device -eq -1) {
            Write-Host "Camera device: auto (reuse last successful device/source, fallback to scan)" -ForegroundColor Yellow
        }
        else {
            Write-Host "Camera device: $Device" -ForegroundColor Yellow
        }
        Write-Host "Capture pipeline: actual acquisition 1280x720; optional training crop 640x360" -ForegroundColor Yellow
        Write-Host "UI: 左侧预览工作台 / 电脑直连 / 经由 A1" -ForegroundColor Yellow
        if (-not $ShowDriverLogs) {
            Write-Host "Driver logs: hidden (use -ShowDriverLogs to enable)" -ForegroundColor Yellow
        }
        Write-Host "Model: best_a1_formal.onnx (switchable in the page)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        $hideStderr = -not $ShowDriverLogs
        Invoke-PythonTool -Python $Python -Arguments $launchArgs -SuppressStderr:$hideStderr
        break
    }
    "a1" {
        $launchArgs = @("a1_companion.py", "--port", $resolvedPort)
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
        Invoke-PythonTool -Python $Python -Arguments $launchArgs -SuppressStderr:$hideStderr
        break
    }
    "viewer" {
        $launchArgs = @("a1_viewer.py", "--port", $resolvedPort)
        if ($Device -ge 0) {
            $launchArgs += @("--device", $Device)
        }

        Write-Host "=== A1 Viewer ===" -ForegroundColor Cyan
        Write-Host "Starting A1 Viewer (board-side OSD preview)..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        Invoke-PythonTool -Python $Python -Arguments $launchArgs
        break
    }
    "probe" {
        $launchArgs = @("stm32_port_probe.py", "--port", $resolvedPort)
        Write-Host "=== STM32 Port Probe ===" -ForegroundColor Cyan
        Write-Host "Starting COM port probe page..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        Invoke-PythonTool -Python $Python -Arguments $launchArgs
        break
    }
    "capture" {
        $launchArgs = @("aurora_capture.py", "--device", $Device, "--port", $resolvedPort)
        if ($Output -ne "") {
            $launchArgs += @("--output", $Output)
        }

        Write-Host "=== Aurora Capture ===" -ForegroundColor Cyan
        Write-Host "Starting Aurora Capture (image preview + dataset export)..." -ForegroundColor Green
        Write-Host "Web UI: $browserUrl" -ForegroundColor Yellow
        Write-Host "Browser: auto-open enabled (use -NoBrowser to disable)" -ForegroundColor Yellow
        Write-Host ""

        if (-not $NoBrowser) {
            Start-BrowserWhenReady -ReadyPort $resolvedPort -Url $browserUrl
        }

        Invoke-PythonTool -Python $Python -Arguments $launchArgs -SuppressStderr
        break
    }
}
