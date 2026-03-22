# YOLOv8 数据集

本目录为 A1 机器人 YOLOv8 训练数据集，支持人物、手势、障碍物等目标类别的标注与训练。

## 目录结构

```text
yolov8_dataset/
├── dataset.yaml            # 训练配置（路径、类别）
├── raw/
│   ├── images/             # 原始图片（不分 train/val，未切分）
│   └── labels/             # 原始 YOLO txt 标注
├── images/
│   ├── train/              # 训练集图片
│   ├── val/                # 验证集图片
│   └── test/               # 测试集图片
└── labels/
    ├── train/              # 训练集标注
    ├── val/                # 验证集标注
    └── test/               # 测试集标注
```

## 快速开始

### 1. 标注图片

```powershell
.\tools\yolov8\labelimg_start.ps1 -ProxyUrl http://127.0.0.1:7897 -UseTunaSource
```

- Open Dir → `data/yolov8_dataset/raw/images`
- Change Save Dir → `data/yolov8_dataset/raw/labels`
- 右下角格式选 **YOLO**

### 2. 划分数据集

```powershell
.\.venv39\Scripts\python.exe tools/yolov8/split_dataset.py `
    --dataset-root data/yolov8_dataset `
    --train 0.8 --val 0.1 --seed 42
```

脚本会自动重建 `images/train|val|test` 和 `labels/train|val|test`（会清空旧内容）。

### 3. 配置 dataset.yaml

```yaml
path: data/yolov8_dataset
train: images/train
val: images/val
test: images/test

nc: 2
names:
  0: person
  1: car
```

- `nc`：类别总数，必须与标注一致
- `names`：类别名称，顺序必须与 LabelImg 标注时的顺序完全一致

### 4. 开始训练

```powershell
.\tools\yolov8\train_gpu.ps1 -Model yolov8n.pt -Epochs 100 -Batch 16 -ImgSize 640 -Name a1_yolov8
```

详细训练说明见 [docs/YOLOV8_TRAINING.md](../../docs/YOLOV8_TRAINING.md)。

## 注意事项

- **不要将大体积图片直接提交 Git**，建议只提交 `dataset.yaml` 和少量示例图片
- 每张图片都需要对应同名 `.txt` 标签，否则 `split_dataset.py` 会报错
- 标注类别顺序变更后，必须重新标注或批量修改标签文件中的类别索引
- SC132GS 传感器输出为**灰度图**，建议标注时使用传感器实拍图，避免用彩色图训练后精度下降

## 类别约定

| 索引 | 类别名 | 说明 |
|---|---|---|
| 0 | `person` | 人物（全身/半身） |
| 1 | `car` | 车辆（障碍物） |

可根据实际需求在 `dataset.yaml` 中扩充（手势、特定物体等）。
