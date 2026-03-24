# A1 人脸检测 + 底盘驱动 Demo

基于 SmartSens A1 SSNE NPU 的人脸检测与 WHEELTEC 底盘控制应用。

> 检测逻辑参考 SDK 原始示例：`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`

## 编译目标

| 二进制 | 入口 | 功能 |
|------|------|------|
| `ssne_face_drive_demo` | `demo_face_drive.cpp` | SCRFD 人脸检测 + 底盘控制 |

## 目录结构

```text
a1_ssne_ai_demo/
├── demo_face_drive.cpp          # 入口
├── CMakeLists.txt               # 构建配置（仅编译所需源文件）
├── include/
│   ├── face_drive_app.hpp       # FaceDriveApp 主类
│   ├── chassis_controller.hpp   # WHEELTEC C50X 协议控制器
│   ├── project_paths.hpp        # 运行时配置（传感器/检测/底盘参数）
│   ├── common.hpp               # SCRFDGRAY / IMAGEPROCESSOR / VISUALIZER 类型
│   ├── osd-device.hpp           # OSD 设备封装
│   └── utils.hpp
├── src/
│   ├── face_drive_app.cpp       # FaceDriveApp 主循环
│   ├── chassis_controller.cpp   # GPIO UART0 底盘通信（0x7B 协议帧）
│   ├── scrfd_gray.cpp           # SCRFD 灰度人脸检测器
│   ├── pipeline_image.cpp       # SC132GS 图像采集管道
│   ├── osd-device.cpp           # OSD DMA 渲染
│   └── utils.cpp
├── app_assets/
│   ├── colorLUT.sscl            # OSD 颜色查找表
│   └── models/
│       └── face_640x480.m1model # SCRFD 人脸检测模型
├── cmake_config/
│   └── Paths.cmake              # SDK 库路径定义
└── scripts/
    └── run.sh                   # 板端启动脚本（insmod + 运行）
```

## 处理流程

```
SC132GS 720×1280 → 裁剪 720×540 (offset_y=370) → SCRFD 640×480
    ↓
  人脸检测结果
    ↓
  有人脸 → SendVelocity(100, 0, 0)  → 前进 100 mm/s
  无人脸 → SendStop()               → 停车
    ↓
  OSD 渲染检测框 (坐标还原到原图)
```

## 编译

```bash
# 完整 EVB 构建（推荐，产物保存至时间戳目录）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"

# 增量编译（开发迭代）
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_face_drive_demo"
```

## 暂未编译的模块

以下源文件保留在目录中，当前 CMakeLists.txt 不编译：

| 文件 | 用途 | 恢复条件 |
|------|------|---------|
| `lidar_sdk_adapter.*` | RPLidar 适配层 | 安装雷达后启用 |
| `project_flow.*` | SDK 原始 Demo 逻辑 | 仅供参考 |
| `vision_app.*` | YOLOv8 + 人脸综合应用 | 启用 YOLOv8 时 |
| `yolov8_detector.*` | YOLOv8 NPU 推理 | 启用 YOLOv8 时 |
| `osd_visualizer.*` | 多层 OSD (含 YOLOv8) | 启用 YOLOv8 时 |
| `debug_data_interface.*` | TCP JSON 调试流 | 启用 Aurora 调试时 |

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
