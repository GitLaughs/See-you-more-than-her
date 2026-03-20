# SSNE AI Demo

这是一个基于 SmartSens SDK 的独立 C++ 人脸检测示例。当前仓库把它作为项目的基础开发入口，用来承载视觉、雷达和后续业务模块的最小骨架。

## 目录结构

```text
ssne_ai_demo/
├── demo_face.cpp
├── include/
│   ├── common.hpp
│   ├── utils.hpp
│   ├── osd-device.hpp
│   ├── project_paths.hpp
│   ├── project_flow.hpp
│   └── lidar_sdk_adapter.hpp
├── src/
│   ├── utils.cpp
│   ├── pipeline_image.cpp
│   ├── scrfd_gray.cpp
│   ├── osd-device.cpp
│   ├── project_flow.cpp
│   └── lidar_sdk_adapter.cpp
├── app_assets/
│   ├── colorLUT.sscl
│   └── models/
├── cmake_config/
│   └── Paths.cmake
├── scripts/
│   └── run.sh
└── CMakeLists.txt
```

## 代码分层

### 入口层

- [demo_face.cpp](demo_face.cpp)：只保留 `main()`，负责启动 `FaceDetectionDemoApp`

### 流程层

- [include/project_flow.hpp](include/project_flow.hpp)
- [src/project_flow.cpp](src/project_flow.cpp)

这一层负责：

- 初始化 SSNE
- 初始化图像采集
- 执行人脸检测
- 做坐标转换和 OSD 绘制
- 处理退出和资源释放

### 配置层

- [include/project_paths.hpp](include/project_paths.hpp)

这一层集中管理：

- 原图尺寸
- 裁剪尺寸
- 检测模型输入尺寸
- 模型路径
- 人脸阈值

### 扩展层

- [include/lidar_sdk_adapter.hpp](include/lidar_sdk_adapter.hpp)
- [src/lidar_sdk_adapter.cpp](src/lidar_sdk_adapter.cpp)

这一层是给雷达扩展预留的独立适配器，后续可以接 `rplidar_sdk`、`ydlidar` 或其他串口雷达。

## 运行方式

在容器内执行：

```powershell
docker exec A1_Builder bash -lc "cd /app/a1_ssne_ai_demo && bash scripts/run.sh"
```

如果你要手工编译，通常会在 SDK 环境里先加载路径配置，再执行 CMake 构建。

## 与本项目的关系

当前项目已经删除了自研 ROS 包，新的开发顺序建议如下：

1. 先把这个 demo 跑通
2. 再把雷达 SDK 接到 `lidar_sdk_adapter`
3. 再按需要决定是否重建 ROS2 包

这样可以先把视觉链路和雷达链路拆开验证，避免一开始就把系统耦合得太重。

## RPLidar 接入建议

如果你要接 `https://github.com/Slamtec/rplidar_sdk.git`，建议把它放到“使用它的模块”旁边，而不是放在 `data/A1_SDK_SC132GS` 根目录。

推荐路径：

- `src/a1_ssne_ai_demo/third_party/rplidar_sdk/`

接入时的常见动作是：

1. 在 CMake 里把 `rplidar_sdk/include` 加到头文件路径
2. 把 `rplidar_sdk/src` 编进目标库或单独静态库
3. 在适配层里通过 `RPlidarDriver::CreateDriver()` 打开串口
4. 调用 `connect()`、`startScan()`、`grabScanData()` 读取数据
5. 结束时调用 `stop()`、`disconnect()`、`DisposeDriver()`

## 后续扩展建议

- 业务配置放到 `include/project_paths.hpp`
- 应用流程放到 `include/project_flow.hpp` 和 `src/project_flow.cpp`
- 新外设独立成一个 `*_adapter.hpp/.cpp`
- 不要把第三方库源码散落在业务目录顶层
