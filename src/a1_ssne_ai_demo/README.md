# SSNE AI Demo

这是一个基于 SmartSens SDK 的独立 C++ 人脸检测示例，也是当前项目里最小、最直接的可编译入口。仓库已经把官方 RPLidar SDK 接进了这个 demo，因此它现在同时承担：

- 人脸检测主流程
- RPLidar 串口采集适配
- 官方 SDK 集成验证
- 后续新外设接入的最小骨架

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
├── third_party/
│   └── rplidar_sdk/
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
- 读取一次雷达扫描
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
- 雷达串口与波特率

### 扩展层

- [include/lidar_sdk_adapter.hpp](include/lidar_sdk_adapter.hpp)
- [src/lidar_sdk_adapter.cpp](src/lidar_sdk_adapter.cpp)

这一层是给雷达扩展预留的独立适配器。当前已经接入官方 RPLidar SDK，后续也可以按同样模式扩展到其他串口雷达。

## 推荐编译环境

- Windows + Docker Desktop
- 容器名：`A1_Builder`
- SDK 根目录挂载到 `/app/smartsens_sdk`
- 源码根目录挂载到 `/app/src`

## 详细编译手册

如果你要按当前仓库的真实目录结构从头编译，请优先查看仓库级手册：

- [docs/BUILD.md](../../docs/BUILD.md)

这里先给出最关键的结论：

- SDK demo 的产物是在 `data/A1_SDK_SC132GS/smartsens_sdk/output/images/` 下生成的
- 真正可写入主板的镜像文件名是 `zImage.smartsens-m1-evb`
- 这里的 `-evb` 是文件名后缀，不是 `.evb` 扩展名
- 仓库新增的全量脚本是 [scripts/build_src_all.sh](../../scripts/build_src_all.sh)
- 如果你只改了 demo 代码，可以直接用 [scripts/build_incremental.sh](../../scripts/build_incremental.sh) 只编译这一块

推荐执行方式：

```powershell
docker exec A1_Builder bash -lc "cd /app/src; chmod +x scripts/build_src_all.sh && scripts/build_src_all.sh"
```

如果你只想单独编译 SDK demo，则优先用增量脚本；要保留 SDK 原有行为，也可以直接调用官方脚本：

```powershell
docker run --rm -v $(pwd):/workspace -w /workspace a1_builder bash -lc "bash scripts/build_incremental.sh sdk ssne_ai_demo"
```

或者继续使用 SDK 原有脚本：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh"
```

如果容器还没启动，可以先执行：

```powershell
docker compose -f docker/docker-compose.yml up -d
docker compose -f docker/docker-compose.yml ps
```

## 编译手册

### 1. 进入容器

所有编译都建议在 `A1_Builder` 容器里完成：

```powershell
docker exec -it A1_Builder bash
```

如果你想直接执行单条命令，也可以使用：

```powershell
docker exec A1_Builder bash -lc "cd /app/src && pwd"
```

### 2. 编译 SDK demo

demo 的实际工程路径是：

```text
/app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk
```

建议的完整构建命令如下：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh 2>&1 | tee /app/output/a1_sc132gs_build.log"
```

构建成功后，关键结果是：

- `ssne_ai_demo` 被编译并安装到目标输出目录
- 日志里出现 `Built target ssne_ai_demo`

你也可以查看本次构建留存的日志：

- [output/a1_sc132gs_build.log](../../output/a1_sc132gs_build.log)

### 3. 编译 ROS2 工作区

ROS2 工作区路径是：

```text
/app/src/ros2_ws
```

推荐的完整构建命令如下：

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; rm -rf build install log; set +u; source /opt/ros/jazzy/setup.bash; set -u; colcon build --symlink-install 2>&1 | tee /app/output/ros2_colcon_build_rplidar.log"
```

构建成功后，关键结果是：

- `colcon build` 结束并输出 `Summary: 15 packages finished`
- `astra_camera`、`rplidar_ros2`、`slam_gmapping` 等包全部通过

你也可以查看本次构建留存的日志：

- [output/ros2_colcon_build_rplidar.log](../../output/ros2_colcon_build_rplidar.log)
- [output/ros2_colcon_build_rplidar5.log](../../output/ros2_colcon_build_rplidar5.log)
- [output/ros2_colcon_build_rplidar6.log](../../output/ros2_colcon_build_rplidar6.log)

### 4. 验证构建结果

demo 侧验证：

- 检查 `ssne_ai_demo` 是否生成
- 观察日志里是否有 `RPLidar SDK adapter started`
- 观察运行时是否打印 `[LIDAR] samples:`

ROS2 侧验证：

- 检查 `install/` 是否生成
- 执行 `source install/setup.bash`
- 启动 RPLidar 节点：`ros2 launch rplidar_ros2 rplidar_launch.py`

## 运行方式

demo 的容器内运行入口现在是：

```powershell
docker exec A1_Builder bash -lc "cd /app/src/a1_ssne_ai_demo && bash scripts/run.sh"
```

如果你要观察雷达接入效果，建议先确认串口设备已经映射到容器内，并且参数文件里的串口号是正确的。

## 与本项目的关系

当前项目已经删除了自研 ROS 包，新的开发顺序建议如下：

1. 先把这个 demo 跑通
2. 再把雷达 SDK 接到 `lidar_sdk_adapter`
3. 再按需要决定是否重建 ROS2 包

这样可以先把视觉链路和雷达链路拆开验证，避免一开始就把系统耦合得太重。

## RPLidar 接入说明

如果你要接官方 `rplidar_sdk`，当前仓库的做法是把它放在使用它的模块旁边：

- `src/a1_ssne_ai_demo/third_party/rplidar_sdk/`

接入时的核心动作是：

1. 在 CMake 里把 `rplidar_sdk/include` 加到头文件路径
2. 把 `rplidar_sdk/src` 编进目标程序
3. 在适配层里通过 `RPlidarDriver::CreateDriver()` 打开串口
4. 调用 `connect()`、`startMotor()`、`startScan()`、`grabScanDataHq()` 读取数据
5. 结束时调用 `stop()`、`stopMotor()`、`disconnect()`、`DisposeDriver()`

当前实现还做了两个实际修复：

- 使用 C++11 兼容写法，避免 SmartSens SDK 构建系统中的语法兼容问题
- 把雷达采集做成独立适配层，避免污染人脸检测主流程

## 常见问题

- 如果容器里 `source` 之后报 `AMENT_TRACE_SETUP_FILES: unbound variable`，请保持脚本里的 `set +u` / `set -u` 包裹。
- 如果 ROS2 构建在 `astra_camera` 报头文件错误，先检查是否已经安装 `cv_bridge`、`image_geometry` 和 `nlohmann-json3-dev`。
- 如果 SDK demo 构建失败，优先检查是否仍然使用了 C++14 才有的语法，比如 `chrono_literals`、`200ms` 这种写法。
- 如果雷达没有数据，先确认串口是否正确、波特率是否匹配，以及容器是否真的拿到了设备节点。

## 后续扩展建议

- 业务配置放到 [include/project_paths.hpp](include/project_paths.hpp)
- 应用流程放到 [include/project_flow.hpp](include/project_flow.hpp) 和 [src/project_flow.cpp](src/project_flow.cpp)
- 新外设独立成一个 `*_adapter.hpp/.cpp`
- 不要把第三方库源码散落在业务目录顶层
