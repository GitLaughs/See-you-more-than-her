# Aurora Windows 工具包

Aurora 是这个仓库的 Windows 侧测试与联调入口，不负责板端烧录。
A1 EVB 的固件烧录请使用仓库根目录下的 `Aurora-2.0.0-ciciec.16\Aurora.exe`。

## 目录

```text
tools/aurora/
├── aurora_capture.py       基础拍照工具
├── aurora_companion.py     伴侣工具：源切换、预览、YOLOv8、底盘调试
├── chassis_comm.py         PC 直连 STM32 底盘串口后端
├── relay_comm.py           经由 A1 的 COM13 / A1_TEST 控制兼容层
├── serial_terminal.py      COM13 / A1 调试串口终端
├── qt_camera_bridge.py     QtMultimedia 摄像头桥
├── launch.ps1              统一启动脚本
└── templates/              前端页面
```

## 启动模式

- `launch.ps1` 默认启动 `aurora_companion.py`
- 默认输入源为 `auto`，优先走 QtMultimedia 摄像头桥
- `-Source a1` 优先打开 `Smartsens-FlyingChip-A1-1`
- `-Source windows` 使用普通 Windows 摄像头
- `-SkipAurora` 只启动 Companion，不拉起 Aurora.exe

`launch.ps1` 只用于 Windows 侧测试与联调，不做烧录。

## 当前界面能力

- Windows 纯预览：原始摄像头输入，不叠检测框
- Windows 本地 YOLOv8：ONNX / PT 推理 + OSD 叠框，支持在页面内按原始文件名输入或切换模型
- A1 摄像头输入：参考 Aurora 的 QtMultimedia 视频流方式枚举 `Smartsens-FlyingChip-A1-1`，失败时回退 OpenCV
- 预览同步：只保留最新帧、MJPEG 禁缓存，支持网页端 0/90/180/270 度旋转
- 串口扫描：刷新底盘串口列表并提示新增端口
- 联通测试：直连页使用 PC ↔ STM32；经由 A1 页统一使用 COM13 发送 `A1_TEST debug_status`
- 经由 A1 底盘控制：前端共享控制区通过 COM13 发送 `A1_TEST move <vx> <vy> <vz>` / `A1_TEST stop`
- A1 调试终端：所有 `A1_TEST` 调试命令统一通过 COM13 发送，不再使用旧 TCP 测试桥

## 烧录方式

1. 编译出 `output/evb/latest/zImage.smartsens-m1-evb`
2. 打开 `Aurora-2.0.0-ciciec.16\Aurora.exe`
3. 选择镜像并执行烧录
4. 烧录完成后重启板卡，再用 Aurora 或板端脚本验证

## 相关说明

- `burn_log.txt` 可用于查看烧录日志
- `aurora_companion.py` 使用 `127.0.0.1` 作为本地访问口径
- PC 直连底盘协议由 `chassis_comm.py` 处理；经由 A1 的调试和底盘命令统一走 `serial_terminal.py` / COM13
