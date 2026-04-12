param(
    [int]$Device = -1,
    [string]$Output = "",
    [int]$Port = 5001,
    [switch]$ShowDriverLogs,
    [switch]$NoBrowser
)

$ErrorActionPreference = "Stop"

# 强制 UTF-8，避免中文输出在不同终端编码下乱码
try {
    & chcp.com 65001 | Out-Null
}
catch {
}
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $Utf8NoBom
[Console]::OutputEncoding = $Utf8NoBom
$OutputEncoding = $Utf8NoBom

# 确保 Python 子进程也按 UTF-8 输出
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Set-Location $ScriptDir

if (-not (Test-Path "aurora_companion.py")) {
    Write-Error "aurora_companion.py not found in $ScriptDir"
}

$captureArgs = @("aurora_companion.py", "--device", $Device, "--port", $Port)
if ($Output -ne "") {
    $captureArgs += @("--output", $Output)
}

Write-Host "=== Aurora Companion ===" -ForegroundColor Cyan
Write-Host "Starting Aurora Companion (camera + chassis debug)..." -ForegroundColor Green
Write-Host "Web UI: http://localhost:$Port" -ForegroundColor Yellow
if ($Device -eq -1) {
    Write-Host "Camera device: auto (prefer A1 SC132GS)" -ForegroundColor Yellow
}
else {
    Write-Host "Camera device: $Device" -ForegroundColor Yellow
}
Write-Host "Capture pipeline: sensor 1280x720 -> training 640x360" -ForegroundColor Yellow
Write-Host "Tabs: 摄像头采集 / A1-STM32 联通测试 / 底盘通信调试" -ForegroundColor Yellow
if (-not $ShowDriverLogs) {
    Write-Host "Driver logs: hidden (use -ShowDriverLogs to enable)" -ForegroundColor Yellow
}
Write-Host ""

if (-not $NoBrowser) {
    try {
        Start-Process "http://localhost:$Port" | Out-Null
    }
    catch {
        Write-Host "[WARN] Unable to open browser automatically. Please open http://localhost:$Port manually." -ForegroundColor DarkYellow
    }
}

if ($ShowDriverLogs) {
    python @captureArgs
}
else {
    # 摄像头驱动会持续向 stderr 打印噪声日志，默认隐藏以免淹没启动信息
    python @captureArgs 2>$null
}
