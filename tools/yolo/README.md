# MobileNet Sigmoid 分类训练与 A1 部署模板

## 用途

本目录复用 [demo-rps](../../demo-rps/) 的训练、转换、推理思路：MobileNetV1 backbone + sigmoid 多标签输出 + 负样本全 0，用于固定 ROI 分类、无目标识别、上板 m1model 验证。

官方工具负责视频截帧和 YOLO 数据集处理；本目录只放 RPS 风格轻量分类模型训练与部署模板。

## 依赖

```bash
pip install torch torchvision timm pillow onnx opencv-python numpy
```

无 GUI 环境可把 `opencv-python` 换成 `opencv-python-headless`。

## 数据集结构

```text
processed_dataset/
├── P/
│   └── *.png
├── R/
│   └── *.png
├── S/
│   └── *.png
└── N/
    └── *.png
```

`P/R/S` 是正样本类别。`N` 是负样本目录，训练 target 为全 0。类别名可换，例如：

```text
processed_dataset/
├── stop/
├── left/
├── right/
└── N/
```

对应训练时传 `--classes stop left right --negative_class N`。

## 训练命令

```bash
python tools/yolo/train_mobilenet_sigmoid_classifier.py \
  --dataset_dir processed_dataset \
  --output_dir outputs/rps_mobilenetv1 \
  --classes P R S \
  --negative_class N \
  --epochs 30 \
  --batch_size 64 \
  --lr 1e-3 \
  --weight_decay 1e-4 \
  --pretrained
```

Windows：

```bat
tools\yolo\train_mobilenet_sigmoid_classifier.bat --dataset_dir processed_dataset --output_dir outputs/rps_mobilenetv1 --classes P R S --negative_class N --epochs 30 --batch_size 64 --lr 1e-3 --weight_decay 1e-4 --pretrained
```

输出：

```text
outputs/rps_mobilenetv1/
├── best.pt
├── last.pt
└── metadata.json
```

## 模型架构

Backbone: `mobilenetv1_100` from `timm`，`num_classes=0`，`global_pool=""`。

Input: `1 x 3 x 320 x 320`。

Head:

1. `Conv2d(channels -> hidden_dim, 1x1)`
2. `BatchNorm2d + ReLU`
3. `Dropout2d`
4. `Conv2d(hidden_dim -> class_count, full feature map)`
5. `Flatten`

Output: sigmoid confidence vector in class order，例如 `[P, R, S]`。

## 负样本语义

正样本 target 是 one-hot：

```text
P -> [1, 0, 0]
R -> [0, 1, 0]
S -> [0, 0, 1]
```

负样本 target 是全 0：

```text
N -> [0, 0, 0]
```

推理时取最大置信度；如果最大分数高于 threshold，输出对应类别，否则输出 `NoTarget`。

## 训练细节

损失函数：`BCEWithLogitsLoss`，对每路输出独立计算二元交叉熵。

优化器：`AdamW`，默认 `lr=1e-3`，`weight_decay=1e-4`。

学习率调度：`CosineAnnealingLR`，`T_max=epochs`。

训练增强与 demo-rps 一致：

1. `Resize((352, 352))`
2. `RandomResizedCrop(320, scale=(0.75, 1.0), ratio=(0.9, 1.1))`
3. `RandomHorizontalFlip(p=0.5)`
4. `RandomRotation(degrees=18)`
5. `ColorJitter(brightness=0.2, contrast=0.2, saturation=0.15, hue=0.05)`
6. `RandomPerspective(distortion_scale=0.15, p=0.2)`
7. `ToTensor()`
8. `Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])`
9. `RandomErasing(p=0.2, scale=(0.02, 0.12), ratio=(0.3, 3.0))`

验证预处理：

1. `Resize((320, 320))`
2. `ToTensor()`
3. `Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])`

## 验证指标

| 指标 | 说明 |
| --- | --- |
| `exact_match` | 所有类别预测与标签完全一致的比例 |
| `positive_top1` | 正样本中 Top-1 分类准确率 |
| `negative_recall` | 负样本中无任何类别超过阈值的比例 |

最佳模型选择：验证阶段以 `(positive_top1 + negative_recall) / 2` 作为综合得分，最高时保存为 `best.pt`。

## 单图推理

```bash
python tools/yolo/infer_mobilenet_sigmoid_classifier.py \
  --image_path path/to/image.png \
  --checkpoint outputs/rps_mobilenetv1/best.pt
```

输出示例：

```json
{
  "label": "P",
  "score": 0.9234,
  "threshold": 0.5,
  "confidence": {
    "P": 0.9234,
    "R": 0.0123,
    "S": 0.0456
  }
}
```

## ONNX 导出

```bash
python tools/yolo/export_mobilenet_sigmoid_onnx.py \
  --checkpoint outputs/rps_mobilenetv1/best.pt \
  --output_path outputs/rps_mobilenetv1/best.onnx \
  --opset 18
```

Windows：

```bat
tools\yolo\export_mobilenet_sigmoid_onnx.bat --checkpoint outputs/rps_mobilenetv1/best.pt --output_path outputs/rps_mobilenetv1/best.onnx --opset 18
```

ONNX 输入固定为 `1 x 3 x 320 x 320`，输出名为 `confidence`，输出已包含 sigmoid。

## 校准张量

```bash
python tools/yolo/save_calibration_tensors.py \
  --dataset_dir processed_dataset \
  --output_dir outputs/rps_mobilenetv1/calibrate_datasets_bin \
  --cal_num 50 \
  --eval_num 20 \
  --output_format bin
```

预处理固定：RGB、resize `320x320`、ImageNet mean/std normalize、NCHW、float32。

输出：

```text
calibrate_datasets_bin/
├── calibrate_datasets/
├── evaluate_datasets/
├── calibrate_datasets/calibration_manifest.txt
└── evaluate_datasets/evaluation_manifest.txt
```

## m1model 转换

按“【进站必读】模型部署流程说明（包括模型切分与 CPU 后处理实现-以 yolov8n 为例）”执行。此模板 ONNX 输出已包含 sigmoid，板端 CPU 后处理只需读取 `confidence` 数组并做 threshold/argmax。

## 板端 Predict 逻辑

RPS demo 参考：[rps_classifier.cpp](../../demo-rps/ssne_ai_demo/src/rps_classifier.cpp)。核心流程：

```text
RunAiPreprocessPipe
-> ssne_get_model_input_dtype
-> set_data_type
-> ssne_inference
-> ssne_getoutput
-> read float confidence array
-> argmax
-> threshold
-> class / NoTarget
```

C++ 关键逻辑：

```cpp
ssne_getoutput(model_id, 1, outputs);
float* data = (float*)get_data(outputs[0]);

float scores[3] = {data[0], data[1], data[2]};
int max_idx = 0;
float max_score = scores[0];
for (int i = 1; i < 3; i++) {
    if (scores[i] > max_score) {
        max_score = scores[i];
        max_idx = i;
    }
}

const char* labels[] = {"P", "R", "S"};
if (max_score > 0.6f) {
    out_label = labels[max_idx];
    out_score = max_score;
} else {
    out_label = "NoTarget";
    out_score = max_score;
}
```

如果类别数量变化，`scores` 和 `labels` 长度必须同步改。
