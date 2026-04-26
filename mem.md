# See-you-more-than-her 仓库记忆

更新时间: 2026-04-26

## 项目总览

这个仓库目前是一个 A1 SC132GS 开发板 + Windows 调试工具 + STM32/WHEELTEC 底盘联调项目。核心目标是把旧备份 `output/smartsens_sdk` 里的业务逻辑适配到更新后的官方 SDK `data/A1_SDK_SC132GS` 中，并用 `tools/aurora` 在 PC 侧完成摄像头预览、数据采集、ONNX/PT 推理、串口调试、A1_TEST CLI、ROS/底盘控制等联调工作。

主要目录:

- `data/A1_SDK_SC132GS`: 新官方 SDK。后续 A1 板端开发以这里为准。
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software`: Buildroot external overlay，包含板级配置、app_demo 包、rootfs overlay。
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo`: A1 板端 demo 源码，当前重点是 `face_detection/ssne_ai_demo`。
- `output/smartsens_sdk`: 旧备份 SDK/旧代码来源，只作为逻辑参考，不应直接覆盖新 SDK。
- `tools/aurora`: Windows 侧调试伴侣 Web 工具，负责预览、相机桥、串口、A1 CLI、底盘、ROS、模型切换。
- `scripts`: Docker/SDK/EVB/ROS 构建脚本。
- `docs`: 项目文档，特别是 `06_程序概览.md`、`07_架构设计.md`、`15_AI模型转换与部署.md`。
- `models`: PC 侧推理模型。目前有 `best.pt`，曾经默认期待 `best_a1_formal.onnx`，但文件可能不存在。
- `output/evb`: EVB 构建产物目录。最近一次成功产物为 `output/evb/20260426_122457`。

## tools/aurora

`tools/aurora` 是 PC 端主调试工具，启动入口通常是:

```powershell
cd tools/aurora
.\launch.ps1 -Device -1 -Source auto
```

### `launch.ps1`

Windows 启动脚本。负责:

- 选择 Python 解释器。
- 清理占用 Companion 端口的旧 `aurora_companion.py`。
- 清理占用 Qt 桥端口 5911 的旧 `qt_camera_bridge.py`。
- 找可用端口并启动 Flask Companion。
- 自动打开浏览器。

注意:

- Windows 下 `Get-Process` 没有稳定的 `CommandLine` 字段，清理旧进程时应使用 `Get-CimInstance Win32_Process`。
- 如果端口清不掉，先查 `Get-NetTCPConnection -LocalPort 5911 -State Listen`。

### `aurora_companion.py`

Flask 主程序。负责:

- Web 页面路由。
- 摄像头设备枚举、选择、打开、刷新。
- `/video_feed` 原始预览流。
- `/detect_feed` PC 侧 YOLOv8/Ultralytics/ONNX 检测流。
- `/detect_models`、`/switch_detect_model`、`/detect_status` 模型列表、切换和状态。
- `/status` 全局状态轮询，包括摄像头分辨率、fps、模型名、Qt 桥状态。
- `/camera_devices`、`/switch_camera` 摄像头列表和切换。
- 数据集采集 `/capture`、最近采集 `/recent_captures`。
- 注册 `chassis_comm.py`、`relay_comm.py`、`ros_bridge.py`、`serial_terminal.py` 蓝图。

关键经验:

- 不能只用 `importlib.util.find_spec("PySide6")` 判断 Qt 桥 Python 可用，因为 `venv_39` 曾出现 PySide6/shiboken6 与 NumPy 2.0.2 ABI 警告。应做真实 import: `import PySide6; from PySide6.QtMultimedia import QMediaDevices, QCamera, QVideoSink`。
- 默认模型不能写死成不存在的 `models/best_a1_formal.onnx`。初始化应优先读偏好文件，其次找现有 `.onnx`，再找 `.pt`，最后才保留默认路径并在状态里报错。
- A1 摄像头设备名通常类似 `Smartsens-FlyingChip-A1-1`，自动模式应优先选择 A1 候选。
- Qt 桥切换摄像头后要等首帧，否则前端会显示连接成功但没有画面。

### `qt_camera_bridge.py`

独立 QtMultimedia HTTP 桥。负责用 PySide6 读取 Windows/Qt 能看到的摄像头，再通过本地 HTTP 提供:

- `/status`
- `/devices`
- `/switch`
- `/frame.jpg?mode=color|gray`

为什么需要它:

- OpenCV/DirectShow 对 A1 设备的 Y8/UYVY/竖屏格式不稳定。
- QtMultimedia 能更接近 Aurora.exe 的相机读取路径。

关键经验:

- A1 设备可能暴露为 `Format_UYVY`、`360x1280` 或 `720x1280`，不要只按 1280x720 判断。
- A1 源要偏好 RAW/GRAY/YUV 类格式，Windows 普通摄像头偏好 RGB/MJPEG/NV12。
- 如果日志出现 NumPy 1.x/2.x ABI 警告但进程仍可启动，真实状态以 `/status`、`/devices`、`/frame.jpg` 为准。

### `templates/companion_ui.html`

单文件前端页面。负责:

- 左侧实时预览、摄像头输入、训练集拍照。
- STM32 直连页。
- A1/COM13 页。
- ONNX/PT 模型切换。
- 串口终端、A1_TEST 快捷按钮、日志显示。
- 底盘运动控制、遥测显示、ROS 控制。

UI 调整经验:

- 摄像头输入模块容易因为两列大按钮、分隔线、说明文字产生大量空白。当前使用 `.camera-input-card`、`.camera-input-actions`、`.camera-input-compact` 压缩。
- 预览区域由 `.preview-wrap` 控制，宽度通过 `--preview-natural-width` 和 `--preview-aspect` 跟随输入源。
- 旋转控件是 `#previewRotationSwitch`，不要让它用普通大按钮样式。
- 如果新增按钮，必须同时确认对应 JS 函数存在。之前 A1 串口终端 HTML 已有按钮，但 `loadSerialTermPorts`、`connectSerialTerm`、`sendSerialTestCommand` 等函数缺失，导致点击无响应。

### `serial_terminal.py`

A1 调试串口实时终端蓝图，路由前缀 `/api/serial_term`。负责:

- `/ports`: 枚举串口。
- `/config`: 读写默认端口、波特率、换行设置。
- `/connect`、`/auto_connect`、`/disconnect`: 连接控制。
- `/send`: 原始文本/HEX 发送。
- `/send_test`: 发送 `A1_TEST <command> <message>` 并等待回传 token。
- `/status`、`/logs`、`/clear`: 状态与日志。

经验:

- 默认端口是 `COM13`，优先匹配 `USB-HiSpeed-SERIAL-A`、`CH347F`、`smartsens`、`flyingchip`。
- A1 板端 CLI 回传最好包含 `"success":true` 和 `"command":"xxx"`，否则前端等待 token 会超时。
- 串口读取需要处理无换行碎片，当前有 partial buffer flush。

### `relay_comm.py`

PC 经由 COM13/A1_TEST 控制 A1，再由 A1 控制 STM32 的中继模块。适合验证 PC -> A1 -> STM32 链路。

涉及:

- 端口枚举/连接。
- A1_TEST 命令发送。
- 与 `serial_terminal.py` 共用串口逻辑。

如果 A1 经由链路无响应，先验证:

1. `serial_terminal.py` 能否连接 COM13。
2. `A1_TEST test_echo` 是否有 JSON 回传。
3. 板端 `/app_demo/scripts/run.sh` 是否正在运行新版 `ssne_ai_demo`。

### `chassis_comm.py`

电脑直连 STM32 的串口控制模块。负责 0x7B/0x7D 协议帧发送、遥测解析、运动控制、急停。

协议:

- 控制帧 11 字节: `0x7B ... BCC 0x7D`
- 遥测帧 24 字节: `0x7B ... BCC 0x7D`
- BCC 是 XOR 校验。

改运动控制/协议时需要同步:

- `tools/aurora/chassis_comm.py`
- `tools/aurora/templates/companion_ui.html`
- A1 板端 `chassis_controller.cpp`
- 文档 `docs/06_程序概览.md`、`docs/07_架构设计.md`

### `ros_bridge.py`

ROS2 桥接模块。负责:

- 启停底盘 ROS 节点。
- 启停避障节点。
- 从 PC 侧检测快照驱动 ROS 动作。
- 维护 ROS 配置，例如串口、波特率、线速度、FOV、自动化开关。

注意:

- ROS 工作区在 `src/ros2_ws`。
- `scripts/build_ros2_ws.sh` 可独立构建 ROS2 工作区。
- 完整 EVB 构建可用 `--skip-ros` 跳过 ROS，避免把问题扩大。

## A1 app_demo: `ssne_ai_demo`

路径:

`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo`

这是板端应用，最终安装到 rootfs 的 `/app_demo/ssne_ai_demo`，脚本在 `/app_demo/scripts/run.sh`。

### `demo_face.cpp`

板端主程序。当前职责:

- 初始化 SSNE。
- 初始化图像 pipeline。
- 根据 `cfg::USE_SCRFD_BACKEND` 选择 `SCRFDGRAY` 或 `YOLOV8`。
- 获取 720x1280 Y8 图像。
- 推理并绘制 OSD 框。
- 初始化 `ChassisController`，通过 A1 UART TX0/RX0 控制 STM32。
- 支持 Link-Test: 默认每 5 秒周期内前进 1 秒，其余停车。
- 支持 stdin 形式的 `A1_TEST` CLI:
  - `A1_TEST help`
  - `A1_TEST status`
  - `A1_TEST test_echo`
  - `A1_TEST debug_status`
  - `A1_TEST debug_frame`
  - `A1_TEST debug_last`
  - `A1_TEST link_test on|off`
  - `A1_TEST stop`
  - `A1_TEST move vx vy vz`

经验:

- 前端/串口等待的 token 需要 `"success":true` 和 `"command":"xxx"`。
- 命令线程只改状态，主循环按状态发送底盘速度，避免串口对象跨线程乱用。
- 退出时必须发 0 速度停车。

最新修复记录（2026-04-26）:

- `ssne_ai_demo` 的人脸检测链路已恢复为历史裁剪模式：`IMAGEPROCESSOR` 重新输出 `720×540` 中间区域，`SCRFDGRAY` 也重新按裁剪图初始化。
- 主循环在 OSD 绘制前会把检测框的 `y` 坐标加回 `PIPE_CROP_Y1 = 370`，避免检测框和画面位置错位。
- 这次先做了静态检查，随后改为只编 `make BR2_EXTERNAL=./smart_software ssne_ai_demo` 做编译验证；已成功生成并安装到 `output/target/app_demo/`，仅有 `utils.cpp` 里原有的 narrowing warning 和一组 CMake 未使用变量警告，没有新增错误。

### `project_paths.hpp`

全局配置:

- `SENSOR_WIDTH=720`
- `SENSOR_HEIGHT=1280`
- `USE_SCRFD_BACKEND=true`
- `DET_WIDTH=640`
- `DET_HEIGHT=480`
- `MODEL_PATH="/app_demo/app_assets/models/face_640x480.m1model"`
- YOLO 类别数、NMS、top-k、底盘速度、Link-Test 周期。

注意:

- 当前默认走 SCRFD 历史模型，YOLOv8 板端 `.m1model` 后续需要重新确认模型文件和输出头。
- `YOLO_NUM_CLASSES` 与目标类别常量要保持一致，不能出现类别数 4 但定义到 class 4 的错位。

### `include/common.hpp`

公共类型和模型类声明:

- `FaceDetectionResult`: boxes、landmarks、scores、class_ids。
- `IMAGEPROCESSOR`: 图像 pipeline 封装。
- `SCRFDGRAY`: SCRFD 灰度模型。
- `YOLOV8`: YOLOv8 head6 灰度模型。

经验:

- 旧备份的 `yolov8_gray.cpp` 依赖 `YOLOV8` 类和 `FaceDetectionResult::class_ids`。如果只复制 cpp 不补 common.hpp，必然编译失败。

### `src/pipeline_image.cpp`

图像输入 pipeline。

当前按新 SDK/文档口径:

- `OnlineSetCrop(kPipeline0, 0, img_width, 0, img_height)`
- `OnlineSetOutputImage(kPipeline0, SSNE_Y_8, img_width, img_height)`

经验:

- 旧代码曾裁剪为 720x540，再用 `crop_offset_y=370` 映射坐标。新文档写的是完整 720x1280 输入，不应继续混用旧裁剪逻辑，否则 OSD 坐标、检测输入和 Aurora 显示会错位。

### `src/scrfd_gray.cpp`

SCRFD 模型实现，含:

- anchor 生成。
- 模型加载。
- 推理。
- 后处理/NMS。
- `FaceDetectionResult` 的 Clear/Free/Reserve/Resize/拷贝实现。

经验:

- 加入 YOLO 后，NMS/排序必须保留 `class_ids`，否则 NMS 后类别和 box 会错位。
- 多类别 NMS 应跳过不同 class id 的框，避免跨类别互相抑制。

### `src/yolov8_gray.cpp`

YOLOv8 head6 模型实现:

- 6 个输出头: cls stride 8/16/32 + reg stride 8/16/32。
- DFL softmax 解码。
- sigmoid 分类。
- NMS。
- 坐标从 det_shape 映射回 sensor img_shape。

经验:

- 此文件依赖 `cfg::YOLO_NUM_CLASSES`、`cfg::YOLO_REG_BINS`、`cfg::OUTPUT_HEAD_NUM`。
- 如果模型文件不是 head6 输出，后处理会不匹配。
- 如果 `model_id < 0`，要限频打印错误，避免刷爆串口/日志。

### `src/chassis_controller.cpp` / `include/chassis_controller.hpp`

A1 板端 UART 控制 STM32:

- GPIO_PIN_0 配 UART_TX0。
- GPIO_PIN_2 配 UART_RX0。
- 波特率 115200。
- 控制帧 11 字节 `0x7B ... 0x7D`。
- 遥测帧 24 字节。

经验:

- CMake 必须链接 `libgpio.so` 和 `libuart.so`，否则会出现 `undefined reference to gpio_init/uart_send_data/...`。

### `src/osd-device.cpp` / `src/utils.cpp` / `include/utils.hpp`

OSD 与辅助函数:

- `VISUALIZER::Initialize`
- `VISUALIZER::Draw`
- `DrawFixedSquare`
- `DrawBitmap`
- OSD 图层和贴图。

经验:

- OSD 坐标类型可能触发 int->float narrowing warning，不影响产物，但需要清理 warning 时可以显式 cast。
- 位图路径按 `/app_demo/app_assets/...` 组织。

### `CMakeLists.txt` / `cmake_config/Paths.cmake`

板端 demo 构建入口。

必须包含:

- `demo_face.cpp`
- `src/chassis_controller.cpp`
- `src/osd-device.cpp`
- `src/pipeline_image.cpp`
- `src/scrfd_gray.cpp`
- `src/utils.cpp`
- `src/yolov8_gray.cpp`

必须链接:

- `libssne.so`
- `libcmabuffer.so`
- `libosd.so`
- `libgpio.so`
- `libuart.so`
- `libsszlog.so`
- `libzlog.so`
- `libemb.so`

经验:

- 不要为了绕过编译错误长期排除 `yolov8_gray.cpp` 或 `chassis_controller.cpp`。正确做法是补全接口和链接库。

## scripts

### `scripts/a1_sc132gs_build.sh`

根目录 wrapper，进入 `data/A1_SDK_SC132GS/smartsens_sdk` 后调用 SDK 内部 `scripts/a1_sc132gs_build.sh`。

用途:

```bash
docker exec A1_Builder bash -lc "cd /app && ./scripts/a1_sc132gs_build.sh"
```

### `scripts/build_complete_evb.sh`

完整 EVB 构建脚本。常用:

```bash
docker exec A1_Builder bash -lc "BUILD_JOBS=4 /app/scripts/build_complete_evb.sh --app-only --skip-ros"
```

模式:

- 默认完整构建 SDK、demo、可选 ROS、重新打包 zImage。
- `--app-only`: 跳过基础库，重编 `ssne_ai_demo` 并重打包 zImage。
- `--skip-ros`: 跳过 ROS2 编译。

最近成功验证:

- `BUILD_JOBS=4 /app/scripts/build_complete_evb.sh --app-only --skip-ros`
- 产物: `output/evb/20260426_122457/zImage.smartsens-m1-evb`
- 同目录 `ssne_ai_demo` 大约 70K。

### `scripts/build_ros2_ws.sh`

编译 `src/ros2_ws`。如果只是验证 A1 SDK/app_demo，不要让 ROS 问题阻塞 EVB，可以先 `--skip-ros`。

### `scripts/bootstrap.sh` / `build_docker.sh`

环境/Docker 辅助脚本。Docker 容器名通常是 `A1_Builder`。

## 常见修改入口

### 修改前端摄像头预览/输入

涉及:

- `tools/aurora/templates/companion_ui.html`
- `tools/aurora/aurora_companion.py`
- `tools/aurora/qt_camera_bridge.py`

关注点:

- 预览尺寸: `.preview-wrap`、`applyPreviewGeometry()`。
- 摄像头输入面板: `.camera-input-card`、`#cameraSourceSwitch`、`#previewRotationSwitch`。
- 源切换: `setCameraSource()`、`switchCamera()`、`/switch_camera`。
- Qt 桥状态: `/status` 中的 `qt_bridge`、`stream_width`、`stream_height`。

### 修改 PC 侧模型切换/推理

涉及:

- `tools/aurora/aurora_companion.py`
- `tools/aurora/templates/companion_ui.html`
- `models/`

关注点:

- `_SUPPORTED_DETECT_MODEL_SUFFIXES`
- `_initial_detect_model_path()`
- `_list_detect_models()`
- `set_detect_model_path()`
- `/detect_models`
- `/switch_detect_model`
- `/detect_feed`

经验:

- 模型不存在时不能在 import 阶段崩掉，应让 UI 状态显示不可用。
- `.pt` 使用 Ultralytics，`.onnx` 使用 onnxruntime。

### 修改 A1 板端检测/控制逻辑

涉及:

- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/demo_face.cpp`
- `project_paths.hpp`
- `include/common.hpp`
- `src/scrfd_gray.cpp`
- `src/yolov8_gray.cpp`
- `src/pipeline_image.cpp`
- `src/chassis_controller.cpp`
- `CMakeLists.txt`
- `cmake_config/Paths.cmake`

验证:

```bash
docker exec A1_Builder bash -lc "cd /app/data/A1_SDK_SC132GS/smartsens_sdk && rm -rf output/build/ssne_ai_demo output/target/app_demo/ssne_ai_demo && make BR2_EXTERNAL=./smart_software ssne_ai_demo"
docker exec A1_Builder bash -lc "BUILD_JOBS=4 /app/scripts/build_complete_evb.sh --app-only --skip-ros"
```

### 修改 A1_TEST/COM13 串口链路

涉及:

- 板端: `demo_face.cpp`
- PC 终端: `tools/aurora/serial_terminal.py`
- PC relay: `tools/aurora/relay_comm.py`
- 前端按钮: `tools/aurora/templates/companion_ui.html`

经验:

- 前端快速测试等 token，所以回传 JSON 字段要保持稳定。
- 如果 `send_test` 发送成功但无响应，先查板端 demo 是否运行，再查串口是否被其他程序占用。

### 修改 STM32 直连底盘控制

涉及:

- `tools/aurora/chassis_comm.py`
- `tools/aurora/templates/companion_ui.html`
- 文档 `docs/05_硬件参考.md`、`docs/06_程序概览.md`、`docs/07_架构设计.md`

关注:

- 11 字节控制帧格式。
- 24 字节遥测帧格式。
- BCC XOR。
- 端口占用和波特率。

### 修改 ROS 联动

涉及:

- `tools/aurora/ros_bridge.py`
- `tools/aurora/templates/companion_ui.html`
- `src/ros2_ws`
- `scripts/build_ros2_ws.sh`

关注:

- ROS 节点是否存在。
- 串口是否与直连调试模块竞争。
- YOLO 快照轮询是否打开。

## 已遇到过的错误与解决经验

### elfutils 编译失败: `PACKAGE_VERSION` undeclared

现象:

```text
dwfl_version.c:38:10: error: 'PACKAGE_VERSION' undeclared
```

解决:

- 在 `data/A1_SDK_SC132GS/smartsens_sdk/package/elfutils/` 添加/修复 fallback patch。
- `lib/printversion.c` 补 `PACKAGE_NAME`、`PACKAGE_VERSION`、`PACKAGE_URL` fallback。
- `libdwfl/dwfl_version.c` 补 `PACKAGE_VERSION` fallback。

### `yolov8_gray.cpp` 编译失败，`YOLOV8` 未声明

原因:

- 只迁移了旧 cpp，没有把类声明和 `FaceDetectionResult::class_ids` 迁入 `common.hpp`。

解决:

- `include/common.hpp` 声明 `YOLOV8`。
- `FaceDetectionResult` 增加 `class_ids`。
- `scrfd_gray.cpp` 中 Clear/Free/Reserve/Resize/拷贝/NMS/排序保留 class_ids。

### 底盘控制链接失败: `undefined reference to uart_send_data/gpio_init`

原因:

- `chassis_controller.cpp` 已加入构建，但 CMake 没有链接 `libgpio.so`、`libuart.so`。

解决:

- `cmake_config/Paths.cmake` 增加 `M1_GPIO_LIB`、`M1_UART_LIB`。
- `CMakeLists.txt` 的 `target_link_libraries` 加上这两个库。

### A1 图像坐标错位/裁剪不一致

原因:

- 旧代码按 720x540 中间裁剪运行，新文档/新配置按完整 720x1280 输入。

解决:

- `pipeline_image.cpp` 改完整输入。
- `demo_face.cpp` 不再加 `crop_offset_y`。
- `project_paths.hpp` 保持 sensor/det shape 一致。

### Qt 相机桥打开失败，所有摄像头失败

原因之一:

- Python 选择逻辑只看 `find_spec("PySide6")`，但实际 import 时 PySide6/shiboken6 与 NumPy ABI 有警告/异常。

解决:

- `_python_has_module("PySide6")` 做真实 import 探测。
- 用 `/status`、`/devices`、`/frame.jpg` 验证桥是否真实可用。
- `launch.ps1` 用 CIM 清理旧桥进程。

### A1 摄像头连接不上但 Windows 摄像头可以

排查:

- `qt_camera_bridge.py /devices` 是否能看到 `Smartsens-FlyingChip-A1-1`。
- `/switch` 后 5 秒内是否有 `/frame.jpg`。
- `aurora_companion.py` 自动选择是否因为旧偏好文件选回 Windows 摄像头。

解决经验:

- 自动模式优先 A1 设备名/格式。
- 切换后等待首帧。
- `/status` 返回 `stream_width/stream_height` 给前端调整显示。

### PC 端 ONNX 模型切换无响应

原因:

- 默认模型 `best_a1_formal.onnx` 不存在时，初始化可能失败或 UI 状态不可用。
- 当前 `models` 目录至少有 `best.pt`，不一定有 `.onnx`。

解决:

- 初始模型选择优先现有 `.onnx`，其次 `.pt`。
- 切换接口只接受存在的 `.onnx/.pt`。
- UI 切换后刷新 `detect_status` 和检测流。

### A1 串口终端 / CLI 按钮无响应

原因:

- HTML 中按钮调用了 JS 函数，但函数未实现。
- 板端回传没有前端等待的 `"command"` 字段。

解决:

- 在 `companion_ui.html` 实现 `loadSerialTermPorts`、`connectSerialTerm`、`autoConnectSerialTerm`、`sendSerialTerm`、`sendSerialTestCommand` 等函数。
- `demo_face.cpp` 的 A1_TEST 回传补 `"command":"xxx"`。

### A1 预览伪彩色、宽度压缩和延迟积累

原因:

- Qt 枚举到的 A1 格式是 `360x1280 Format_UYVY @ 90fps`，实际是 720 宽 Y8 灰度字节被驱动按 UYVY 暴露。
- 如果直接 `frame.toImage()` 再编码，Qt 会把灰度原始字节按 UYVY 彩色视频解释，前端表现为绿色/紫色伪彩色，并且宽度只有一半。
- A1 每帧同时编码彩色和灰度 JPEG 会增加 90fps 下的 CPU 压力，浏览器 MJPEG 也容易因缓存/缓冲出现延迟累积。

解决:

- 在 `tools/aurora/qt_camera_bridge.py` 的 `_a1_raw_y8_image` 中直接 map `QVideoFrame` 平面，把每行 `bytesPerLine` 中的 720 个灰度字节构造成 `Format_Grayscale8`，再编码。
- A1 模式下 `color_jpeg` 和 `gray_jpeg` 复用同一张灰度 JPEG，质量降到 `A1_JPEG_QUALITY`，优先选择 90fps 格式。
- `/frame.jpg` 和 Flask 的 MJPEG 路由都加 `Cache-Control: no-store`、`Pragma: no-cache`、`X-Accel-Buffering: no`。
- 验证时桥接状态应从 `360x1280` 变为 `720x1280`，实际测试约 `87.9fps`，JPEG 解码后三通道差值为 0。

### 完整 EVB 构建耗时太长或卡住

建议:

- 已完整构建过 SDK 后，用:

```bash
docker exec A1_Builder bash -lc "BUILD_JOBS=4 /app/scripts/build_complete_evb.sh --app-only --skip-ros"
```

- 先单独验证 demo:

```bash
docker exec A1_Builder bash -lc "cd /app/data/A1_SDK_SC132GS/smartsens_sdk && make BR2_EXTERNAL=./smart_software ssne_ai_demo"
```

### Git 仓库分歧

当前曾观察到:

```text
feat/aurora-ui-enhancement...origin/feat/aurora-ui-enhancement [ahead 50, behind 165]
```

经验:

- 没有 `UU` 文件不代表可以直接 push；ahead/behind 是历史分歧。
- 大量 SDK 替换和未跟踪文件存在时，不要直接 rebase/merge，先新建分支并提交当前状态。
- 远端 URL 中不要保存明文 token。
- 如果要以本地为准解决远端历史分歧，可在确认本地已提交后执行 `git merge -s ours --allow-unrelated-histories origin/main`，保留本地文件树并生成 merge commit。

## 当前可用验证命令

Python 语法检查:

```powershell
venv_39\Scripts\python.exe -m py_compile tools\aurora\aurora_companion.py tools\aurora\qt_camera_bridge.py tools\aurora\serial_terminal.py tools\aurora\relay_comm.py tools\aurora\chassis_comm.py tools\aurora\ros_bridge.py
```

Qt 桥快速验证:

```powershell
venv_39\Scripts\python.exe tools\aurora\qt_camera_bridge.py --host 127.0.0.1 --port 5919
```

SDK demo 编译:

```powershell
docker exec A1_Builder bash -lc "cd /app/data/A1_SDK_SC132GS/smartsens_sdk && rm -rf output/build/ssne_ai_demo output/target/app_demo/ssne_ai_demo && make BR2_EXTERNAL=./smart_software ssne_ai_demo"
```

EVB 打包:

```powershell
docker exec A1_Builder bash -lc "BUILD_JOBS=4 /app/scripts/build_complete_evb.sh --app-only --skip-ros"
```
