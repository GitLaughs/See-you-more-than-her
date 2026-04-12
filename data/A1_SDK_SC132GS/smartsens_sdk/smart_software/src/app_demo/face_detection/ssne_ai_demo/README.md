# SSNE AI 演示项目

## 项目概述

基于 SmartSens SSNE (SmartSens Neural Engine) 的人脸检测 + 底盘控制演示程序。  
SC132GS 摄像头采集 **1280×720 全分辨率** 灰度图，通过硬件 ISP 缩放至 **640×360** 送 NPU 推理，
后处理坐标系直接映射回 1280×720，并通过 GPIO UART 驱动 WHEELTEC C50X 底盘实现"见脸前进、无脸停车"的闭环控制。

### 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 传感器输出 | 1280×720 Y8 | 无裁剪，全分辨率 |
| 推理输入 | 640×360 Y8 | RunAiPreprocessPipe 硬件缩放 |
| 比例因子 | w=2.0, h=2.0 | Postprocess 坐标映射回 1280×720 |
| 人脸置信度阈值 | 0.4 | SCRFD |
| 底盘前进速度 | 100 mm/s | 检测到人脸时 |
| UART 波特率 | 115200 | A1 PIN0(TX) → STM32 UART3(PB11) |

---

## 文件结构

```
ssne_ai_demo/
├── demo_face.cpp                  # 主程序：初始化 + 主循环（采图→检测→底盘控制→OSD）
├── project_paths.hpp              # 全局常量（模型路径、阈值、UART 配置等）
├── include/
│   ├── common.hpp                # IMAGEPROCESSOR / SCRFDGRAY / FaceDetectionResult
│   ├── utils.hpp                 # NMS、归并排序、VISUALIZER 类声明
│   ├── osd-device.hpp            # VISUALIZER OSD 绘制接口
│   └── chassis_controller.hpp   # ChassisController / ChassisState（WHEELTEC 11字节协议）
├── src/
│   ├── pipeline_image.cpp        # IMAGEPROCESSOR：全分辨率 1280×720 采集（无裁剪）
│   ├── scrfd_gray.cpp            # SCRFDGRAY：SSNE 推理 + DFL 解码 + NMS
│   ├── osd-device.cpp            # VISUALIZER：OSD 硬件叠加检测框
│   ├── utils.cpp                 # NMS / 归并排序实现
│   └── chassis_controller.cpp   # ChassisController：GPIO UART 底盘通信实现
├── app_assets/
│   ├── models/
│   │   └── face_640x480.m1model  # SCRFD 推理模型（当前板端模型文件）
│   └── colorLUT.sscl             # OSD 颜色查找表
├── cmake_config/
│   └── Paths.cmake               # SDK 库路径（BASE_DIR / EXPORT_LIB_M1_SDK_ROOT_PATH）
├── scripts/
│   └── run.sh                    # 板端启动脚本
└── CMakeLists.txt                # 交叉编译配置（自动 GLOB src/*.cpp）
```

---

## 处理管道

```
SC132GS 摄像头
      │ 1280×720 Y8（原始帧）
      ▼
OnlineSetCrop(kPipeline0, 0, 1280, 0, 720)  ← 无裁剪，全帧
OnlineSetOutputImage(kPipeline0, SSNE_Y_8, 1280, 720)
      │ pipe0: 1280×720 Y8
      ▼
IMAGEPROCESSOR::GetImage()
      │ img_sensor (1280×720 Y8 tensor)
      ▼
SCRFDGRAY::Predict(img_sensor, det_result, 0.4f)
      │ RunAiPreprocessPipe(pipe_offline, img_sensor, model_input_640x360)
      │   ← 硬件 ISP 缩放 1280×720 → 640×360
      │ ssne_inference(model_input_640x360)
      │ Postprocess: w_scale=2.0, h_scale=2.0
      │   ← 坐标直接映射回 1280×720 空间
      ▼
FaceDetectionResult (boxes in 1280×720 coords)
      │
      ├─ 有人脸 ──► ChassisController::SendVelocity(100, 0, 0)  前进 100 mm/s
      │             VISUALIZER::Draw(boxes)  OSD 叠加检测框
      │
      └─ 无人脸 ──► ChassisController::SendVelocity(0, 0, 0)    停车
                    VISUALIZER::Draw({})     清除 OSD
```

---

## 底盘通信协议（WHEELTEC C50X）

### A1 → STM32 控制帧（11 字节）

```
字节   内容    说明
[0]    0x7B   帧头
[1]    Cmd    0x00 = 正常控制
[2]    0x00   保留
[3]    Vx_H   X 轴速度高字节 (mm/s, int16_t)
[4]    Vx_L   X 轴速度低字节
[5]    Vy_H   Y 轴速度高字节 (AKM 始终 = 0)
[6]    Vy_L
[7]    Vz_H   Z 轴角速度/前轮转角高字节
[8]    Vz_L
[9]    BCC    XOR(bytes[0..8])
[10]   0x7D   帧尾
```

**速度示例：** 前进 0.1 m/s → Vx=100 → `[0x7B, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, BCC, 0x7D]`

### STM32 → A1 状态帧（24 字节）

| 字节 | 内容 | 单位 |
|------|------|------|
| [0] | 0x7B | 帧头 |
| [1] | Stop_Flag | 0=正常, 1=紧急停止 |
| [2-3] | Vx | mm/s |
| [4-5] | Vy | mm/s |
| [6-7] | Vz | mrad/s |
| [8-13] | Ax/Ay/Az | ÷1000 = m/s² |
| [14-19] | Gx/Gy/Gz | ÷1000 = rad/s |
| [20-21] | Volt | ÷100 = V |
| [22] | BCC | XOR(bytes[0..21]) |
| [23] | 0x7D | 帧尾 |

---

## SSNE API 说明

### OnlineSetCrop

```cpp
int OnlineSetCrop(PipelineIdType pipeline_id,
                  uint16_t x_start, uint16_t x_end,
                  uint16_t y_start, uint16_t y_end);
```

设置 Online Pipeline 的传感器裁剪区域。本项目传入 `(kPipeline0, 0, 1280, 0, 720)` 即无裁剪全帧。

### OnlineSetOutputImage

```cpp
int OnlineSetOutputImage(PipelineIdType pipeline_id,
                         uint8_t dtype, uint16_t width, uint16_t height);
```

设置 pipe 输出格式与尺寸，需与裁剪尺寸一致。

### RunAiPreprocessPipe

```cpp
int RunAiPreprocessPipe(AiPreprocessPipe handle,
                        ssne_tensor_t input, ssne_tensor_t output);
```

硬件 ISP 缩放图像到模型输入尺寸（本项目: 1280×720 → 640×360）。

---

## 编译与运行

### 在 Docker 容器内编译

```bash
# 全量编译（含内核打包）
bash /app/scripts/build_complete_evb.sh --skip-ros

# 仅重编 Demo（快速迭代）
cd /app/data/A1_SDK_SC132GS/smartsens_sdk
rm -rf output/build/ssne_ai_demo
make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_ai_demo
```

### 板端运行

```bash
/app_demo/scripts/run.sh

# 预期输出：
# [INFO] open online pipe0: 0
# [INFO] 底盘控制器初始化成功 (UART 115200)
# 键盘监听线程已启动，输入 'q' 退出程序...
# [DRIVE] 检测到人脸 1 个，直行 100 mm/s
# [STOP] 未检测到人脸，停车
```

输入 `q` + 回车 退出程序并释放所有资源。

---

## 调试 Tips

- 确认 A1 `GPIO_PIN_0`(TX) 与 STM32 `PB11`(UART3 RX) 正确连接
- 使用 `aurora_companion.py` Web UI 的「A1 ↔ STM32 联通测试」按钮验证 UART 双向链路
- 模型文件 `face_640x480.m1model` 必须存在于 `app_assets/models/`，否则 `ssne_initial()` 之后的 `detector.Initialize()` 会失败

---

## 编译与运行

### 在 Docker 容器内编译

```bash
# 全量编译（含内核打包）
bash /app/scripts/build_complete_evb.sh --skip-ros

# 仅重编 Demo（快速迭代）
cd /app/data/A1_SDK_SC132GS/smartsens_sdk
rm -rf output/build/ssne_ai_demo
make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_ai_demo
```

### 板端运行

```bash
/app_demo/scripts/run.sh

# 预期输出：
# [INFO] open online pipe0: 0
# [INFO] 底盘控制器初始化成功 (UART 115200)
# 键盘监听线程已启动，输入 'q' 退出程序...
# [DRIVE] 检测到人脸 1 个，直行 100 mm/s
# [STOP] 未检测到人脸，停车
```

输入 `q` + 回车 退出程序并释放所有资源。

---

## 调试 Tips

- 确认 A1 `GPIO_PIN_0`(TX) 与 STM32 `PB11`(UART3 RX) 正确连接
- 使用 `aurora_companion.py` Web UI 的「A1 ↔ STM32 联通测试」按钮验证 UART 双向链路
- 模型文件 `face_640x480.m1model` 必须存在于 `app_assets/models/`，否则 `ssne_initial()` 之后的 `detector.Initialize()` 会失败

