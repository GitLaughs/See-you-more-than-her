# YOLOv8 Training and Annotation Guide (GPU)

## 1. Clone Official YOLOv8 Repository

```powershell
git clone https://github.com/ultralytics/ultralytics.git third_party/ultralytics
```

## 2. Create Python 3.9 Virtual Environment

```powershell
py -3.9 -m venv .venv39
.\.venv39\Scripts\Activate.ps1
python -m pip install --upgrade pip
```

## 3. Install GPU Training and Annotation Dependencies

```powershell
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
pip install ultralytics labelimg
```

Check GPU visibility:

```powershell
python -c "import torch; print(torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'NO_GPU')"
```

## 4. Annotate Data with LabelImg

```powershell
.\tools\yolov8\labelimg_start.ps1
```

Expected paths:

- images: `data/yolov8_dataset/raw/images`
- labels: `data/yolov8_dataset/raw/labels`

## 5. Split Dataset

```powershell
.\.venv39\Scripts\python.exe .\tools\yolov8\split_dataset.py --dataset-root data/yolov8_dataset --train 0.8 --val 0.1 --seed 42
```

## 6. Train with GPU

```powershell
.\tools\yolov8\train_gpu.ps1 -Model yolov8n.pt -Epochs 100 -Batch 16 -ImgSize 640 -Name a1_yolov8
```

Equivalent CLI example:

```powershell
yolo detect train model=yolov8n.pt data=data/yolov8_dataset/dataset.yaml epochs=100 imgsz=640 batch=16 device=0 workers=8 amp=True
```

## 7. Common Useful Commands

Export ONNX:

```powershell
yolo export model=runs/train/a1_yolov8/weights/best.pt format=onnx opset=13 simplify=True
```

Validate:

```powershell
yolo detect val model=runs/train/a1_yolov8/weights/best.pt data=data/yolov8_dataset/dataset.yaml device=0
```

Predict:

```powershell
yolo detect predict model=runs/train/a1_yolov8/weights/best.pt source=data/yolov8_dataset/images/test device=0
```
