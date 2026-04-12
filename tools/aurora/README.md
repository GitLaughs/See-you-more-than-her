# Aurora Capture Tool — A1 摄像头拍照工具 + 底盘调试伴侣

通过 USB Type-C 连接 A1 开发板的 SC132GS 摄像头，进行实时预览和拍照；同时提供 A1↔STM32 WHEELTEC C50X 底盘通信的可视化调试界面。

## 文件结构

```text
tools/aurora/
├── aurora_capture.py      # 基础拍照工具（端口 5000）
├── aurora_companion.py    # 增强版伴侣工具（端口 5001，含底盘调试）
├── chassis_comm.py        # STM32 WHEELTEC C50X 通信后端（Flask Blueprint）
├── launch.ps1             # 统一入口（拍照）
├── templates/
│   ├── companion_ui.html  # 双 Tab 前端页面（摄像头 + 底盘调试）
│   └── (无烧录页面)
├── requirements.txt       # 依赖：opencv, flask, pyserial 等
└── README.md
```

## 功能

### aurora_capture.py（基础工具）

- 实时预览 640×360 灰度摄像头画面
- 拍照保存两种格式：
  - **640×360**: 原始灰度图（摄像头原生输出）
  - **1280×720**: 上采样图（展示用途）
- 摄像头断联自动重连 + 手动刷新
- 启动时默认自动优先 A1 摄像头（`--device -1`）
- 支持前端下拉切换摄像头设备
- Web 前端界面，支持键盘快捷键

### aurora_companion.py（增强伴侣工具）

在基础功能之上新增：

- **摄像头 Tab**：实时预览、拍照、最近拍摄缩略图画廊、摄像头刷新
- **底盘调试 Tab**：
  - 📌 **接线参考**：A1↔STM32 接线表（UART3 PB10/PB11）+ PC 调试接法
  - 🔌 **串口连接**：端口扫描、波特率选择（默认 115200）、连接/断开
  - 🕹 **运动控制**：D-Pad 方向控制、线速度/角速度滑杆、键盘 WASD、急停
  - 📊 **实时遥测**：Vx/Vy/Vz、加速度计、陀螺仪、电池电压（颜色预警）
  - 🔬 **通信日志**：TX/RX 帧历史（彩色字节着色）
  - ⚡ **原始帧发送**：手动输入十六进制帧调试

### chassis_comm.py（通信后端）

Flask Blueprint，挂载在 `/api/chassis/`：

| 端点 | 方法 | 说明 |
|------|------|------|
| `/ports` | GET | 列出可用串口 |
| `/connect` | POST | 连接串口 `{port, baud}` |
| `/disconnect` | POST | 断开串口 |
| `/status` | GET | 连接状态 + 最新遥测 |
| `/move` | POST | 发送速度指令 `{vx, vy, vz}` |
| `/stop` | POST | 急停（vx=vy=vz=0） |
| `/tx_log` | GET | 最近 20 条发送帧 |
| `/rx_log` | GET | 最近 20 条接收帧 |
| `/raw_send` | POST | 发送原始十六进制帧 |

## 使用方法

### 1. 安装依赖

```bash
pip install -r requirements.txt
```

### 2. 连接 A1 开发板

通过 USB Type-C 数据线连接 A1 开发板到 PC。确保开发板已开机且摄像头正常工作。

### 3. 启动工具

**基础拍照工具（端口 5000）：**

```bash
python aurora_capture.py
```

**增强伴侣工具，含底盘调试（端口 5001）：**

```bash
python aurora_companion.py
```

或使用统一入口（推荐）：

```powershell
cd tools/aurora

# 自动优先选择 A1 摄像头
.\launch.ps1

# 手工指定某个设备号
.\launch.ps1 -Device 2
```

参数说明：
- `--device -1`: 摄像头设备 ID（默认 -1，自动优先 A1）
- `--output <dir>`: 拍照保存目录（默认 `../../data/yolov8_dataset/raw/images`）
- `--port 5001`: Web 服务端口（默认 5001，Companion）/ `5000`（Capture）

### 4. 打开浏览器

- 基础工具：访问 `http://localhost:5000`
- 伴侣工具：访问 `http://localhost:5001`（含底盘调试 Tab）

### 5. 拍照

- 点击 **📷 拍照 1280×720** 按钮拍摄原始灰度图
- 点击 **🏹 拍照 640×360** 按钮拍摄 YOLOv8 训练用裁剪图
- 快捷键（摄像头 Tab）：`1` = 1280×720，`2` = 640×360，`R` = 刷新

### 6. 底盘调试（仅 aurora_companion.py）

1. 切换到「底盘通信调试」Tab
2. 按「⟳」刷新串口列表，选择对应端口（如 `COM3`）
3. 点击「🔌 连接」
4. 使用 D-Pad 或键盘 WASD 控制小车；空格键急停
5. 遥测面板实时显示速度、IMU、电压数据
6. 如需手动调试，可在「原始帧发送」区域输入十六进制帧并发送

## 技术说明

### 摄像头格式

SC132GS 传感器通过 USB Type-C 输出标准 16:9 的 640×360 灰度视频流。工具使用 OpenCV 打开摄像头时，会尝试设置 FOURCC 为 `Y800`/`GREY` 以正确解析灰度格式。

### 640×360 裁剪

640×360 为原生输出，1280×720 由工具执行放大生成，仅用于展示或兼容输出。

### 与 SDK pipeline 的关系

SDK 中 `pipeline_image.cpp` 负责板端推理的图像预处理；此工具的 640×360 裁剪专用于在 PC 端制作 YOLO 训练数据集，两者场景不同。

### WHEELTEC C50X 通信协议

chass_comm.py 基于 STM32 固件源码（`WHEELTEC_C50X_2025.12.26/BALANCE/`）实现：

**指令帧（A1 → STM32，11 字节）：**
```
[0x7B][Cmd][0x00][Vx_H][Vx_L][Vy_H][Vy_L][Vz_H][Vz_L][BCC][0x7D]
```
- `Cmd`：`0x00` = 正常运动，`0x01/0x02` = 回充，`0x03` = 对接
- 速度单位：mm/s（int16 大端序）
- `BCC = XOR(byte[0]..byte[8])`

**遥测帧（STM32 → A1，24 字节）：**
```
[0x7B][FlagStop][Vx×2][Vy×2][Vz×2][Ax×2][Ay×2][Az×2][Gx×2][Gy×2][Gz×2][Volt×2][BCC][0x7D]
```
- 速度：int16 mm/s；加速度/陀螺：int16 ÷ 1000 → m/s² / rad/s
- 电压：int16 ÷ 1000 → V

**接线（A1 ↔ STM32）：**

| A1 | STM32 | 说明 |
|----|-------|------|
| GPIO_PIN_0 (UART0 TX) | PB11 (UART3 RX) | 指令下发 |
| GPIO_PIN_2 (UART0 RX) | PB10 (UART3 TX) | 遥测回传 |
| GND | GND | 共地（必须）|

**PC 调试接法（USB 串口调试器）：**

| USB 串口 | STM32 | 说明 |
|---------|-------|------|
| TX | PB11 (UART3 RX) | PC → STM32 |
| RX | PB10 (UART3 TX) | STM32 → PC |
| GND | GND | 共地 |
