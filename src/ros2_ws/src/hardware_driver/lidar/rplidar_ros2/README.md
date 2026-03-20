# rplidar_ros2

这是一个基于官方 Slamtec RPLIDAR SDK 的 ROS2 Jazzy 驱动封装，目标是直接在当前工作区里发布 `sensor_msgs/msg/LaserScan`。

## 依赖

- ROS2 Jazzy
- `rclcpp`
- `sensor_msgs`
- 官方 `rplidar_sdk`

## 目录说明

- `src/rplidar_ros2_node.cpp`：ROS2 节点入口
- `config/rplidar.yaml`：默认参数
- `launch/rplidar_launch.py`：启动文件

## 参考源码

官方 ROS1 仓库已保留在同级目录的 `rplidar_ros_upstream`，仅用于对照上游文档和参数，不参与 ROS2 编译。

## 构建方式

这个包会直接编译 `../rplidar_ros_upstream/sdk` 下的官方 SDK 源码。

## 启动方式

```bash
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install
source install/setup.bash
ros2 launch rplidar_ros2 rplidar_launch.py
```
