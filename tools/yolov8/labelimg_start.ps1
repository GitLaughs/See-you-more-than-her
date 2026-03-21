param(
    [string]$ProxyUrl = "",
    [switch]$UseTunaSource,
    [switch]$SkipInstall
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $RepoRoot

if ($ProxyUrl -ne "") {
    $env:HTTP_PROXY = $ProxyUrl
    $env:HTTPS_PROXY = $ProxyUrl
    $env:http_proxy = $ProxyUrl
    $env:https_proxy = $ProxyUrl
}

if (!(Test-Path ".venv39\Scripts\python.exe")) {
    Write-Error "Python 3.9 venv not found at .venv39. Run py -3.9 -m venv .venv39 first."
}

& .\.venv39\Scripts\Activate.ps1

$pipArgs = @()
if ($UseTunaSource) {
    $pipArgs = @("-i", "https://pypi.tuna.tsinghua.edu.cn/simple", "--trusted-host", "pypi.tuna.tsinghua.edu.cn")
}

if (-not $SkipInstall) {
    python -m pip install --upgrade pip @pipArgs
    python -m pip install labelimg @pipArgs
}

if (!(Test-Path "data/yolov8_dataset/raw/images")) {
    Write-Error "Missing directory: data/yolov8_dataset/raw/images"
}

if (!(Test-Path "data/yolov8_dataset/raw/labels")) {
    New-Item -ItemType Directory -Path "data/yolov8_dataset/raw/labels" | Out-Null
}

# LabelImg outputs YOLO txt labels when configured in UI.
.\.venv39\Scripts\labelImg.exe data/yolov8_dataset/raw/images data/yolov8_dataset/raw/labels
