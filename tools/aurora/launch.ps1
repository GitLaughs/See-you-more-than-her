<# Aurora 伴侣工具启动脚本 (v2.1)
   功能: 一键启动 Aurora.exe + 伴侣工具 (点云/障碍/检测/底盘/烧录)

   用法:
     .\launch.ps1                    # 一键启动 Aurora + 伴侣工具
     .\launch.ps1 -NoAurora          # 仅启动伴侣工具
     .\launch.ps1 -Demo              # 演示模式（模拟数据）
     .\launch.ps1 -Flash             # 仅固件烧录
     .\launch.ps1 -TargetHost "10.0.0.50"  # 指定 A1 IP 地址
     .\launch.ps1 -Quick             # 快速模式：跳过依赖检查
#>

param(
    [switch]$NoAurora,
    [switch]$Demo,
    [switch]$Flash,
    [switch]$Quick,
    [string]$TargetHost = "",
    [int]$Port = 0,
    [string]$Firmware = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$AuroraExe = Join-Path $RepoRoot "Aurora-2.0.0-ciciec.13\Aurora.exe"
$CompanionScript = Join-Path $PSScriptRoot "aurora_companion.py"

# ──────────────────────────────────────────────
# 检查 Python 环境
# ──────────────────────────────────────────────
$PythonCmd = $null
foreach ($cmd in @("python3", "python", "py")) {
    try {
        $ver = & $cmd --version 2>&1
        if ($ver -match "Python 3\.\d+") {
            $PythonCmd = $cmd
            break
        }
    } catch {}
}

if (-not $PythonCmd) {
    Write-Host ""
    Write-Host "  [X] 未找到 Python 3，请先安装 Python 3.8+" -ForegroundColor Red
    Write-Host "      https://www.python.org/downloads/" -ForegroundColor DarkGray
    Write-Host ""
    exit 1
}

# ──────────────────────────────────────────────
# 启动横幅
# ──────────────────────────────────────────────
Write-Host ""
Write-Host "  ╔══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "  ║        Aurora 伴侣工具  v2.1                    ║" -ForegroundColor Cyan
Write-Host "  ║   A1 开发板 · 点云 · 检测 · 底盘 · 烧录        ║" -ForegroundColor Cyan
Write-Host "  ╚══════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Python : $PythonCmd ($((& $PythonCmd --version 2>&1)))" -ForegroundColor DarkGray
Write-Host "  仓库   : $RepoRoot" -ForegroundColor DarkGray
Write-Host ""

# ──────────────────────────────────────────────
# 安装依赖（-Quick 跳过）
# ──────────────────────────────────────────────
$RequirementsFile = Join-Path $PSScriptRoot "requirements.txt"
if (-not $Quick -and (Test-Path $RequirementsFile)) {
    Write-Host "  [1/3] 检查 Python 依赖..." -ForegroundColor Yellow
    & $PythonCmd -m pip install -q -r $RequirementsFile 2>&1 | Out-Null
    Write-Host "        依赖已就绪" -ForegroundColor Green
} else {
    Write-Host "  [1/3] 跳过依赖检查 (-Quick)" -ForegroundColor DarkGray
}

# ──────────────────────────────────────────────
# 启动 Aurora.exe（后台进程）
# ──────────────────────────────────────────────
if (-not $NoAurora -and -not $Flash) {
    if (Test-Path $AuroraExe) {
        Write-Host "  [2/3] 启动 Aurora.exe..." -ForegroundColor Yellow
        $auroraProc = Start-Process -FilePath $AuroraExe -PassThru -WindowStyle Normal
        Write-Host "        Aurora PID: $($auroraProc.Id)" -ForegroundColor Green
        Start-Sleep -Seconds 2
    } else {
        Write-Host "  [2/3] 未找到 Aurora.exe (仅启动伴侣工具)" -ForegroundColor DarkYellow
    }
} else {
    Write-Host "  [2/3] 跳过 Aurora.exe" -ForegroundColor DarkGray
}

# ──────────────────────────────────────────────
# 构建伴侣工具参数
# ──────────────────────────────────────────────
$companionArgs = @()

if ($Demo) {
    $companionArgs += "--demo"
}
if ($Flash) {
    $companionArgs += "--flash"
    if ($Firmware) {
        $companionArgs += $Firmware
    }
}
if ($TargetHost) {
    $companionArgs += "--host"
    $companionArgs += $TargetHost
}
if ($Port -gt 0) {
    $companionArgs += "--port"
    $companionArgs += $Port.ToString()
}

# ──────────────────────────────────────────────
# 启动伴侣工具
# ──────────────────────────────────────────────
Write-Host "  [3/3] 启动伴侣工具..." -ForegroundColor Yellow
if ($Demo) {
    Write-Host "        模式: 演示 (模拟数据)" -ForegroundColor Magenta
}
Write-Host ""

& $PythonCmd $CompanionScript @companionArgs

Write-Host ""
Write-Host "  Aurora 伴侣工具已退出" -ForegroundColor Cyan
Write-Host ""
