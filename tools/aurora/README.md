# Aurora Windows 工具

Aurora 是仓库里的 Windows 侧联调入口，用于相机预览、A1 串口调试、STM32 底盘控制和 ROS 辅助调试。

## 适用场景
- 预览 A1 / Windows 摄像头
- 通过 COM13 使用 A1_TEST 调试板端程序
- 直连 STM32 做底盘控制
- 通过 ROS bridge 查看和下发底盘相关状态

## 目录结构

| 文件 | 作用 |
| --- | --- |
| `aurora_companion.py` | Flask + PySide6 主入口 |
| `qt_camera_bridge.py` | QtMultimedia 相机桥 |
| `serial_terminal.py` | A1_TEST 串口终端 |
| `relay_comm.py` | PC → A1_TEST → STM32 relay 通道 |
| `chassis_comm.py` | PC 直连 STM32 控制 |
| `ros_bridge.py` | ROS 侧状态与控制桥 |
| `templates/companion_ui.html` | 单页 Web UI |
| `launch.ps1` | Windows 启动脚本 |

## 安装与启动

```powershell
cd tools/aurora
pip install -r requirements.txt
.\launch.ps1
```

常用启动方式：

```powershell
.\launch.ps1 -SkipAurora
.\launch.ps1 -Source a1
.\launch.ps1 -Source windows
.\launch.ps1 -Port 5802
.\launch.ps1 -ListenHost 0.0.0.0
.\launch.ps1 -Device 0
```

默认地址：`http://127.0.0.1:5801`

## 当前启动流程
- 默认会先尝试启动 `Aurora.exe` 做相机初始化
- 随后启动 `aurora_companion.py`
- Companion 就绪后会自动打开浏览器
- 如果端口被旧进程占用，启动脚本会先清理 stale Companion / Qt bridge 进程

## 核心能力

### 相机预览
- 支持 `auto`、`a1`、`windows` 三种 source
- 通过 `qt_camera_bridge.py` 提供 QtMultimedia 相机链路
- Web UI 侧使用 Companion 提供的预览流做查看和切换

### A1_TEST 串口调试
- 默认串口为 `COM13`
- 默认波特率为 `115200`
- `serial_terminal.py` 负责命令发送、回显显示和文本/Hex 视图
- 终端输出对 UTF-8 / GB18030 做兼容处理，减少中文日志乱码

### 底盘联调
- 支持手动运动控制、停车、状态查询
- 可用于直接验证 STM32 底盘响应，也可经由 A1 板端链路联调

### ROS bridge
- `ros_bridge.py` 负责 ROS 工作区环境探测、状态桥接和控制转发
- 该路径用于 ROS 辅助调试，不代表 ROS2 已经是板端默认运行栈

## 双控制路径

### 直连 STM32
PC 串口 → STM32 UART。主要由 `chassis_comm.py` 负责，适合直接验证底盘运动与遥测。

### 经由 A1
PC COM13 → A1_TEST → A1 UART0 → STM32 UART3。主要由 `serial_terminal.py` 与 `relay_comm.py` 负责，适合联调板端程序和底盘联动。

## A1_TEST 常用命令
- `help`
- `status`
- `A1_TEST test_echo <msg>`
- `A1_TEST debug_status`
- `A1_TEST debug_frame`
- `A1_TEST link_test on`
- `A1_TEST link_test off`
- `A1_TEST stop`
- `A1_TEST move <vx> <vy> <vz>`

## 与主仓库的关系
- Aurora 负责 Windows 侧预览、串口调试和联调
- Aurora 不负责固件烧录本身
- 固件仍应先通过 `scripts/build_complete_evb.sh` 生成 `zImage.smartsens-m1-evb`
- 烧录使用官方 `Aurora.exe` 或其他板端烧录流程

## 常见问题

### Companion 没画面
当前已接受流程：先打开 `Aurora.exe` 完成相机初始化，再由 Companion 接管。

### COM13 连接失败
确认串口号、115200 波特率、A1 板端程序已运行。

### 页面打不开
确认 `tools/aurora/launch.ps1` 已启动，并访问 `http://127.0.0.1:5801`。

### 想只看 Windows 摄像头
使用 `./launch.ps1 -Source windows`。
