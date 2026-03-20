# A1 ROS2 项目骨架（容器内开发）

本目录是面向飞凌微 A1 + 思特威图像传感器的 ROS2 C++ 开发起点。

## 目标

- 基于 A1 图像采集和 AI 推理能力实现：
  - 高动态手势识别
  - 目标跟踪
  - 避障
  - 实时深度感知
  - 环境三维点云生成（下一阶段接入）
- 通过串口/图像输出可验证
- 覆盖三类异常：摄像头/数据、推理、资源
- 保证实时性、低功耗、帧率稳定性

## 已实现（第一阶段）

- ROS2 Jazzy 工作空间：`ros2_ws`
- 包：`a1_robot_stack`
- 节点：
  - `perception_node`：视觉感知主循环（已预留 YOLO ONNX 接口）
  - `lidar_ingest_node`：激光雷达接入与近障判断
  - `safety_supervisor_node`：异常/心跳监管与紧急停车
  - `chassis_controller_node`：底盘控制输出（预留 UART/CAN）
  - `performance_monitor_node`：FPS、丢帧、波动监控
- 启动文件：`a1_robot_stack/launch/bringup.launch.py`

## 与参考样例对应关系

参考：`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo`

- 参考样例里的 `IMAGEPROCESSOR + SCRFDGRAY + VISUALIZER` 处理链路，映射到：
  - `perception_node`：采集 + 推理 + 结果发布
  - 后续将把 `Predict()` 替换为 `YOLO ONNX + A1 NPU` 推理路径
- 参考样例的资源释放和稳定循环思路，映射到：
  - 多节点心跳 + 故障上报 + 监管节点兜底保护

## 在容器内构建运行

```bash
# 进入容器
cd /app/src/ros2_ws
source /opt/ros/jazzy/setup.bash

# 编译
colcon build --symlink-install
source install/setup.bash

# 启动
ros2 launch a1_robot_stack bringup.launch.py
```

### A1 兼容性优先启动（推荐）

为降低板端负载，建议先使用核心链路启动：

```bash
ros2 launch a1_robot_stack bringup_a1_core.launch.py
```

核心链路保留：

- perception_node
- lidar_ingest_node
- chassis_controller_node
- display_bridge_node
- safety_supervisor_node

说明：`performance_monitor_node` 可按需单独启用，不作为首轮板端稳定性验证的默认项。

### ROS 编译测试脚本

已新增脚本：`data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh`

用法：

```bash
bash data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh
```

如需先执行官方 SDK 编译再测 ROS：

```bash
bash data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh --with-sdk
```

测试报告默认输出到：`output/ros_compile_test_report.txt`

## 下一步集成点（必须完成）

- A1 摄像头灰度流接入（替换 mock 输入）
- YOLO ONNX 推理接入（A1 NPU 或板端可用推理后端）
- 雷达 + 视觉深度融合，发布真实 PointCloud2
- 底盘串口协议适配（速度命令下发与回读）
- 场景鲁棒性策略（曝光/增益/去噪自适应）
- 功耗与实时性联合优化（动态调频、线程绑核、零拷贝）

## 已补齐的硬件接口参数（2026-03）

- `perception_node`
  - `use_mock_input`：是否使用模拟输入
  - `camera_topic`：真实图像输入话题，默认 `/a1/camera/mono`
  - `camera_timeout_sec`：相机超时告警阈值
- `lidar_ingest_node`
  - `use_scan_topic`：`true` 使用 `/scan`，`false` 使用串口模式
  - `use_rplidar_sdk`：是否使用 RPLidar SDK 直接驱动雷达
  - `scan_topic`：雷达话题名
  - `serial_port` / `serial_baud` / `serial_poll_ms`：串口雷达参数
- `chassis_controller_node`
  - `use_uart_output`：启用底盘 UART 输出
  - `uart_port` / `uart_baud`：底盘串口参数
- `display_bridge_node`（新增）
  - 订阅感知与告警话题，发布 `/display/overlay_text`
  - 同步写入 `/tmp/a1_display_status.txt` 供 HDMI 控制台查看

## 代码结构速查（本轮新增/更新）

- `src/a1_robot_stack/src/perception_node.cpp`
  - 新增真实图像输入模式（`camera_topic`）和相机超时告警
  - 保留 mock 输入，便于无相机时联调
- `src/a1_robot_stack/src/lidar_ingest_node.cpp`
  - 支持话题模式（`/scan`）与串口模式双通道接入
  - 串口模式用于 CH347/UART 雷达快速打通
  - 新增 `use_rplidar_sdk` 参数，支持使用 RPLidar SDK 直接驱动雷达
- `src/a1_robot_stack/third_party/rplidar_sdk/`（新增）
  - RPLidar SDK 库，用于直接驱动 Slamtec 激光雷达
- `src/a1_robot_stack/src/chassis_controller_node.cpp`
  - 新增 UART 底盘命令输出（端口、波特率可配置）
  - 保留 `/chassis/cmd_out` 调试输出
- `src/a1_robot_stack/src/display_bridge_node.cpp`（新增）
  - 汇总感知/告警状态，输出显示叠字与状态文件
- `src/a1_robot_stack/src/safety_supervisor_node.cpp`
  - 新增 display 心跳监管，避免显示链路失效无告警
- `src/a1_robot_stack/launch/bringup_a1_core.launch.py`（新增）
  - 面向板端性能的核心节点启动编排
- `data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh`（新增）
  - 容器内 ROS 编译体检脚本，支持可选前置 SDK 编译
