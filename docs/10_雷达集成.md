# RPLidar SDK 接入指南

本文档说明如何将 Slamtec RPLidar C++ SDK 接入 A1 项目，包括独立 C++ 适配层（`ssne_vision_demo`）和 ROS2 驱动节点两种路径。

## 当前状态

RPLidar SDK 已作为独立适配层集成到 `src/a1_ssne_ai_demo/` 中：

- SDK 路径：`src/a1_ssne_ai_demo/third_party/rplidar_sdk/`
- 适配层：`src/a1_ssne_ai_demo/include/lidar_sdk_adapter.hpp`
- 实现：`src/a1_ssne_ai_demo/src/lidar_sdk_adapter.cpp`
- ROS2 驱动包：`src/ros2_ws/src/hardware_driver/lidar/rplidar_ros2/`

## 适配层接口

```cpp
#include "lidar_sdk_adapter.hpp"

// 数据结构
struct LidarSample {
    float   angle_deg;    // 角度，0~360°
    float   distance_m;   // 距离，单位米
    uint8_t quality;      // 信号质量，0~15
};

// 使用示例
ssne_demo::RplidarSdkAdapter lidar;
lidar.Init("/dev/ttyUSB0", 115200);
lidar.Start();

std::vector<ssne_demo::LidarSample> samples;
if (lidar.GrabScan(samples)) {
    for (auto& s : samples) {
        // 处理点云数据
    }
}

lidar.Stop();
lidar.Release();
```

## CMake 集成

`ssne_vision_demo` 的 CMakeLists.txt 已包含 RPLidar SDK 构建配置：

```cmake
set(RPLIDAR_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rplidar_sdk/sdk)
include_directories(${RPLIDAR_SDK_DIR}/include)
include_directories(${RPLIDAR_SDK_DIR}/src)

file(GLOB RPLIDAR_SDK_SRC
    "${RPLIDAR_SDK_DIR}/src/arch/linux/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/dataunpacker/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/dataunpacker/unpacker/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/hal/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/*.cpp"
)
```

## 障碍感知逻辑

`ssne_vision_demo` 在每帧叠加障碍计算：

```
1. 获取全圈扫描点云（360° × N个点）
2. 将前方 ±30° 范围内的点按 6 个扇区分组
3. 各扇区取最近距离值
4. 若任一扇区 min_dist < 0.5m → 触发 Layer 2 红色警告覆盖
5. 通过 TCP 9090 向 Aurora 推送 obstacle_zones 数据
```

障碍区域数据结构（JSON）：
```json
{
  "obstacle_zones": [
    {"angle_start": -30, "angle_end": -10, "min_dist": 1.20, "blocked": false},
    {"angle_start": -10, "angle_end":  10, "min_dist": 0.42, "blocked": true},
    {"angle_start":  10, "angle_end":  30, "min_dist": 0.95, "blocked": false}
  ]
}
```

## ROS2 雷达节点

RPLidar ROS2 驱动包：`src/ros2_ws/src/hardware_driver/lidar/rplidar_ros2/`

```bash
# 启动 RPLidar 节点
ros2 launch rplidar_ros2 rplidar_launch.py

# 查看点云话题
ros2 topic echo /scan
```

默认参数（可在 launch 文件中覆盖）：

| 参数 | 默认值 | 说明 |
|---|---|---|
| `serial_port` | `/dev/ttyUSB0` | 串口设备 |
| `serial_baudrate` | `115200` | 波特率 |
| `frame_id` | `laser` | 坐标系 |
| `scan_mode` | `Standard` | 扫描模式 |

## 板端串口配置

A1 开发板上的 RPLidar 通过 CH347 USB 转串口接入，设备节点通常为 `/dev/ttyUSB0`。

若出现权限问题：

```bash
sudo chmod 666 /dev/ttyUSB0
# 或将用户加入 dialout 组
sudo usermod -aG dialout $USER
```

## 扩展指引

若要将适配层升级为 ROS2 节点，建议参考以下步骤：

1. 在 `src/ros2_ws/src/` 下新建包（如 `a1_lidar_bridge`）
2. 将 `lidar_sdk_adapter.hpp/.cpp` 复制为依赖
3. 创建 ROS2 节点，在循环中调用 `GrabScan()` 并发布 `sensor_msgs/LaserScan`
4. 添加 `package.xml` 和 `CMakeLists.txt`
5. 用 colcon 构建
