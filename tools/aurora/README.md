# Aurora Windows 工具包

Aurora 是这个项目的 Windows 侧测试与联调工具集合，用于摄像头预览、采集和底盘调试。

## 目录结构

```text
tools/aurora/
├── aurora_companion.py     主工具：摄像头预览 + 底盘调试 UI（推荐）
├── serial_terminal.py      A1 调试串口终端（A1_TEST 协议）
├── qt_camera_bridge.py     QtMultimedia 摄像头桥接器
├── relay_comm.py           串口通信中继模块
├── launch.ps1              Windows 统一启动脚本
├── templates/              前端 HTML 模板
└── README.md               本文档
```

## 快速启动

### Windows

```powershell
# 使用默认配置启动（自动连接 A1 摄像头）
cd tools/aurora
.\launch.ps1
```

启动后访问 http://127.0.0.1:5801

### 启动参数

```powershell
# 默认：先启动 Aurora.exe 做相机初始化，再启动 Companion
.\launch.ps1

# 跳过启动 Aurora.exe，仅启动 Companion
.\launch.ps1 -SkipAurora

# 强制使用 A1 摄像头源
.\launch.ps1 -Source a1

# 强制使用普通 Windows 摄像头
.\launch.ps1 -Source windows

# 指定 Companion 监听端口
.\launch.ps1 -Port 5802

# 指定绑定地址
.\launch.ps1 -ListenHost 0.0.0.0

# 指定摄像头设备号
.\launch.ps1 -Device 0
```

## Aurora Companion 功能

### 摄像头功能

- **实时预览**: 支持 A1 摄像头或普通 Windows 摄像头
- **旋转控制**: 0° / 90° / 180° / 270° 旋转
- **截图功能**: 保存当前帧为图像文件
- **画廊浏览**: 查看已保存的截图
- **自动重连**: 断线后自动尝试重新连接

### 底盘调试（双模式）

#### 模式一：直连 STM32

- 直接连接 STM32 串口
- WASD 键盘控制底盘运动
- 速度调节滑块
- 遥测数据显示

#### 模式二：经由 A1（推荐）

- 通过 COM13 连接 A1 调试串口
- 使用 A1_TEST 协议与板端通信
- 功能：
  - 串口连接管理
  - Link-Test 开关控制
  - 手动运动控制（WASD / 滑块）
  - 停车按钮
  - 调试状态查询（debug_status / debug_frame）
  - 回显测试（test_echo）
  - 日志实时显示

### A1 调试终端

- 内置 serial_terminal.py 功能
- 发送任意 A1_TEST 命令
- 实时显示返回的 JSON 响应
- Hex 视图和文本视图
- 发送历史记录

## A1_TEST 协议命令一览

| 命令 | 说明 |
|------|------|
| `help` | 查看可用命令 |
| `status` | 系统状态查询 |
| `A1_TEST test_echo <msg>` | 回显测试 |
| `A1_TEST debug_status` | 查询调试状态 |
| `A1_TEST debug_frame` | 查询当前帧状态 |
| `A1_TEST link_test on` | 开启联通测试 |
| `A1_TEST link_test off` | 关闭联通测试 |
| `A1_TEST stop` | 停车 |
| `A1_TEST move <vx> <vy> <vz>` | 手动运动控制 |

## 串口扫描与连接

工具会自动扫描可用串口，优先提示：

- 包含 "usb" 描述的串口
- 默认端口：COM13（A1 调试串口）
- 支持手动选择任意串口
- 波特率默认：115200

## 烧录说明

**固件烧录不使用此工具**，请使用官方 Aurora.exe：

1. 编译生成 `output/evb/<timestamp>/zImage.smartsens-m1-evb`
2. 使用官方 Aurora.exe 工具选择镜像并烧录
3. 烧录成功后重启 A1 开发板
4. 使用本工具进行预览和调试

## 技术说明

### UTF-8 解码处理

串口终端对 UTF-8 编码数据进行了特殊处理：
- 使用 `errors="replace"` 避免解码崩溃
- 按换行符 `\n` 分割数据帧
- 处理被分包截断的 UTF-8 字符

### 数据流架构

```
A1 摄像头 → QtMultimedia Bridge → Flask Server → Web UI (MJPEG)
                                      ↑
A1 调试串口 (COM13) ──────────────────┘
         ↓
  A1_TEST 协议处理
         ↓
  底盘控制 / 状态查询
```

## 常见问题

1. **摄像头无法打开**
   - 检查 Aurora.exe 是否在占用
   - 尝试切换源模式（-Source a1 / windows）
   - 检查设备管理器中的摄像头设备

2. **串口连接失败**
   - 确认波特率为 115200
   - 检查串口号是否正确
   - 确认 A1 板端程序正在运行

3. **Link-Test 没有反应**
   - 检查 A1↔STM32 接线
   - 确认底盘电源已打开
   - 在 A1 调试终端发送 `A1_TEST debug_status` 查看状态

## 开发环境

- Python 3.9+
- PySide6 6.5.3+
- Flask
- pyserial
- opencv-python

安装依赖：
```powershell
pip install -r requirements.txt
```
