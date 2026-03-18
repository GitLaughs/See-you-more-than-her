param(
    [string]$Model = "yolov8n.pt",
    [int]$Epochs = 100,
    [int]$Batch = 16,
    [int]$ImgSize = 640,
    [string]$Project = "runs/train",
    [string]$Name = "a1_yolov8"
)

$ErrorActionPreference = "Stop"

if (!(Test-Path ".venv39\Scripts\python.exe")) {
    Write-Error "Python 3.9 venv not found at .venv39. Run py -3.9 -m venv .venv39 first."
}

& .\.venv39\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
pip install ultralytics labelimg

# device=0 forces GPU 0. Change to device=cpu if GPU is unavailable.
yolo detect train `
    model=$Model `
    data=data/yolov8_dataset/dataset.yaml `
    epochs=$Epochs `
    imgsz=$ImgSize `
    batch=$Batch `
    device=0 `
    workers=8 `
    cache=True `
    amp=True `
    project=$Project `
    name=$Name
