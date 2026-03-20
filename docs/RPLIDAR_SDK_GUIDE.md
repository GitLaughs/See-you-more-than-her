# RPLidar SDK 接入指南

这个仓库当前已经删除了自研 ROS 包，因此 RPLidar SDK 最适合先作为独立 C++ 适配层接到 SDK demo 里，再决定后续是否重新封装成 ROS 节点。

## 建议放置位置

不要把 `rplidar_sdk` 直接扔到 `data/A1_SDK_SC132GS` 根目录下。建议按用途放在对应模块旁边：

- 先做 SDK demo 适配时：`src/a1_ssne_ai_demo/third_party/rplidar_sdk/`
- 后面重建 ROS 驱动时：`src/ros2_ws/src/<your_lidar_pkg>/third_party/rplidar_sdk/`

## 基本使用方式

如果你直接使用 Slamtec 官方 SDK，常见流程是：

1. 从仓库拉取源码
2. 把 `sdk/include` 加到编译头文件路径
3. 把 `sdk/src` 编译进目标库或直接链接
4. 用 `RPlidarDriver::CreateDriver()` 创建驱动对象
5. 调用 `connect()` 打开串口
6. 调用 `startScan()` 或 `startScanExpress()` 开始扫描
7. 调用 `grabScanData()` / `grabScanDataHq()` 读取点云
8. 结束后调用 `stop()`、`disconnect()`，再释放驱动对象

## 代码骨架建议

仓库里已经预留了一个适配层入口：

- [src/a1_ssne_ai_demo/include/lidar_sdk_adapter.hpp](../src/a1_ssne_ai_demo/include/lidar_sdk_adapter.hpp)
- [src/a1_ssne_ai_demo/src/lidar_sdk_adapter.cpp](../src/a1_ssne_ai_demo/src/lidar_sdk_adapter.cpp)

你可以先把串口号、波特率和扫描结果封装到这个适配层里，然后在上层业务里只依赖 `LidarSample` 这种轻量数据结构。

## 与本项目的关系

当前项目的优先路径是：

1. 先把 SDK demo 跑通
2. 再把人脸检测、雷达、后续控制逻辑拆成独立模块
3. 最后再决定是否重新落回 ROS2

这样做的好处是，雷达 SDK 和视觉 demo 都能先独立验证，不会互相卡住。
