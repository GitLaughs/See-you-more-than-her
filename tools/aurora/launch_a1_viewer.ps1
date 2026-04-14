param(
    [int]$Device   = -1,
    [int]$Port     = 5802,
    [switch]$NoBrowser
)

$ErrorActionPreference = "Stop"

try { & chcp.com 65001 | Out-Null } catch {}
$Utf8 = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding  = $Utf8
[Console]::OutputEncoding = $Utf8
$OutputEncoding           = $Utf8
$env:PYTHONUTF8           = "1"
$env:PYTHONIOENCODING     = "utf-8"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

if (-not (Test-Path "a1_viewer.py")) {
    Write-Error "未找到 a1_viewer.py，请在 tools/aurora/ 目录运行本脚本。"
    exit 1
}

# 查找 Python（优先 .venv39）
$PythonCandidates = @(
    (Resolve-Path "..\..\\.venv39\Scripts\python.exe" -ErrorAction SilentlyContinue),
    (Resolve-Path "..\..\.venv39\Scripts\python.exe"  -ErrorAction SilentlyContinue),
    "python"
) | Where-Object { $_ }

$Python = $null
foreach ($c in $PythonCandidates) {
    try {
        $ver = & $c --version 2>&1
        if ($ver -match "Python 3") { $Python = $c; break }
    } catch {}
}
if (-not $Python) { $Python = "python" }
Write-Host "[INFO] Python: $Python"

$Args_ = @("a1_viewer.py", "--port", $Port)
if ($Device -ge 0) { $Args_ += "--device", $Device }

if (-not $NoBrowser) {
    $Url = "http://localhost:$Port"
    Start-Process $Url
    Write-Host "[INFO] 浏览器将打开: $Url"
}

Write-Host "[INFO] 启动 A1 Viewer (端口 $Port)..."
& $Python @Args_
