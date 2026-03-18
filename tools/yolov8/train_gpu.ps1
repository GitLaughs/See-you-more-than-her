param(
    [string]$Model = "yolov8n.pt",
    [int]$Epochs = 100,
    [int]$Batch = 16,
    [int]$ImgSize = 640,
    [string]$Project = "runs/train",
    [string]$Name = "a1_yolov8",
    [string]$Device = "0",
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
    python -m pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
    python -m pip install ultralytics labelimg @pipArgs
}

if (!(Test-Path "data/yolov8_dataset/dataset.yaml")) {
    Write-Error "Missing dataset config: data/yolov8_dataset/dataset.yaml"
}

# device=0 forces GPU 0. Change to device=cpu if GPU is unavailable.
.\.venv39\Scripts\yolo.exe detect train `
    model=$Model `
    data=data/yolov8_dataset/dataset.yaml `
    epochs=$Epochs `
    imgsz=$ImgSize `
    batch=$Batch `
    device=$Device `
    workers=8 `
    cache=True `
    amp=True `
    project=$Project `
    name=$Name
