# Aurora Capture Tool — A1 摄像头拍照工具 + 底盘调试伴侣

通过 USB Type-C 连接 A1 开发板的 SC132GS 摄像头，进行实时预览和拍照；同时提供 A1↔STM32 WHEELTEC C50X 底盘通信的可视化调试界面。

## 文件结构

```text
tools/aurora/
├── aurora_capture.py      # 基础拍照工具（端口 5000）
├── aurora_companion.py    # 增强版伴侣工具（端口 5001，含底盘与联通测试）
├── a1_companion.py        # A1 专用伴侣入口（默认端口 5803）
├── stm32_port_probe.py    # Windows 串口端口探测器（逐口发送前进帧，找有效回包）
├── chassis_comm.py        # STM32 WHEELTEC C50X 通信后端（Flask Blueprint）
├── launch.ps1             # 统一 Windows 测试入口（companion / A1 / viewer / probe / capture）
├── templates/
│   ├── companion_ui.html   # 三页面前端：摄像头 / A1-STM32 联通测试 / 底盘调试
│   └── stm32_port_probe.html # 串口端口探测页面
├── requirements.txt       # 依赖：opencv, flask, pyserial 等
└── README.md
```

## 功能

### aurora_capture.py（基础工具）

- 实时预览 SC132GS 灰度画面（采集口径 1280×720）
- 拍照保存两种格式：
  - **1280×720**: 传感器采集分辨率原图
  - **640×360**: 16:9 中心裁剪，用于训练集
- 摄像头断联自动重连 + 手动刷新
- 启动时默认自动优先 A1 摄像头（`--device -1`）

### aurora_companion.py（增强伴侣工具）

在基础功能之上新增：

- **摄像头采集页**：实时预览、拍照、最近拍摄缩略图画廊、摄像头刷新
- **A1-STM32 联通测试页**：
  - 一键发送零速度测试帧
  - 判断单向/双向连通
  - 显示回传遥测详情
- **底盘通信调试页**：
  - 📌 接线参考（UART3 PB10/PB11）+ PC 调试接法
  - 🔌 串口连接（端口扫描、波特率、连接/断开）
  - 🕹 运动控制（D-Pad、WASD、急停）
  - 📊 实时遥测（速度、IMU、电压）
  - 🔬 TX/RX 日志与原始帧发送

### a1_companion.py（A1 专用伴侣入口）

- 复用 `aurora_companion.py` 的全部能力，但默认切到 A1 head6 模型与真实类别名
- 默认端口单独使用 `5803`，便于和旧版伴侣工具同时调试
- 适合直接查看 A1 开发板回传的视频、OSD 检测框、串口连接信息和 WASD 控制

### launch.ps1（统一测试入口）

- `launch.ps1` 默认启动 `aurora_companion.py`
- `launch.ps1 -Mode a1` 启动 A1 专用伴侣，默认端口 `5803`
- `launch.ps1 -Mode viewer` 启动 A1 Viewer，默认端口 `5802`
- `launch.ps1 -Mode probe` 启动 STM32 串口探测页，默认端口 `5006`
- `launch.ps1 -Mode capture` 启动基础拍照工具，默认端口 `5000`
- `launch.ps1` 只负责 Windows 侧测试与联调，不负责 EVB 烧录

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
| `/ping` | POST | 联通测试（A1→STM32 与 STM32→A1 回传校验） |
| `/tx_log` | GET | 最近 30 条发送帧 |
| `/rx_log` | GET | 最近 30 条接收帧 |
| `/raw_send` | POST | 发送原始十六进制帧 |

### stm32_port_probe.py（Windows 串口端口探测器）

- 枚举 Windows 上的 COM 口并逐个测试
- 向每个端口发送低速前进帧，等待 24 字节遥测回包
- 扫描结束后自动补停车帧，避免持续前进
- 适合在接口标签看不到时，快速确认当前硬件连到了哪个串口

## 使用方法

### 1. 安装依赖

```bash
pip install -r requirements.txt
```

### 2. 连接 A1 开发板

通过 USB Type-C 数据线连接 A1 开发板到 PC，确保开发板已开机且摄像头正常工作。

### 3. 启动工具

```powershell
cd tools/aurora

# 默认自动优先选择 A1 摄像头，并启动浏览器
.\launch.ps1

# 手工指定设备号
.\launch.ps1 -Device 2

# 显示底层驱动日志
.\launch.ps1 -ShowDriverLogs
```

也可直接运行：

```bash
# 基础工具（端口 5000）
python aurora_capture.py

# 增强工具（端口 5001）
python aurora_companion.py

# A1 专用伴侣（端口 5803）
python a1_companion.py

# 串口端口探测器（端口 5006）
python stm32_port_probe.py
```

### 4. 打开浏览器

- 基础工具：`http://127.0.0.1:5000`
- 伴侣工具：`http://127.0.0.1:5001`
- A1 专用伴侣：`http://127.0.0.1:5803`

### 5. 拍照

- 点击 **拍照 1280×720** 保存原始采集图
- 点击 **拍照 640×360** 保存训练裁剪图
- 快捷键：`1` = 1280×720，`2` = 640×360，`R` = 刷新摄像头

### 6. 联通测试与底盘调试（aurora_companion.py）

1. 在「底盘通信调试」页完成串口连接
2. 切换到「A1-STM32 联通测试」页，点击「发起联通测试」
3. 查看结果是“单向连通”还是“双向连通”
4. 返回调试页进行运动控制与遥测联调

## 技术说明

### 摄像头采集口径

- SC132GS 传感器采集按 **1280×720** 灰度口径处理
- 训练集输出使用 **640×360**（16:9 中心裁剪）
- 读取异常尺寸时，工具会做兜底缩放以保持统一输出

### 与 Aurora SDK 口径关系

Aurora SDK 日志中常见 `720x1280` 缓冲尺寸（与显示方向/缓冲布局相关）；本工具对外统一为 `1280x720` 采集语义与 `640x360` 训练语义，便于数据集制作与下游模型训练。

### WHEELTEC C50X 协议要点

#### 指令帧（A1 → STM32，11 字节）

```text
[0x7B][Cmd][0x00][Vx_H][Vx_L][Vy_H][Vy_L][Vz_H][Vz_L][BCC][0x7D]
```

- `Cmd`: `0x00` 正常运动，`0x01/0x02` 回充，`0x03` 对接
- 速度单位：mm/s（int16 大端序）
- `BCC = XOR(byte[0]..byte[8])`

#### 遥测帧（STM32 → A1，24 字节）

```text
[0x7B][FlagStop][Vx×2][Vy×2][Vz×2][Ax×2][Ay×2][Az×2][Gx×2][Gy×2][Gz×2][Volt×2][BCC][0x7D]
```

- 速度：int16 mm/s
- 加速度/陀螺：int16 ÷ 1000
- 电压：int16 ÷ 1000

#### 接线（A1 ↔ STM32）

| A1 | STM32 | 说明 |
|----|-------|------|
| GPIO_PIN_0 (UART0 TX) | PB11 (UART3 RX) | 指令下发 |
| GPIO_PIN_2 (UART0 RX) | PB10 (UART3 TX) | 遥测回传 |
| GND | GND | 共地（必须） |
