# A1 Gray 5-Class Classifier

基于 MobileNetV1 的灰度五分类训练集制作与训练流程，支持 `person`、`stop`、`forward`、`obstacle`、`NoTarget` 五类。

## 依赖安装

```bash
pip install -r requirements.txt
```

## 项目结构

```
.
├── train_a1_5class_classifier.py   # 5 类训练脚本
├── prepare_video_dataset.py        # 视频转灰度图片数据集
├── generate_negative_dataset.py    # 采样 NoTarget 负样本
├── requirements.txt                # Python 依赖
└── outputs/                        # 训练输出目录
```

## 数据集目录

### 输入视频目录

```
datasets/
├── person/
├── stop/
├── forward/
├── obstacle/
└── NoTarget/
```

### 输出图片目录

```
processed_dataset/
├── train/
│   ├── person/
│   ├── stop/
│   ├── forward/
│   ├── obstacle/
│   └── NoTarget/
├── val/
│   └── <class>/
└── test/
    └── <class>/
```

## 制作训练集

### 1. 视频转灰度图片

```bash
python prepare_video_dataset.py \
    --dataset_dir /path/to/video/datasets \
    --output_dir /path/to/processed_dataset \
    --crop 210 270 750 810 \
    --frame_step 3
```

说明：
- 输入视频已是灰度也可以直接用
- 脚本会把裁剪结果按灰度保存
- 默认按视频数切分 train/val/test

### 2. 训练 5 类模型

```bash
python train_a1_5class_classifier.py \
    --dataset_dir /path/to/processed_dataset \
    --output_dir outputs/a1_5class_mobilenetv1 \
    --epochs 30 \
    --batch_size 64 \
    --lr 1e-3
```

模型配置：
- 输入：`1 x 1 x 320 x 320` 灰度张量
- 输出：`5` 类 logits
- 损失：`CrossEntropyLoss`
- 预训练：默认关闭

## 输出

- `best.pt`
- `last.pt`
- `metadata.json`
- 可选 `best.onnx`
