# YOLOv8 使用说明（中文详细版，含 GPU）

本文面向本仓库的 Windows + Python 3.9 + RTX 显卡环境，覆盖从标注、划分、训练到导出的完整流程。

## 1. 目录约定

- 训练代码：`third_party/ultralytics`
- 数据集模板：`data/yolov8_dataset`
- 数据划分脚本：`tools/yolov8/split_dataset.py`
- 训练脚本：`tools/yolov8/train_gpu.ps1`
- 标注启动脚本：`tools/yolov8/labelimg_start.ps1`

数据集目录结构：

```text
data/yolov8_dataset/
├── dataset.yaml
├── raw/
│   ├── images/         # 原始图片
│   └── labels/         # 原始标签（YOLO txt）
├── images/
│   ├── train/
│   ├── val/
│   └── test/
└── labels/
    ├── train/
    ├── val/
    └── test/
```

## 2. 初始化环境

### 2.1 克隆 YOLOv8 官方仓库

```powershell
git clone https://github.com/ultralytics/ultralytics.git third_party/ultralytics
```

### 2.2 创建并激活 Python 3.9 虚拟环境

```powershell
py -3.9 -m venv .venv39
.\.venv39\Scripts\Activate.ps1
python -m pip install --upgrade pip
```

### 2.3 通过代理和清华源安装依赖

如本机代理端口为 `127.0.0.1:7897`：

```powershell
$env:HTTP_PROXY="http://127.0.0.1:7897"
$env:HTTPS_PROXY="http://127.0.0.1:7897"
```

安装 ultralytics 和 labelimg（清华源）：

```powershell
pip install ultralytics labelimg -i https://pypi.tuna.tsinghua.edu.cn/simple --trusted-host pypi.tuna.tsinghua.edu.cn
```

安装 CUDA 版 PyTorch（cu121）：

```powershell
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
```

验证 GPU：

```powershell
python -c "import torch; print(torch.__version__); print(torch.cuda.is_available()); print(torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'NO_GPU')"
```

## 3. 标注流程（LabelImg）

启动脚本：

```powershell
.\tools\yolov8\labelimg_start.ps1 -ProxyUrl http://127.0.0.1:7897 -UseTunaSource
```

标注时请确认：

- Open Dir 指向 `data/yolov8_dataset/raw/images`
- Change Save Dir 指向 `data/yolov8_dataset/raw/labels`
- 右下角格式选择为 `YOLO`
- 类别文件建议固定管理，避免训练与标注类别顺序不一致

## 4. 划分训练集/验证集/测试集

```powershell
.\.venv39\Scripts\python.exe .\tools\yolov8\split_dataset.py --dataset-root data/yolov8_dataset --train 0.8 --val 0.1 --seed 42
```

说明：

- 该脚本会重建 `images/train|val|test` 与 `labels/train|val|test`（会清空旧内容）
- `--seed` 固定后可复现实验
- 要求每张图片都有同名 `.txt` 标签，否则报错

## 5. 配置类别（非常关键）

编辑 `data/yolov8_dataset/dataset.yaml`：

- `path` 保持为 `data/yolov8_dataset`
- `train/val/test` 路径保持与目录一致
- `names` 必须与标注类别一一对应，顺序完全一致

示例：

```yaml
names:
  0: person
  1: car
```

## 6. GPU 训练

推荐使用项目脚本：

```powershell
.\tools\yolov8\train_gpu.ps1 -Model yolov8n.pt -Epochs 100 -Batch 16 -ImgSize 640 -Name a1_yolov8 -ProxyUrl http://127.0.0.1:7897 -UseTunaSource
```

常用参数说明：

- `-Model`：初始权重，如 `yolov8n.pt`、`yolov8s.pt`
- `-Epochs`：训练轮次
- `-Batch`：批大小，显存不足时适当减小
- `-ImgSize`：输入尺寸
- `-Device`：默认 `0`（第一张 GPU），也可设 `cpu`
- `-SkipInstall`：已装好依赖时可跳过安装

## 7. 评估、推理、导出

评估：

```powershell
.\.venv39\Scripts\yolo.exe detect val model=runs/train/a1_yolov8/weights/best.pt data=data/yolov8_dataset/dataset.yaml device=0
```

推理：

```powershell
.\.venv39\Scripts\yolo.exe detect predict model=runs/train/a1_yolov8/weights/best.pt source=data/yolov8_dataset/images/test device=0
```

导出 ONNX：

```powershell
.\.venv39\Scripts\yolo.exe export model=runs/train/a1_yolov8/weights/best.pt format=onnx opset=13 simplify=True
```

## 8. SSNE 模型转换（部署到 A1 主板）

训练完成后，需要将 ONNX 模型转换为 SSNE NPU 格式（`.m1model`），才能在 A1 开发板上运行。

### 8.1 导出 ONNX（固定输入尺寸 640×640）

```powershell
.\.venv39\Scripts\yolo.exe export `
    model=runs/train/a1_yolov8/weights/best.pt `
    format=onnx opset=13 simplify=True `
    imgsz=640
```

导出产物：`runs/train/a1_yolov8/weights/best.onnx`

### 8.2 转换为 .m1model

使用 SmartSens 提供的模型转换工具（位于 SDK 内）：

```bash
# 在容器内执行
docker exec A1_Builder bash -lc "
  cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk/output/host/bin &&
  ./ssne_convert \
    --input /app/models/best.onnx \
    --output /app/models/yolov8n_640x640.m1model \
    --input_shape 1,1,640,640 \
    --input_dtype float32
"
```

> 注意：SC132GS 传感器输出为灰度图（单通道），因此 `input_shape` 第二维为 `1`（非 `3`）。  
> 训练时建议将数据集图片转为灰度以提升精度。

### 8.3 部署模型到演示程序

将转换好的 `.m1model` 文件放置到：

```
src/a1_ssne_ai_demo/app_assets/models/yolov8n_640x640.m1model
```

并确认 `project_paths.hpp` 中的路径配置：

```cpp
std::string yolo_model_path{"/app_demo/app_assets/models/yolov8n_640x640.m1model"};
int yolo_num_classes{2};  // 与训练时的类别数一致
std::vector<std::string> yolo_class_names{"person", "car"};  // 与 dataset.yaml names 一致
```

重新编译后烧录至主板即可运行。

## 9. 常见问题排查

- 问题：`torch.cuda.is_available()` 为 `False`  
  处理：确认 NVIDIA 驱动正常、安装的是 `+cu121` 版本 torch、未误装 CPU 版覆盖。

- 问题：下载依赖超时或 SSL 失败  
  处理：优先设置代理 `127.0.0.1:7897`，并使用清华源安装 Python 包。

- 问题：训练时报类别数量不一致  
  处理：检查标注类别与 `dataset.yaml` 的 `names` 顺序是否一致。

- 问题：`split_dataset.py` 报缺少标签  
  处理：确保 `raw/images/xxx.jpg` 对应 `raw/labels/xxx.txt`。

- 问题：`.m1model` 转换失败  
  处理：确认 ONNX opset 版本为 13，输入尺寸为正方形（640×640），且已 simplify。
