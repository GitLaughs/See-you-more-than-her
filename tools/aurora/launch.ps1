param(
    [int]$Device = 0,
    [string]$Output = "",
    [int]$Port = 5000,
    [string]$Flash = "",
    [ValidateSet("auto", "docker", "aurora")]
    [string]$Mode = "auto",
    [int]$FlashPort = 5055
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Set-Location $ScriptDir

if ($Flash -ne "") {
    if (-not (Test-Path "aurora_flash_web.py")) {
        Write-Error "aurora_flash_web.py not found in $ScriptDir"
    }

    $flashArgs = @("aurora_flash_web.py", "--port", $FlashPort, "--mode", $Mode)
    if ($Flash -ne "latest") {
        $flashArgs += @("--firmware", $Flash)
    }

    Write-Host "=== Aurora Flash Web Tool ===" -ForegroundColor Cyan
    Write-Host "Starting flash web UI..." -ForegroundColor Green
    Write-Host "Web UI: http://localhost:$FlashPort" -ForegroundColor Yellow
    Write-Host "Mode: $Mode" -ForegroundColor Yellow
    Write-Host ""

    python @flashArgs
    exit $LASTEXITCODE
}

if (-not (Test-Path "aurora_capture.py")) {
    Write-Error "aurora_capture.py not found in $ScriptDir"
}

$args = @("aurora_capture.py", "--device", $Device, "--port", $Port)
if ($Output -ne "") {
    $args += @("--output", $Output)
}

Write-Host "=== Aurora Capture Tool ===" -ForegroundColor Cyan
Write-Host "Starting camera capture tool..." -ForegroundColor Green
Write-Host "Web UI: http://localhost:$Port" -ForegroundColor Yellow
Write-Host ""

python @args
