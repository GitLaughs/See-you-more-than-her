# SSNE AI Demo

这个目录名沿用历史命名，当前实现已经不是人脸识别，而是基于 YOLOv8 的人物检测 + 底盘联动演示。

## 核心行为

- SC132GS 采集 1280×720 Y8 全分辨率原始帧
- `RunAiPreprocessPipe` 将输入缩放到 640×360 送入 NPU
- YOLOv8 head6 模型在板端推理，CPU 完成 DFL decode 与 NMS
- 检测到 `person` 时，通过 WHEELTEC C50X 协议让小车前进
- 未检测到目标时立即停车，并清空 OSD

## 关键文件

```text
demo_face.cpp              主循环：采图、检测、OSD、底盘控制
project_paths.hpp          分辨率、模型路径、阈值、UART 参数
src/pipeline_image.cpp     全分辨率采集管线
src/yolov8_gray.cpp        YOLOv8 推理和后处理
src/chassis_controller.cpp 底盘协议与串口发送
src/osd-device.cpp         OSD 叠框实现
```

## 主要参数

| 项目 | 值 |
| --- | --- |
| 传感器输入 | 1280 × 720 Y8 |
| 模型输入 | 640 × 360 Y8 |
| 类别数 | 4 |
| 触发类别 | person |
| 前进速度 | 100 mm/s |
| 串口波特率 | 115200 |

## 运行方式

### 在容器里编译

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

### 在板端运行

```bash
/app_demo/scripts/run.sh
```

运行后按 `q` 回车退出。

## 接线

| A1 | STM32 | 说明 |
| --- | --- | --- |
| GPIO_PIN_0 (UART0 TX) | PB11 (UART3 RX) | A1 下发控制帧 |
| GPIO_PIN_2 (UART0 RX) | PB10 (UART3 TX) | STM32 回传遥测 |
| GND | GND | 必须共地 |

## 备注

- `best_yolov8_640x360.m1model` 是当前板端模型文件
- `face_640x480.m1model` 仅作历史保留
- A1 的图像显示由硬件 OSD 完成，不需要 Windows 端再叠框
