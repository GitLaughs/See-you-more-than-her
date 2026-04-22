# Aurora Windows 工具包

Aurora 是这个仓库的 Windows 侧测试与联调入口，不负责板端烧录。
A1 EVB 的固件烧录请使用仓库根目录下的 `Aurora-2.0.0-ciciec.14\Aurora.exe`。

## 目录

```text
tools/aurora/
├── aurora_capture.py       基础拍照工具
├── aurora_companion.py     伴侣工具：源切换、预览、YOLOv8、底盘调试
├── a1_viewer.py            A1 板端 OSD 结果查看器
├── stm32_port_probe.py     Windows 串口扫描与联通测试
├── chassis_comm.py         底盘串口通信后端
├── launch.ps1              统一启动脚本
└── templates/              前端页面
```

## 启动模式

- `launch.ps1` 默认启动 `aurora_companion.py`
- `launch.ps1 -Mode a1` 启动 A1 Viewer
- `launch.ps1 -Mode viewer` 启动 A1 视频查看器
- `launch.ps1 -Mode probe` 启动串口探测器
- `launch.ps1 -Mode capture` 启动基础拍照工具

`launch.ps1` 只用于 Windows 侧测试与联调，不做烧录。

## 当前界面能力

- Windows 纯预览：原始摄像头输入，不叠检测框
- Windows 本地 YOLOv8：ONNX / PT 推理 + OSD 叠框，支持在页面内按原始文件名输入或切换模型
- A1 板端预览：用于查看板端输出的带框/OSD 画面
- 串口扫描：刷新底盘串口列表并提示新增端口
- 联通测试：A1 ↔ STM32 发送零速度测试帧并读取遥测

## 烧录方式

1. 编译出 `output/evb/latest/zImage.smartsens-m1-evb`
2. 打开 `Aurora-2.0.0-ciciec.14\Aurora.exe`
3. 选择镜像并执行烧录
4. 烧录完成后重启板卡，再用 Aurora 或板端脚本验证

## 相关说明

- `burn_log.txt` 可用于查看烧录日志
- `aurora_companion.py` 使用 `127.0.0.1` 作为本地访问口径
- 端口扫描和底盘协议都由 `chassis_comm.py` 处理
