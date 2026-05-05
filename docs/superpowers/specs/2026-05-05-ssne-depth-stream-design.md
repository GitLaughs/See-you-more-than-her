---
name: ssne-depth-stream-design
description: Add automatic fake depth frame streaming and A1_TEST trigger in ssne_ai_demo
type: project
---

# ssne_ai_demo 深度图回传设计

## 目标

在 `ssne_ai_demo` 内重新加入一条可见的深度图回传链路：

- 板端自动周期性输出伪深度帧
- 电脑侧 Aurora 继续用现有 `A1_DEPTH_*` 协议接收并显示
- 保留 `A1_TEST depth_snapshot` 手动触发一次深度帧
- 不影响现有 RPS 分类与 STM32 底盘控制

## 范围

仅改 `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/` 下板端 demo 代码。

不改 Aurora 前端深度解析逻辑，不改串口协议格式，不改 STM32 固件。

## 现状

当前 `demo_rps_game.cpp` 已有：

- RPS 分类主循环
- `A1_TEST ping / rps_snapshot / chassis_test / move / stop`
- 通过 `ChassisController::SendVelocity()` 控底盘

当前没有找到深度图发送端实现。

Aurora 侧已有现成接收端：

- 解析 `A1_DEPTH_BEGIN`
- 解析 `A1_DEPTH_CHUNK`
- 解析 `A1_DEPTH_END`
- 用 `/api/depth/latest` 和 UI canvas 显示

## 设计

### 1. 深度帧生成

在 `demo_rps_game.cpp` 内新增一个轻量深度帧生成器，生成固定尺寸的 `u8` 灰度图。

建议尺寸：`80x60` 或 `160x120`。

生成内容：

- 使用简单 deterministic pattern
- 可以是渐变、条纹、中心亮斑、噪声叠加
- 重点是让 Aurora 端明显看到帧在变化

### 2. 输出协议

继续使用现有文本协议：

- `A1_DEPTH_BEGIN frame=... w=... h=... fmt=u8 encoding=base64 chunks=... bytes=...`
- `A1_DEPTH_CHUNK frame=... index=... data=...`
- `A1_DEPTH_END frame=...`
- 可选 `A1_DEPTH_OBJECT ...`，先不加也可以

协议必须和 Aurora `serial_terminal.py` 现有解析完全兼容。

### 3. 发送时机

做两种触发：

- **自动发送**：主循环按固定周期发一帧，比如每 500ms 或每 1s
- **手动触发**：`A1_TEST depth_snapshot` 立即发一帧

自动发送是默认路径，手动触发用于调试确认链路。

### 4. 与现有逻辑隔离

深度发送不要阻塞 RPS 分类和底盘控制：

- 深度帧生成单独函数
- 深度帧输出单独函数
- 主循环里只做轻量调用
- 不修改 `update_latest_rps_snapshot()` / `send_velocity_if_changed()` 的行为

### 5. 状态与节流

需要一个深度帧序号 `depth_frame_index`。

自动发送节流用 `std::chrono::steady_clock` 控制，避免每轮循环都发。

## 错误处理

- 深度帧生成失败时直接跳过，不影响分类和底盘控制
- `A1_TEST depth_snapshot` 未知时返回标准 debug error
- Aurora 端若未连上串口，板端照常运行，不依赖回传成功

## 测试

最少验证项：

1. 板端启动后 stdout 能看到 `A1_DEPTH_*`
2. Aurora 端 `/api/depth/latest` 能收到帧
3. UI 深度 canvas 有图
4. `A1_TEST rps_snapshot` 仍正常
5. `A1_TEST chassis_test forward/stop` 仍正常
6. 自动深度输出不影响分类日志和底盘动作

## 非目标

- 不做真实传感器深度算法
- 不做板端与分类帧严格同步
- 不改 STM32 协议
- 不新增前端 UI 样式
