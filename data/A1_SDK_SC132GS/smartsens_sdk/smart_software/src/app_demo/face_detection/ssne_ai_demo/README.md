# SSNE AI 5 分类视觉导航演示项目

## 项目概述

本项目是基于 SmartSens SSNE (SmartSens Neural Engine) 的板端 AI 演示程序，实现 **5 分类视觉导航分类器**，用于 A1 视觉机器人评委演示主线。

系统通过摄像头采集 720×1280 Y8 灰度图像，中心裁剪至 320×320 后送入 MobileNet 分类模型推理，根据分类结果控制 STM32 底盘运动（forward 前进，其余类别停止），并通过 OSD 叠加层显示运行状态。

## 当前模型信息

- **类别**：`person` / `stop` / `forward` / `obstacle` / `NoTarget`（5 类）
- **模型文件**：`/app_demo/app_assets/models/test.m1model`
- **输入**：320×320 Y8 灰度（从 720×1280 摄像头帧中心裁剪）
- **输出**：5 个 float32 logits
- **阈值**：argmax 置信度 ≥ 0.6 输出有效类别，否则归为 `NoTarget`
- **底盘动作**：`forward` → 前进（vx=200 mm/s），其余 → 停止

## 文件结构

```
ssne_ai_demo/
├── demo_rps_game.cpp          # 主程序：推理循环 + A1_TEST 命令处理 + 深度帧 + 底盘控制
├── include/                   # 头文件
│   ├── common.hpp             # IMAGEPROCESSOR 图像采集管线
│   ├── rps_classifier.hpp     # RPS_CLASSIFIER 5 分类推理接口
│   ├── chassis_controller.hpp # ChassisController STM32 UART 底盘控制
│   ├── osd-device.hpp         # OsdDevice OSD 屏幕叠加层（5 图层）
│   ├── utils.hpp              # VISUALIZER 可视化高级接口
│   └── log.hpp                # 调试日志宏
├── src/                       # 实现文件
│   ├── rps_classifier.cpp     # 分类器：crop→normalize→inference→argmax+阈值
│   ├── pipeline_image.cpp     # Online Pipeline 0 图像采集（Y8 灰度）
│   ├── chassis_controller.cpp # UART 11 字节控制帧 / 24 字节遥测帧
│   ├── osd-device.cpp         # 5 层 OSD：Layer0-1 Graphic，Layer2-4 RLE Image
│   └── utils.cpp              # DrawFixedSquare / DrawBitmap / ClearLayer
├── app_assets/                # 板端运行时资源
│   ├── models/
│   │   └── test.m1model       # 5 分类 MobileNet 模型（320×320 Y8 输入）
│   ├── background.ssbmp       # 背景位图（透明开窗 720×1280）
│   └── background_colorLUT.sscl  # 共享颜色查找表
├── cmake_config/
│   └── Paths.cmake            # SDK 库 / 头文件路径
├── scripts/
│   └── run.sh                 # 板端启动脚本（检查模型 + OSD 资源）
├── CMakeLists.txt             # CMake 构建配置
└── README.md                  # 本文档
```

## 核心文件说明

### 1. demo_rps_game.cpp — 主程序

程序入口，完成以下初始化与主循环：

1. **SSNE 引擎初始化** — `ssne_initial()`
2. **图像采集管线** — IMAGEPROCESSOR（Online Pipeline 0，Sensor 0，Y8 灰度，720×1280）
3. **分类器初始化** — RPS_CLASSIFIER（加载 test.m1model，配置 320×320 crop + normalize）
4. **底盘初始化** — ChassisController（UART TX0/RX0，115200 bps，GPIO_PIN_0/2）
5. **键盘监听线程** — stdin 读取 A1_TEST 命令
6. **主循环**（200ms 间隔）：
   - 采集一帧 → 分类推理 → 更新最新快照 → 发送底盘速度命令
   - 每 1s 自动发射模拟深度帧（A1_DEPTH）
   - 每 2s 打印分类摘要日志

**常量定义**：

| 常量 | 值 | 说明 |
|------|-----|------|
| kCameraWidth | 720 | 摄像头图像宽度 |
| kCameraHeight | 1280 | 摄像头图像高度 |
| kClassifierInputWidth/Height | 320 | 分类器输入宽高 |
| kClassCount | 5 | 分类类别数 |
| kForwardVelocity | 200 | 前进速度（mm/s） |
| kDepthWidth/Height | 80×60 | 模拟深度帧尺寸 |
| kDepthAutoIntervalMs | 1000 | 深度帧发送间隔（ms） |

### 2. RPS_CLASSIFIER — 分类器

**预处理管线**：
1. 从 720×1280 中心裁剪 320×320 区域（crop_x0=200, crop_y0=480）
2. 调用模型内置归一化参数（SetNormalize）

**推理流程**：
1. `RunAiPreprocessPipe` — 裁剪 + 归一化
2. `ssne_inference` — NPU 推理
3. `ssne_getoutput` — 获取 5 个 float32 logits
4. argmax + 阈值比较（≥ 0.6 输出有效类别，否则 NoTarget）

**输出格式**：`label`（字符串）+ `confidence`（float）+ `scores[5]`（5 类得分）

### 3. ChassisController — 底盘控制

使用 A1 UART TX0（GPIO_PIN_0）/ RX0（GPIO_PIN_2），115200 bps。

**控制帧**（11 字节，A1 → STM32）：

| 偏移 | 字段 | 说明 |
|------|------|------|
| [0] | 0x7B | 帧头 |
| [1] | Cmd | 0x00 = 正常控制 |
| [2] | 0x00 | 保留 |
| [3-4] | Vx | X轴速度（mm/s，int16 BE） |
| [5-6] | Vy | Y轴速度（AKM = 0） |
| [7-8] | Vz | Z轴角速度（mrad/s） |
| [9] | BCC | XOR(bytes[0..8]) |
| [10] | 0x7D | 帧尾 |

**遥测帧**（24 字节，STM32 → A1）：Vx/Vy/Vz、Ax/Ay/Az、Gx/Gy/Gz、Volt，BCC 校验。

### 4. OsdDevice — OSD 屏幕叠加层

**5 层布局**：

| Layer | 类型 | 用途 |
|-------|------|------|
| 0 | TYPE_GRAPHIC（SS_TYPE_QUADRANGLE） | 检测框绘制 |
| 1 | TYPE_GRAPHIC（SS_TYPE_QUADRANGLE） | 固定正方形 |
| 2 | TYPE_IMAGE（SS_TYPE_RLE） | 背景位图（透明开窗 720×1280） |
| 3 | TYPE_IMAGE（SS_TYPE_RLE） | 分类结果贴图（预留） |
| 4 | TYPE_IMAGE（SS_TYPE_RLE） | 预留 |

### 5. VISUALIZER — 可视化高级接口

封装 OSD 操作：
- `DrawFixedSquare(x_min, y_min, x_max, y_max, layer_id=1)` — 绘制固定正方形
- `DrawBitmap(bitmap_path, lut_path, pos_x, pos_y, layer_id=2)` — 绘制 .ssbmp 位图
- `ClearLayer(layer_id)` — 清空指定图层

位图路径自动以 `/app_demo/app_assets/` 为前缀。

### 6. IMAGEPROCESSOR — 图像采集

封装 SSNE Online Pipeline 0，从摄像头 Sensor 0 采集 Y8 灰度图像（720×1280）。

## A1_TEST 命令协议

通过 stdin 接收命令，响应格式为 `A1_DEBUG {"command":"...","success":true/false,...}`：

| 命令 | 说明 |
|------|------|
| `A1_TEST ping` | 连接测试，返回 chassis_ok 状态 |
| `A1_TEST test_echo <msg>` | 回声测试 |
| `A1_TEST depth_snapshot` | 发射一帧模拟深度数据（80×60 u8，Base64 分片） |
| `A1_TEST rps_snapshot <id>` | 获取最新分类快照（等待最多 3s，返回 label/confidence/scores[5]/action） |
| `A1_TEST chassis_test forward\|stop` | 底盘动作测试 |
| `A1_TEST move <vx> <vy> <vz>` | 直接底盘速度控制 |
| `A1_TEST stop` | 紧急停止 |

## A1_DEPTH 深度帧协议

模拟深度数据传输，用于 Aurora 联调验证：

```
A1_DEPTH_BEGIN frame=N w=80 h=60 fmt=u8 encoding=base64 chunks=C bytes=4800
A1_DEPTH_CHUNK frame=N index=0 data=<base64>
A1_DEPTH_CHUNK frame=N index=1 data=<base64>
...
A1_DEPTH_OBJECT frame=N cls=fake score=1.00 bucket=mid depth=1.20 box=0.35,0.35,0.30,0.30
A1_DEPTH_END frame=N
```

80×60 灰度值 Base64 编码（4800 bytes），分 5 片传输（每片 960 chars）。

## 构建与运行

### 构建

通过 SDK 构建系统交叉编译：

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
```

或增量构建应用：

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_ai_demo"
```

### 板端运行

```bash
ssh root@<A1_IP>
cd /app_demo
./scripts/run.sh
```

`run.sh` 会检查 `test.m1model` 和 OSD 资源是否存在，然后启动 `ssne_ai_demo`。

### Windows 联调

```powershell
cd tools/aurora
.\launch.ps1
# 浏览器打开 http://127.0.0.1:6201
# 使用 A1_TEST 终端发送命令
```

## 数据流

```
Camera Sensor 0 (720×1280 Y8)
        │
        ▼
IMAGEPROCESSOR::GetImage()
        │
        ▼
RPS_CLASSIFIER::Predict()
  ├─ RunAiPreprocessPipe: 中心裁剪 320×320 + 归一化
  ├─ ssne_inference: NPU 推理
  └─ argmax + 阈值 0.6 → label / confidence / scores[5]
        │
        ├──▶ action_for_label(label): "forward" → vx=200, 其他 → vx=0
        │         │
        │         ▼
        │    ChassisController::SendVelocity(vx, 0, 0)
        │         │
        │         ▼
        │    UART TX0 → STM32 → 底盘电机
        │
        └──▶ stdout: [RPS] 每 2s 摘要日志
        │    stdout: A1_DEPTH 每 1s 模拟深度帧
        │    stdin: 响应 A1_TEST 命令
```

## 技术特点

1. **5 分类视觉导航**：person / stop / forward / obstacle / NoTarget
2. **灰度单通道**：Y8 格式，320×320 输入
3. **NPU 硬件加速**：SSNE 引擎，ssne_inference 执行推理
4. **预处理管线**：AiPreprocessPipe（crop + normalize）
5. **底盘集成**：UART 11 字节控制帧，BCC 异或校验，24 字节遥测
6. **A1_TEST CLI**：stdin 命令监听，JSON 响应，支持 Aurora 联调
7. **深度模拟**：80×60 模拟深度帧，Base64 编码分片传输
8. **OSD 叠加层**：5 层（2 Graphic + 3 RLE Image），独立控制

## 配置项

| 配置项 | 值 | 说明 |
|-------|-----|------|
| 摄像头分辨率 | 720×1280 | 宽×高，Y8 灰度 |
| 模型输入 | 320×320 | 中心裁剪（crop_x0=200, crop_y0=480） |
| Online 格式 | SSNE_Y_8 | 灰度 8 位 |
| 类别数 | 5 | person/stop/forward/obstacle/NoTarget |
| 置信度阈值 | 0.6 | 低于此值归为 NoTarget |
| 前进速度 | 200 mm/s | forward 类别对应的线速度 |
| 推理间隔 | 200 ms | 主循环 usleep(200000) |
| 深度帧尺寸 | 80×60 | 模拟深度图 |
| 深度间隔 | 1000 ms | 自动发射 |
| 日志间隔 | 2000 ms | 分类摘要输出 |
| UART 波特率 | 115200 | TX0/RX0 |
