param(
    [int]$Device = -1,
    [string]$Output = "",
    [int]$Port = 5000
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Set-Location $ScriptDir

if (-not (Test-Path "aurora_capture.py")) {
    Write-Error "aurora_capture.py not found in $ScriptDir"
}

$captureArgs = @("aurora_capture.py", "--device", $Device, "--port", $Port)
if ($Output -ne "") {
    $captureArgs += @("--output", $Output)
}

Write-Host "=== Aurora Capture Tool ===" -ForegroundColor Cyan
Write-Host "Starting camera capture tool..." -ForegroundColor Green
Write-Host "Web UI: http://localhost:$Port" -ForegroundColor Yellow
Write-Host "Camera device: $Device (-1 means auto-select A1)" -ForegroundColor Yellow
Write-Host ""

python @captureArgs
