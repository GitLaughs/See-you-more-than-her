# Issue Planning

本文件用于规划三人分工与后续维护。

## 当前仓库 Issue 对齐（2026-03-20）

- 已存在并建议复用：
  - #5 `[B] 避障模块 + 5路GPIO底盘驱动`：承接底盘 UART/GPIO 协议与实车闭环
  - #10 `[C] 前端视频与点云可视化模块`：承接显示输出与状态叠加
  - #11 `[维护] 每周集成构建与回归检查`：承接 ROS bringup 联调回归
- 建议新增：
  - `[集成] A1硬件接口打通：显示屏/雷达/ROS/底盘`（总集成 issue）
  - `[驱动] A1雷达串口协议适配（CH347/UART）`（协议细化 issue）

## 本次代码落地对应关系

- 显示屏输出：
  - 新增 `display_bridge_node`，统一输出 `/display/overlay_text` 和 `/tmp/a1_display_status.txt`
  - 安全监管新增 display 心跳，确保显示链路掉线可报警
- 激光雷达接入：
  - `lidar_ingest_node` 支持两种模式：`/scan` 话题模式、串口 ASCII 距离模式
  - 串口参数可配：端口、波特率、轮询周期
- ROS 操作系统接入：
  - `perception_node` 支持从 `/a1/camera/mono` 订阅真实图像输入
  - `bringup.launch.py` 增加硬件参数，默认走真实输入链路
- 小车底盘接入：
  - `chassis_controller_node` 新增 UART 输出能力，命令帧限制在 32 字节以内
  - 保留 `/chassis/cmd_out` 话题用于仿真与调试镜像输出

## 建议分工（与 issue 对齐）

- 成员 A（视觉/ROS）：
  - 完成 `/a1/camera/mono` 实际驱动发布与帧率稳定
  - 在 #3 和总集成 issue 中同步相机链路验证结论
- 成员 B（底盘/串口）：
  - 在 #5 完成 `VX/WZ` 协议与真实底盘驱动映射
  - 校准 GPIO_PIN_0(TX0) / GPIO_PIN_2(RX0) 复用与波特率
- 成员 C（显示/融合）：
  - 在 #10 完成 HDMI 显示链路和 overlay 样式
  - 串联雷达结果与告警在显示层的可视化
- 维护任务（全员轮值）：
  - 在 #11 固化 bringup 回归脚本与最小验收标准

## 人员分工（建议）

- 成员 A（视觉）：YOLOv8 训练、YOLO 视觉、手势识别
- 成员 B（控制）：避障、GPIO 驱动底盘、跟踪联动控制
- 成员 C（感知与展示）：深度感知、3D 点云、前端显示

## 里程碑建议

- Sprint 1: 模型与感知输入打通
- Sprint 2: 控制联动与避障闭环
- Sprint 3: 点云/深度/前端可视化
- Sprint 4: 稳定性与性能优化
