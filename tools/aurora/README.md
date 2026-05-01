# Aurora Windows 工具

Aurora 是 Windows 侧视频与 COM13 调试入口，用于相机预览、训练集拍照、COM13 串口终端、A1_TEST 手动命令和电脑-A1-STM32 联通测试。

直连 STM32 调试已拆到 `tools/PC/`；经 A1 中继的底盘控制已拆到 `tools/A1/`。

## 适用场景
- 预览 A1 / Windows 摄像头
- 保存原始图和训练裁剪图
- 通过 COM13 使用 A1_TEST 调试板端程序
- 执行 A1_TEST debug_status / link_test 等手动联通测试

## 目录结构

| 文件 | 作用 |
| --- | --- |
| `aurora_companion.py` | Flask + 相机预览主入口 |
| `qt_camera_bridge.py` | QtMultimedia 相机桥 |
| `serial_terminal.py` | A1_TEST 串口终端 |
| `templates/companion_ui.html` | Aurora Web UI |
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
.\launch.ps1 -Port 6211
.\launch.ps1 -ListenHost 0.0.0.0
.\launch.ps1 -Device 0
```

默认地址：`http://127.0.0.1:6201`

## 当前启动流程
- 默认会先尝试启动 `Aurora.exe` 做相机初始化
- 随后启动 `aurora_companion.py`
- Companion 就绪后会自动打开浏览器
- 如果端口被旧进程占用，启动脚本会先清理 stale Companion / Qt bridge 进程
- 启动脚本使用固定端口；默认端口不可用时会报错，不自动抢后续端口

## 核心能力

### 相机预览
- 支持 `auto`、`a1`、`windows` 三种 source
- 通过 `qt_camera_bridge.py` 提供 QtMultimedia 相机链路
- Web UI 侧使用 Companion 提供的预览流做查看和切换

### 训练集拍照
- 支持保存 `720x1280` 原图，便于回溯原始采集画面
- 支持保存 A1 主线所需的 `640×480` 训练图
- 最近拍摄缩略图在页面内显示

### A1_TEST 串口调试
- 默认串口为 `COM13`
- 默认波特率为 `115200`
- `serial_terminal.py` 负责命令发送、回显显示和文本/Hex 视图
- 终端输出对 UTF-8 / GB18030 做兼容处理，减少中文日志乱码

## A1_TEST 常用命令
- `help`
- `status`
- `A1_TEST test_echo <msg>`
- `A1_TEST debug_status`
- `A1_TEST debug_frame`
- `A1_TEST link_test on`
- `A1_TEST link_test off`

## 与其他工具的关系
- `tools/aurora/`：视频、拍照、COM13 终端、A1_TEST 手动测试，默认 `6201`
- `tools/PC/`：电脑直连 STM32 调试，默认 `6202`
- `tools/A1/`：COM13 → A1_TEST → STM32 中继控制，默认 `6203`

三套工具使用独立入口、独立启动脚本、独立页面和独立默认端口。

## 常见问题

### Companion 没画面
当前已接受流程：先打开 `Aurora.exe` 完成相机初始化，再由 Companion 接管。

### COM13 连接失败
确认串口号、115200 波特率、A1 板端程序已运行，且没有 A1 工具或其他串口工具占用 COM13。

### 页面打不开
确认 `tools/aurora/launch.ps1` 已启动，并访问 `http://127.0.0.1:6201`。

### 默认端口为什么不是 5801？
Windows 当前会保留 `5730-5929` 端口段，`5801` 会触发 `WinError 10013` / “以一种访问权限不允许的方式做了一个访问套接字的尝试”。因此默认改为 `6201`。

### 想只看 Windows 摄像头
使用 `./launch.ps1 -Source windows`。
