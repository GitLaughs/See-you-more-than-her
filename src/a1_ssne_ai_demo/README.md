# A1 SSNE AI Demo 模块

本模块是 A1 平台上面向嵌入式部署的核心视觉算法包，基于 SmartSens SC132GS 传感器和 SSNE NPU。  
模块分为两个可独立编译的二进制目标：

| 目标 | 入口 | 功能 |
|---|---|---|
| `ssne_ai_demo` | `demo_face.cpp` | SCRFD 人脸检测 + RPLidar 扫描（原始 Demo） |
| `ssne_vision_demo` | `demo_vision.cpp` | YOLOv8 目标检测 + 人脸检测 + 雷达障碍感知 + OSD 多层叠加 + TCP 调试接口 |

## 目录结构

```text
a1_ssne_ai_demo/
├── demo_face.cpp               # ssne_ai_demo 入口（人脸 Demo）
├── demo_vision.cpp             # ssne_vision_demo 入口（综合视觉 Demo）
├── CMakeLists.txt
├── include/
│   ├── project_paths.hpp       # 全局配置（路径、阈值、端口）
│   ├── project_flow.hpp        # 人脸 Demo 流程控制接口
│   ├── vision_app.hpp          # 综合视觉应用接口（新）
│   ├── yolov8_detector.hpp     # YOLOv8 检测器接口（新）
│   ├── osd_visualizer.hpp      # 多层 OSD 可视化接口（新）
│   ├── debug_data_interface.hpp# TCP 调试数据接口（新）
│   ├── lidar_sdk_adapter.hpp   # RPLidar 适配层接口
│   ├── osd-device.hpp          # 底层 OSD 设备接口
│   ├── common.hpp
│   └── utils.hpp
├── src/
│   ├── project_flow.cpp        # 人脸 Demo 流程（ssne_ai_demo 用）
│   ├── vision_app.cpp          # 综合视觉主循环（ssne_vision_demo 用）
│   ├── yolov8_detector.cpp     # YOLOv8 推理实现（新）
│   ├── osd_visualizer.cpp      # OSD 多层绘制实现（新）
│   ├── debug_data_interface.cpp# TCP JSON 调试流实现（新）
│   ├── lidar_sdk_adapter.cpp   # RPLidar 适配层实现
│   ├── osd-device.cpp          # OSD 设备封装
│   ├── scrfd_gray.cpp          # SCRFD 灰度人脸检测
│   ├── pipeline_image.cpp      # 图像预处理管线
│   └── utils.cpp
├── third_party/
│   └── rplidar_sdk/            # Slamtec RPLidar C++ SDK
├── app_assets/
│   ├── colorLUT.sscl           # OSD LUT 颜色表
│   └── models/                 # NPU 模型（.m1model 格式）
│       ├── face_640x480.m1model
│       └── yolov8n_640x640.m1model
├── cmake_config/
│   └── Paths.cmake             # 交叉编译路径变量
└── scripts/
    └── run.sh                  # 板端启动脚本
```

## 模块说明

### `project_paths.hpp` — 统一配置

集中管理所有运行时参数，不需要修改其他源文件即可调整：

| 字段 | 默认值 | 说明 |
|---|---|---|
| `image_shape` | `{720, 1280}` | 传感器原始分辨率（H×W） |
| `crop_shape` | `{720, 540}` | 裁剪后输入尺寸 |
| `det_shape` | `{640, 480}` | SCRFD 人脸检测输入尺寸 |
| `confidence_threshold` | `0.4f` | SCRFD 置信度阈值 |
| `face_model_path` | `/app_demo/.../face_640x480.m1model` | 人脸模型路径（板端绝对路径） |
| `yolo_det_shape` | `{640, 640}` | YOLOv8 输入尺寸 |
| `yolo_confidence_threshold` | `0.25f` | YOLOv8 置信度阈值 |
| `yolo_nms_threshold` | `0.45f` | YOLOv8 NMS IOU 阈值 |
| `yolo_num_classes` | `2` | 检测类别数（person、car） |
| `yolo_model_path` | `/app_demo/.../yolov8n_640x640.m1model` | YOLOv8 模型路径（板端绝对路径） |
| `yolo_class_names` | `{"person", "car"}` | 类别名称列表 |
| `lidar_serial_port` | `/dev/ttyUSB0` | RPLidar 串口 |
| `lidar_baudrate` | `115200` | 波特率 |
| `debug_tcp_port` | `9090` | Aurora 调试工具 TCP 端口 |

---

### `yolov8_detector.hpp/.cpp` — YOLOv8 目标检测

基于 SSNE NPU 的 YOLOv8 无锚框检测器，支持多类别目标检测。

**核心架构**：
- 3 个检测头，步长 `{8, 16, 32}`
- DFL（Distribution Focal Loss）边框解码，`reg_max=16`
- Per-class sigmoid 置信度过滤
- Per-class NMS（IoU 阈值独立过滤）

**关键接口**：
```cpp
// 初始化检测器
bool Initialize(const std::string& model_path,
                const std::array<int,2>& img_shape,
                const std::array<int,2>& det_shape,
                int num_classes, float conf_thresh, float nms_thresh);

// 执行推理，结果写入 result
bool Predict(const ImageInput& img_in, DetectionResult& result);

// 释放 NPU 资源
void Release();
```

**数据类型**：
```cpp
struct Detection {
    float box[4];   // [x1, y1, x2, y2]，原图坐标
    float score;
    int   class_id;
};
struct DetectionResult {
    std::vector<Detection> detections;
};
```

---

### `osd_visualizer.hpp/.cpp` — 多层 OSD 可视化

使用硬件 DMA OSD 层绘制检测结果，支持多类别颜色区分：

| OSD 层 | 用途 | 颜色 |
|---|---|---|
| Layer 0 | SCRFD 人脸框 | 绿色（LUT 索引 0） |
| Layer 1 | YOLOv8 检测框（按类别区分） | person=黄色，gesture=蓝色，obstacle=红色 |
| Layer 2 | 信息覆盖层（状态文字、障碍警告） | 白色/红色 |

**关键接口**：
```cpp
bool Init(OsdDevice* osd, int img_width, int img_height);
void DrawFaces(const FaceDetectionResult& faces);
void DrawDetections(const DetectionResult& dets);
void DrawInfoRegion(const std::string& info, bool obstacle_warning);
void ClearAll();
```

---

### `vision_app.hpp/.cpp` + `demo_vision.cpp` — 综合视觉应用

`VisionApp` 整合所有感知模块的主循环：

```
[图像采集] → [SCRFD 人脸检测] → [YOLOv8 目标检测]
    ↓
[RPLidar 扫描] → [障碍区域计算（6 扇区，阈值 0.5m）]
    ↓
[OSD 多层绘制] + [TCP 调试数据发送]
```

前方 ±30° 扇区内发现距离 < 0.5m 的障碍物时，Layer 2 叠加红色警告覆盖。

---

### `debug_data_interface.hpp/.cpp` — TCP 调试接口

向 Aurora 调试工具（或任意 TCP 客户端）实时推送 JSON 格式的感知数据帧：

```json
{
  "type": "frame",
  "timestamp_ms": 1700000000000,
  "pointcloud": [{"a": 45.0, "d": 1.23, "q": 15}],
  "detections": [{"class": "person", "score": 0.87, "box": [100, 200, 300, 400]}],
  "obstacle_zones": [{"angle_start": -30, "angle_end": 30, "min_dist": 0.42, "blocked": true}]
}
```

端口：`9090`（可在 `project_paths.hpp` 中修改）  
协议：换行符分隔的 JSON 流（newline-delimited JSON）

---

### `lidar_sdk_adapter.hpp/.cpp` — RPLidar 适配层

封装 Slamtec RPLidar C++ SDK，提供轻量 `LidarSample` 结构：

```cpp
struct LidarSample {
    float angle_deg;
    float distance_m;
    uint8_t quality;
};

// 获取一圈扫描数据
bool GrabScan(std::vector<LidarSample>& samples);
```

详细接入说明见 [docs/RPLIDAR_SDK_GUIDE.md](../../docs/RPLIDAR_SDK_GUIDE.md)。

---

## 编译

在 Docker 容器（`A1_Builder`）内通过 Buildroot 构建：

```bash
# 综合视觉 demo（推荐）
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk && \
    make BR2_EXTERNAL=./smart_software ssne_vision_demo"

# 原始人脸 demo
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk && \
    make BR2_EXTERNAL=./smart_software ssne_ai_demo"

# 使用项目全量构建脚本
bash scripts/build_vision_stack.sh
```

详细编译步骤见 [docs/BUILD.md](../../docs/BUILD.md)。

## NPU 模型准备

将 SSNE 格式（`.m1model`）的模型文件放置到 `app_assets/models/` 目录：

```
app_assets/models/
├── face_640x480.m1model       # SCRFD 人脸检测模型
└── yolov8n_640x640.m1model    # YOLOv8 目标检测模型
```

YOLOv8 模型训练与导出流程见 [docs/YOLOV8_TRAINING.md](../../docs/YOLOV8_TRAINING.md)。

## Aurora 调试工具


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
