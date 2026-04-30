# A1 Windows 工具

A1 工具是 Windows 侧 COM13 → A1_TEST → STM32 中继控制入口，不包含摄像头、直连 STM32 端口选择或 ROS 调试。

## 适用场景
- 检查 COM13 / A1_TEST 会话
- 发送 A1_TEST 手动命令
- 经 A1 中继做底盘联通测试
- 经 A1 中继做前后左右/停止控制
- 查看 A1_TEST / 串口回传日志

## 启动

```powershell
cd tools/A1
.\launch.ps1
```

默认地址：`http://127.0.0.1:6203`

如需改端口：

```powershell
.\launch.ps1 -Port 6213
```

## 边界
- 不启动 Qt camera bridge
- 不探测摄像头
- 不提供直连 STM32 控制
- 不提供 ROS 调试

## 常见问题

### 默认端口为什么不是 5803？
Windows 当前会保留 `5730-5929` 端口段，`5803` 会触发 `WinError 10013` / “以一种访问权限不允许的方式做了一个访问套接字的尝试”。因此默认改为 `6203`。

### COM13 连接失败
确认 A1 板端程序已运行、串口号为 COM13、波特率为 115200，且没有 Aurora 或其他串口工具占用该端口。
