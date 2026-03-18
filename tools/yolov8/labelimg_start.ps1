$ErrorActionPreference = "Stop"

if (!(Test-Path ".venv39\Scripts\python.exe")) {
    Write-Error "Python 3.9 venv not found at .venv39. Run py -3.9 -m venv .venv39 first."
}

& .\.venv39\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install labelimg

# LabelImg outputs YOLO txt labels when configured in UI.
labelImg data/yolov8_dataset/raw/images data/yolov8_dataset/raw/labels
