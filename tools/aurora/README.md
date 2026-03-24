# Aurora 伴侣工具

配合 SmartSens Aurora 应用（v2.0.0），为 A1 开发板提供 **实时数据可视化**、**底盘状态监控** 和 **固件烧录** 功能。

> **背景**: A1 开发板不支持 MIPI DSI，无法外接显示屏。图像通过上方 Type-C (USB3.0) 传输到 PC 端 Aurora 显示；
> 雷达点云、障碍信息、检测结果、底盘状态等数据通过 TCP 9090 端口传输到 PC 端伴侣工具进行可视化。

## 功能一览

| 功能 | 说明 |
|------|------|
| **三维点云** | 2D 极坐标雷达图 + 3D 散点图，实时渲染 RPLidar 360° 扫描数据 |
| **障碍区域** | 6 扇区雷达图，红色阻塞/绿色畅通，标注最近距离 |
| **检测结果** | 置信度柱状图 + 趋势曲线 + 类别分布饼图 |
| **底盘状态** | WHEELTEC 底盘 UART 连接状态、速度仪表、电池电压、人脸驱动状态 |
| **固件烧录** | 通过 CH347 SPI 接口烧录 zImage.smartsens-m1-evb 固件 |
| **深色主题** | Catppuccin Mocha 风格界面，长时间使用更护眼 |
| **演示模式** | 模拟数据驱动，无需连接硬件即可体验全部功能 |

## 系统要求

- Windows 10/11 x64
- Python 3.8+
- 已安装 [WCH CH347 驱动](https://www.wch.cn/downloads/CH343SER_EXE.html)（烧录功能需要）

## 快速开始

### 1. 安装依赖

```powershell
cd tools/aurora
pip install -r requirements.txt
```

### 2. 启动

```powershell
# 方式一：PowerShell 启动脚本（同时启动 Aurora + 伴侣工具）
.\launch.ps1

# 方式二：直接运行 Python
python aurora_companion.py
```

### 3. 演示模式（无需硬件）

```powershell
.\launch.ps1 -Demo
# 或
python aurora_companion.py --demo
```

## 详细用法

### 可视化面板

```powershell
# 默认连接 192.168.1.100:9090
python aurora_companion.py

# 指定 A1 IP 地址
python aurora_companion.py --host 10.0.0.50

# 指定端口
python aurora_companion.py --host 10.0.0.50 --port 9091

# 仅启动可视化（不尝试烧录）
python aurora_companion.py --view
```

面板布局：

```
┌──────────────────────────────────────────────────────────────┐
│           Aurora 伴侣工具 — A1 开发板实时数据可视化            │
├─────────────┬─────────────┬───────────────┬──────────────────┤
│ 雷达俯视图   │  三维点云    │   障碍区域     │  ┌────────────┐ │
│ (极坐标)    │  (3D散点)   │  (6扇区雷达)   │  │  底盘状态    │ │
├─────────────┼─────────────┼───────────────┤  │  UART连接   │ │
│ 检测置信度   │ 检测数量趋势 │   类别分布     │  │  速度/电压   │ │
│ (柱状图)    │ (折线图)    │  (饼图)       │  │  人脸驱动    │ │
├──────────────────────────────────────────┤  └────────────┘ │
│ ● 已连接 │ A1: 192.168.1.100:9090 │ 雷达: 5.0m │ 10 Hz       │
└──────────────────────────────────────────────────────────────┘
```

### 固件烧录

```powershell
# 烧录默认固件 (output/evb/zImage.smartsens-m1-evb)
python aurora_companion.py --flash

# 烧录指定固件文件
python aurora_companion.py --flash path/to/firmware.bin
```

烧录前请确认：
1. A1 开发板下方 Type-C 已连接 PC
2. SW3 开关切到 CH347 侧
3. WCH CH347 驱动已安装

### PowerShell 启动脚本

```powershell
.\launch.ps1                        # 一键启动 Aurora + 伴侣工具
.\launch.ps1 -NoAurora              # 仅启动伴侣工具
.\launch.ps1 -Demo                  # 演示模式
.\launch.ps1 -Quick                 # 快速启动（跳过依赖检查）
.\launch.ps1 -Flash                 # 固件烧录
.\launch.ps1 -Flash -Firmware "D:\fw.bin"  # 烧录指定固件
.\launch.ps1 -Host "10.0.0.50"     # 指定 A1 IP
```

## 配置文件

编辑 `config/settings.toml` 自定义连接参数：

```toml
[connection]
host = "192.168.1.100"
port = 9090
reconnect_interval = 3.0

[display]
refresh_fps = 10
radar_range = 5.0
obstacle_warn_dist = 0.5

[chassis]
max_speed = 500
voltage_min = 10.0
voltage_max = 12.6
baudrate = 115200

[flash]
firmware_path = "output/evb/zImage.smartsens-m1-evb"
aurora_exe = "Aurora-2.0.0-ciciec.13/Aurora.exe"
```

## 数据协议

A1 板端通过 `debug_data_interface` (TCP 9090) 发送换行分隔的 JSON 消息：

```json
// 点云数据
{"type":"pointcloud","points":[{"a":45.0,"d":2.3,"q":12}, ...]}

// 障碍区域
{"type":"obstacle_zones","zones":[{"angle_start":0,"angle_end":60,"min_dist":0.35,"blocked":true}, ...]}

// 检测结果
{"type":"detections","data":[{"class":"face","score":0.95,"box":[100,50,300,400]}, ...]}

// 底盘状态（WHEELTEC STM32 反馈）
{"type":"chassis_status","data":{"vx":200,"vy":0,"vz":0,"voltage":11800}}

// 人脸驱动状态
{"type":"face_drive","data":{"face_count":1,"obstacle":false,"state":"直行"}}

// 综合帧（包含以上所有）
{"type":"frame","timestamp_ms":123456,"pointcloud":[...],"detections":[...],"obstacle_zones":[...],"chassis":{...},"face_drive":{...}}
```

## 与 Aurora 的关系

```
┌──────────────┐    USB3.0 Type-C(上)    ┌───────────────────┐
│              │ ◄──────────────────────► │   Aurora.exe       │
│   A1 开发板  │    图像 + OSD 流         │   (图像/OSD显示)    │
│   SC132GS    │                          └───────────────────┘
│   RPLidar    │                          
│   YOLOv8     │    TCP 9090 (WiFi/ETH)  ┌───────────────────┐
│   SCRFD      │ ◄──────────────────────► │  伴侣工具          │
│              │    JSON 数据流            │  (点云/障碍/检测    │
│              │    含底盘+人脸驱动状态     │   /底盘状态)       │
│              │                          └───────────────────┘
│              │    USB2.0 Type-C(下)     ┌───────────────────┐
│              │ ◄──────────────────────► │  烧录工具          │
│              │    CH347 SPI Flash        │  (固件烧录)        │
└──────┬───────┘                          └───────────────────┘
       │ UART0 (GPIO PIN_0/PIN_2)
       │ 0x7B/0x7D 协议帧
┌──────┴───────┐
│  WHEELTEC    │
│  STM32 底盘  │
└──────────────┘
```

## 目录结构

```
tools/aurora/
├── aurora_companion.py      # 主入口（深色主题 + 4 视图面板）
├── launch.ps1               # PowerShell 一键启动脚本
├── requirements.txt         # Python 依赖
├── README.md                # 本文档
├── config/
│   └── settings.toml        # 连接/显示/底盘/烧录配置
└── modules/
    ├── __init__.py
    ├── tcp_client.py        # TCP 客户端（自动重连 + JSON 解析）
    ├── pointcloud_view.py   # 三维点云查看器
    ├── obstacle_view.py     # 障碍区域查看器
    ├── detection_view.py    # 检测结果查看器
    ├── chassis_view.py      # 底盘状态 & 人脸驱动面板
    └── flash_tool.py        # CH347 SPI 烧录工具
```
