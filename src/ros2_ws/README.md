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

## 下一步集成点（必须完成）

- A1 摄像头灰度流接入（替换 mock 输入）
- YOLO ONNX 推理接入（A1 NPU 或板端可用推理后端）
- 雷达 + 视觉深度融合，发布真实 PointCloud2
- 底盘串口协议适配（速度命令下发与回读）
- 场景鲁棒性策略（曝光/增益/去噪自适应）
- 功耗与实时性联合优化（动态调频、线程绑核、零拷贝）
