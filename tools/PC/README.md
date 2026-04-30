# PC Windows 工具

PC 工具是 Windows 侧电脑直连 STM32 与 ROS 调试入口，不包含摄像头、COM13 终端或 A1 中继控制。

## 适用场景
- 电脑 USB 串口直连 STM32 UART
- 直连链路联通测试
- 前后左右/停止控制
- STM32 遥测查看
- ROS 工作区状态与节点启停调试

## 启动

```powershell
cd tools/PC
.\launch.ps1
```

默认地址：`http://127.0.0.1:6202`

如需改端口：

```powershell
.\launch.ps1 -Port 6212
```

## 边界
- 不启动 Qt camera bridge
- 不探测摄像头
- 不使用 COM13 / A1_TEST 终端
- 不提供 A1 中继控制

## 常见问题

### 默认端口为什么不是 5802？
Windows 当前会保留 `5730-5929` 端口段，`5802` 会触发 `WinError 10013` / “以一种访问权限不允许的方式做了一个访问套接字的尝试”。因此默认改为 `6202`。

### ROS 节点启动失败
先确认 `src/ros2_ws/` 已构建，且 ROS 环境可用。PC 工具只负责调试入口，不替代 ROS 工作区构建。
