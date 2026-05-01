# YOLOv8 本地训练与 A1 转换流程

## 目标

从根目录 [video.mp4](../../video.mp4) 或自选视频制作 640x480 YOLO 训练集，使用 [third_party/ultralytics/](../../third_party/ultralytics/) 训练 YOLOv8，再导出 ONNX、剪裁 head6、转换为 A1 `.m1model`。

## 目录

- 视频标注工具：[tools/video/](../video/)
- 原始图片：[raw/images/](raw/images/)
- 原始标签：[raw/labels/](raw/labels/)
- 划分后图片：`images/train|val|test/`
- 划分后标签：`labels/train|val|test/`
- 数据配置：[dataset.yaml](dataset.yaml)
- YOLOv8 代码：[third_party/ultralytics/](../../third_party/ultralytics/)

## 1. 拍视频

把视频放到仓库根目录：

```text
video.mp4
```

也可以在前端输入绝对路径或相对仓库根目录的路径。

## 2. 从视频生成图片和标签

```powershell
cd <repo-root>
.\tools\video\launch.ps1
```

打开 `http://127.0.0.1:6210`，输入：

- 视频路径：默认 `video.mp4`
- 类别名：例如 `person`
- 类别 ID：例如 `0`
- ROI 坐标：原始视频坐标 `x1 y1 x2 y2`
- 抽帧间隔：例如 `5`

输出固定为 640x480：

```text
tools/yolo/raw/images/*.jpg
tools/yolo/raw/labels/*.txt
```

标签格式：

```text
class_id x_center y_center width height
```

## 3. 检查类别配置

编辑 [dataset.yaml](dataset.yaml)，确保 `names` 顺序与前端输入 class id 一致。

默认：

```yaml
names:
  0: person
  1: gesture1
  2: gesture2
  3: obstacle_box
```

如果前端使用 `class_id=0 class_name=ball`，这里也应改为：

```yaml
names:
  0: ball
```

## 4. 划分训练集

```bash
python tools/yolo/split_dataset.py --dataset-root tools/yolo --train 0.8 --val 0.1 --seed 42
```

脚本会重建：

```text
tools/yolo/images/train|val|test/
tools/yolo/labels/train|val|test/
```

要求每张图片都有同名标签。

## 5. 安装/使用 Ultralytics

```powershell
py -3.9 -m venv .venv39
.\.venv39\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -e third_party/ultralytics
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
```

如果只用 CPU，把 PyTorch 安装命令换成本机适合版本。

## 6. 训练 YOLOv8

```powershell
.\.venv39\Scripts\yolo.exe detect train model=yolov8n.pt data=tools/yolo/dataset.yaml epochs=100 batch=16 imgsz=640 device=0 project=tools/yolo/runs name=a1_yolo_640x480
```

输出：

```text
tools/yolo/runs/a1_yolo_640x480/weights/best.pt
```

## 7. 验证与推理

```powershell
.\.venv39\Scripts\yolo.exe detect val model=tools/yolo/runs/a1_yolo_640x480/weights/best.pt data=tools/yolo/dataset.yaml device=0
```

```powershell
.\.venv39\Scripts\yolo.exe detect predict model=tools/yolo/runs/a1_yolo_640x480/weights/best.pt source=tools/yolo/images/test device=0
```

## 8. 导出 ONNX

```powershell
.\.venv39\Scripts\yolo.exe export model=tools/yolo/runs/a1_yolo_640x480/weights/best.pt format=onnx opset=13 simplify=True imgsz=640
```

输出通常在：

```text
tools/yolo/runs/a1_yolo_640x480/weights/best.onnx
```

## 9. 剪裁 ONNX head6

按 [docs/15_AI模型转换与部署.md](../../docs/15_AI模型转换与部署.md) 的 head6 输出节点剪裁方法处理 `best.onnx`。

剪裁后模型把 YOLO decode、DFL、NMS 放到 CPU 后处理，便于 A1 NPU 运行。

## 10. 转换 m1model 和上板

按 [docs/15_AI模型转换与部署.md](../../docs/15_AI模型转换与部署.md)：

1. 用 SmartSens 模型转换工具/思思 AI 助手把 head6 ONNX 转成 `.m1model`。
2. 放到板端 app assets models 目录。
3. 确认板端前处理和训练一致：输入链路统一 640x480。
4. 重新构建镜像并上板验证。

## MobileNet Sigmoid 模板

本目录还保留 RPS 风格 MobileNet sigmoid 分类脚本：

- [train_mobilenet_sigmoid_classifier.py](train_mobilenet_sigmoid_classifier.py)
- [export_mobilenet_sigmoid_onnx.py](export_mobilenet_sigmoid_onnx.py)
- [infer_mobilenet_sigmoid_classifier.py](infer_mobilenet_sigmoid_classifier.py)
- [save_calibration_tensors.py](save_calibration_tensors.py)

它们用于固定 ROI 分类，不用于 YOLO bbox 检测训练。
