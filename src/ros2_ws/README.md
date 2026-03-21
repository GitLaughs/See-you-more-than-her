# A1 ROS2 工作区（容器内开发）

这个目录承载当前仓库的 ROS2 上游源码集合，已经不再保留本地自研的 ROS 包。

## 当前源码来源

- `base_control_ros2`：底盘驱动与控制
- `hardware_driver`：相机与雷达驱动
- `bingda_ros2_demos`：导航、视觉和 VSLAM 例程
- `ncnn_ros2`：NCNN 视觉推理示例
- `depend_pkg`：ROS2 依赖包集合，包含 `slam_gmapping`
- `object_information_msgs_ros2`：目标检测消息类型
- `hardware_driver/lidar/rplidar_ros2`：基于官方 Slamtec SDK 的 ROS2 雷达封装

## 目录约定

- 上游仓库直接放在 `ros2_ws/src/` 下，colcon 会递归发现其中的包
- 不再保留自研的 `a1_robot_stack`

## 容器内构建

```bash
cd /app/src/ros2_ws
rm -rf build install log
set +u
source /opt/ros/jazzy/setup.bash
set -u
colcon build --symlink-install
source install/setup.bash
```

## 常用 launch 入口

- `ros2 launch base_control_ros2 base_control.launch.py`
- `ros2 launch robot_navigation_ros2 robot_lidar.launch.py`
- `ros2 launch robot_vslam_ros2 robot_rgbd_lidar.launch.py`
- `ros2 launch robot_vision_ros2 camera.launch.py`
- `ros2 launch ncnn_ros2 yolov8_ros.launch.py`
- `ros2 launch astra_camera camera.launch.py`
- `ros2 launch rplidar_ros2 rplidar_launch.py`

## ROS 编译测试脚本

脚本位置：`data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh`

用法：

```bash
bash data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh
```

如需先执行官方 SDK 编译再测 ROS：

```bash
bash data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh --with-sdk
```

该脚本会在容器内对当前工作区的整套上游 ROS2 源码执行 `colcon build`，并把报告写到 `output/ros_compile_test_report.txt`。

## 代码职责速查

- `base_control_ros2`：底盘控制和基础运动接口
- `hardware_driver`：`astra_camera`、`astra_camera_msgs`、`sllidar_ros2`、`ydlidar_ros2_driver` 等驱动包
- `bingda_ros2_demos`：`robot_navigation_ros2`、`robot_vision_ros2`、`robot_vslam_ros2`
- `ncnn_ros2`：视觉检测和推理示例
- `depend_pkg`：`slam_gmapping` 等第三方依赖源码
- `object_information_msgs_ros2`：检测结果消息定义
- `hardware_driver/lidar/rplidar_ros2`：RPLidar ROS2 包，直接发布 `scan`

## RPLidar SDK 放置建议

如果要接入 Slamtec 的 `rplidar_sdk`，建议把它放在“使用它的驱动包”旁边，而不是放到 `data/A1_SDK_SC132GS` 根目录下。推荐路径示例：

- `src/a1_ssne_ai_demo/third_party/rplidar_sdk/`：如果你先做独立 demo 适配
- `src/ros2_ws/src/<your_lidar_package>/third_party/rplidar_sdk/`：如果后续再接回 ROS 包

使用方式上，通常是：

1. 在 CMake 里把 `rplidar_sdk/include` 加到 `include_directories()`
2. 把 `rplidar_sdk/src` 编进库或者链接到目标
3. 在代码里通过 `RPlidarDriver::CreateDriver()` 打开串口，调用 `connect()`、`startScan()`、`grabScanData()`、`disconnect()`

当前仓库里已经删除了自研 ROS 包，因此如果你要先在 SDK demo 里接雷达，建议先做成独立的 C++ 适配层，再决定后面是否重建 ROS 节点。

官方 `rplidar_ros` ROS1 仓库保留在 `hardware_driver/lidar/rplidar_ros_upstream`，仅作参考，不参与 colcon 构建。
