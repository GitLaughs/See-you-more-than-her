<# Aurora 伴侣工具启动脚本
   功能: 启动 Aurora.exe (图像/OSD 显示) + 伴侣工具 (点云/障碍/检测/烧录)

   用法:
     .\launch.ps1                    # 启动 Aurora + 伴侣工具
     .\launch.ps1 -NoAurora          # 仅启动伴侣工具
     .\launch.ps1 -Demo              # 演示模式（模拟数据）
     .\launch.ps1 -Flash             # 仅固件烧录
     .\launch.ps1 -Host "10.0.0.50"  # 指定 A1 IP 地址
#>

param(
    [switch]$NoAurora,
    [switch]$Demo,
    [switch]$Flash,
    [string]$Host = "",
    [int]$Port = 0,
    [string]$Firmware = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$AuroraExe = Join-Path $RepoRoot "Aurora-2.0.0-ciciec.13\Aurora.exe"
$CompanionScript = Join-Path $PSScriptRoot "aurora_companion.py"

# 检查 Python 环境
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
    Write-Host "[错误] 未找到 Python 3，请先安装 Python 3.8+" -ForegroundColor Red
    exit 1
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Aurora 伴侣工具启动器" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Python: $PythonCmd ($((& $PythonCmd --version 2>&1)))"
Write-Host "  仓库: $RepoRoot"
Write-Host ""

# 检查依赖
$RequirementsFile = Join-Path $PSScriptRoot "requirements.txt"
if (Test-Path $RequirementsFile) {
    Write-Host "[准备] 检查 Python 依赖..." -ForegroundColor Yellow
    & $PythonCmd -m pip install -q -r $RequirementsFile 2>&1 | Out-Null
    Write-Host "  依赖已就绪" -ForegroundColor Green
}

# 启动 Aurora.exe（后台进程）
if (-not $NoAurora -and -not $Flash) {
    if (Test-Path $AuroraExe) {
        Write-Host "[启动] Aurora.exe..." -ForegroundColor Yellow
        $auroraProc = Start-Process -FilePath $AuroraExe -PassThru -WindowStyle Normal
        Write-Host "  Aurora PID: $($auroraProc.Id)" -ForegroundColor Green
        Start-Sleep -Seconds 2
    } else {
        Write-Host "[提示] 未找到 Aurora.exe: $AuroraExe" -ForegroundColor DarkYellow
        Write-Host "  将仅启动伴侣工具" -ForegroundColor DarkYellow
    }
}

# 构建伴侣工具参数
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
if ($Host) {
    $companionArgs += "--host"
    $companionArgs += $Host
}
if ($Port -gt 0) {
    $companionArgs += "--port"
    $companionArgs += $Port.ToString()
}

# 启动伴侣工具
Write-Host "[启动] Aurora 伴侣工具..." -ForegroundColor Yellow
& $PythonCmd $CompanionScript @companionArgs

Write-Host ""
Write-Host "[完成] Aurora 伴侣工具已退出" -ForegroundColor Cyan
