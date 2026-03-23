# A1 ROS2 工作区

基于 ROS2 Jazzy 的 A1 机器人软件栈，覆盖底盘控制、传感器驱动、导航和视觉推理。

## 包概览

| 包目录 | 功能 |
|---|---|
| `base_control_ros2` | STM32 AKM 小车 UART 驱动与运动控制 |
| `hardware_driver` | 激光雷达（RPLidar）驱动及其他传感器接口 |
| `object_information_msgs_ros2` | 目标检测消息类型定义 |
| `a1_robot_stack` | 硬件整合 bringup（坐标系、传感器联调） |

## 目录约定

```text
ros2_ws/
├── src/
│   ├── a1_robot_stack/                 # 硬件整合与 bringup
│   ├── base_control_ros2/              # STM32 AKM 小车底盘控制
│   ├── hardware_driver/                # 传感器驱动（激光雷达等）
│   │   └── lidar/rplidar_ros2/         # RPLidar ROS2 封装
│   └── object_information_msgs_ros2/   # 检测消息类型
└── README.md
```

colcon 会递归发现 `src/` 下所有包，无需手动配置。

## 容器内快速构建

```bash
# 进入容器
docker exec -it A1_Builder bash

# 首次安装依赖
apt-get update && apt-get install -y \
  ros-jazzy-camera-info-manager ros-jazzy-cv-bridge \
  ros-jazzy-image-transport ros-jazzy-tf2-ros \
  ros-jazzy-rclcpp-components libusb-1.0-0-dev \
  libuvc-dev nlohmann-json3-dev

# 全量构建
cd /app/src/ros2_ws
rm -rf build install log
set +u && source /opt/ros/jazzy/setup.bash && set -u
colcon build --symlink-install

# 使用构建结果
source install/setup.bash
```

或使用项目脚本（推荐）：

```bash
bash scripts/build_ros2_ws.sh --clean
```

## 常用 launch 入口

| 功能 | 命令 |
|---|---|
| 底盘控制 | `ros2 launch base_control_ros2 base_control.launch.py` |
| 雷达 | `ros2 launch rplidar_ros2 rplidar_launch.py` |
| A1 完整 bringup | `ros2 launch a1_robot_stack bringup.launch.py` |

## ROS 编译测试

```bash
# 仅编译测试（生成报告）
bash data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh

# SDK + ROS 全链路测试
bash data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh --with-sdk
```

报告输出：`output/ros_compile_test_report.txt`

## 话题速查

| 话题 | 类型 | 发布节点 |
|---|---|---|
| `/a1/camera/mono` | `sensor_msgs/Image` | 相机驱动 |
| `/scan` | `sensor_msgs/LaserScan` | RPLidar 驱动 |
| `/cmd_vel` | `geometry_msgs/Twist` | 导航/手柄控制 |
| `/chassis/cmd_out` | 自定义 | 底盘控制节点 |
| `/object_information` | `object_information_msgs/ObjectInfo` | NCNN 推理节点 |
| `/display/overlay_text` | `std_msgs/String` | 显示桥节点 |

## 与 SSNE Demo 的关系

`src/a1_ssne_ai_demo` 中的 `ssne_vision_demo` 通过 TCP 端口 `9090` 向外推送感知数据，
Aurora 调试工具或自定义 ROS 桥节点可订阅该数据流并转为 ROS 话题。

详细说明见 [src/a1_ssne_ai_demo/README.md](../a1_ssne_ai_demo/README.md)。

## 常见问题

- `AMENT_TRACE_SETUP_FILES: unbound variable`：在 `source` 前后添加 `set +u` / `set -u`。
- 编译报缺少依赖：参考上方 `apt-get install` 命令补充。
- colcon 找不到包：确认包目录下有 `package.xml` 和 `CMakeLists.txt`（或 `setup.py`）。
