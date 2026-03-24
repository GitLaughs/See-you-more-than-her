# A1 Vision Robot Stack

基于 SmartSens A1 开发板的智能视觉机器人软件栈。当前阶段为摄像头人脸检测 + WHEELTEC 底盘控制的硬件兼容性验证。

## 当前功能

| 模块 | 说明 | 状态 |
|------|------|------|
| SCRFD 人脸检测 | 灰度图多尺度人脸检测 (SSNE NPU 加速) | ✅ 已完成 |
| OSD 硬件叠加 | DMA 硬件加速检测框渲染 | ✅ 已完成 |
| 底盘控制 | A1 GPIO UART → STM32 WHEELTEC C50X 协议 | ✅ 已完成 |
| Aurora 伴侣工具 | 图像/OSD 可视化 + CH347 固件烧录 | ✅ 已完成 |
| RPLidar 激光雷达 | 360° 点云采集与避障 | ⏸ 暂时禁用 |
| YOLOv8 目标检测 | 基于 NPU 的多类别检测 | ⏸ 暂时禁用 |
| ROS2 底盘控制 | UART 底盘驱动 + 导航 + SLAM | ⏸ 后续集成 |

## 仓库结构

```text
├── data/
│   ├── A1_SDK_SC132GS/          # SmartSens SDK（Buildroot + NPU 工具链）
│   └── yolov8_dataset/          # YOLOv8 训练数据集模板
├── docker/                      # Docker 编译环境（a1-sdk-builder）
├── docs/                        # 开发文档
├── models/                      # 模型文件（.m1model）
├── output/                      # 编译产物（EVB 固件）
├── scripts/                     # 构建脚本
├── src/
│   ├── a1_ssne_ai_demo/         # 主应用：人脸检测 + 底盘控制
│   │   ├── demo_face_drive.cpp  #   入口
│   │   ├── include/             #   头文件
│   │   │   ├── face_drive_app.hpp     # 应用主类
│   │   │   ├── chassis_controller.hpp # WHEELTEC 协议控制器
│   │   │   ├── project_paths.hpp      # 运行时配置
│   │   │   ├── common.hpp             # SCRFD/OSD 类型定义
│   │   │   └── ...
│   │   ├── src/                 #   源文件
│   │   │   ├── face_drive_app.cpp     # 应用主循环
│   │   │   ├── chassis_controller.cpp # GPIO UART 底盘通信
│   │   │   ├── scrfd_gray.cpp         # SCRFD 人脸检测
│   │   │   ├── pipeline_image.cpp     # 图像采集管道
│   │   │   ├── osd-device.cpp         # OSD 设备封装
│   │   │   └── utils.cpp
│   │   ├── app_assets/          #   板端资源（模型 + OSD LUT）
│   │   ├── cmake_config/        #   交叉编译路径配置
│   │   └── scripts/run.sh       #   板端启动脚本
│   ├── buildroot_pkg/           # Buildroot 外部包定义
│   ├── ros2_ws/                 # ROS2 工作区（后续集成）
│   └── stm32_akm_driver/       # STM32 AKM 控制板文档
├── third_party/
│   └── ultralytics/             # YOLOv8 训练框架
├── tools/
│   ├── aurora/                  # Aurora 伴侣工具（可视化 + 烧录）
│   └── yolov8/                  # 标注、划分、训练脚本
└── WHEELTEC_C50X_2025.12.26/    # WHEELTEC 小车 STM32 固件源码（Keil工程）
```

## 技术架构

```text
┌────────────────────────────────────────────┐
│            A1 开发板 (主循环)                │
├──────────┬──────────┬──────────────────────┤
│ 图像采集  │ 人脸检测  │     OSD 渲染          │
│ SC132GS  │ SCRFD    │   硬件叠加检测框       │
│ 720×1280 │ 640×480  │   DMA 图层            │
└────┬─────┴────┬─────┴──────────────────────┘
     │          │
     │          ▼
     │    人脸检测结果
     │          │
     │     ┌────┴─────┐
     │     │  驱动决策  │
     │     │ 有脸→前进  │
     │     │ 无脸→停车  │
     │     └────┬─────┘
     │          │
     │          ▼ GPIO UART0
     │   ┌──────────────┐
     │   │  STM32 AKM   │
     │   │ WHEELTEC C50X │
     │   │ 0x7B 协议帧   │
     │   └──────────────┘
     └───────────────────
```

### 硬件连接

| A1 端 | STM32 端 | 说明 |
|------|---------|------|
| GPIO_PIN_0 (UART0 TX) | UART3 RX (PB11) | A1 → STM32 指令 |
| GPIO_PIN_2 (UART0 RX) | UART3 TX (PB10) | STM32 → A1 状态 |
| GND | GND | 共地 |

### WHEELTEC C50X 协议

发送帧 (11 字节)：`[0x7B][Cmd][0x00][Vx_H][Vx_L][Vy_H][Vy_L][Vz_H][Vz_L][BCC][0x7D]`

- **Cmd**: `0x00` = 正常运动
- **Vx/Vy/Vz**: int16 速度 (mm/s)
- **BCC**: XOR(byte[0]..byte[8])

## 快速开始

### 环境要求

- Windows 10/11 + Docker Desktop 或 Linux + Docker Engine 24+
- A1 SDK Docker 镜像（`a1-sdk-builder:latest`）
- 磁盘空间：约 20GB（镜像 + SDK + 编译缓存）

### 1. 启动编译容器

```powershell
# 构建 Docker 镜像（首次）
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .

# 启动容器
docker compose -f docker/docker-compose.yml up -d
```

### 2. 编译

```powershell
# 全量编译（首次，含 SDK + Demo + ROS2）
docker exec A1_Builder bash -lc "bash /app/scripts/build_src_all.sh"

# 增量编译：仅 Demo
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_face_drive_demo"

# 增量编译：仅 ROS2 工作区
docker exec A1_Builder bash -lc "bash /app/scripts/build_ros2_ws.sh"
```

### 3. 生成 EVB 固件

```text
output/evb/zImage.smartsens-m1-evb  ← 编译产物
```

### 4. 板端运行

```bash
# SSH 进入 A1 开发板后
cd /app_demo
./scripts/run.sh
# 输出: [INFO] FaceDriveApp 初始化完成
# 输出: [INFO] 检测到人脸 → 直行 100 mm/s
```

### 5. Aurora 伴侣工具

```powershell
cd tools/aurora
.\launch.ps1           # 启动图像/OSD可视化
.\launch.ps1 -Flash    # CH347 固件烧录
.\launch.ps1 -Demo     # 演示模式（无需硬件）
```

## 运行时配置

集中在 `src/a1_ssne_ai_demo/include/project_paths.hpp`：

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `image_shape` | `{720, 1280}` | 传感器分辨率（H×W） |
| `crop_shape` | `{720, 540}` | 裁剪后输入尺寸 |
| `crop_offset_y` | `370` | 裁剪纵向偏移 |
| `det_shape` | `{640, 480}` | SCRFD 检测输入尺寸 |
| `confidence_threshold` | `0.4f` | SCRFD 置信度阈值 |
| `face_model_path` | `/app_demo/app_assets/models/face_640x480.m1model` | 板端模型路径 |
| `chassis_baudrate` | `115200` | UART 波特率 |

## 文档索引

### 入门

| 文档 | 内容 |
|------|------|
| [快速上手指南](docs/快速上手指南.md) | 新人必读：环境搭建 + 快速启动 |
| [编译手册](docs/BUILD.md) | SDK / Demo / ROS2 编译流程 |
| [常见问题](docs/常见问题.md) | 编译和运行问题排查 |

### 架构与硬件

| 文档 | 内容 |
|------|------|
| [双板架构设计](docs/双板架构设计.md) | A1 + STM32 通信与系统设计 |
| [硬件连接说明](docs/硬件连接说明.md) | A1 开发板接口定义 |
| [GPIO 引脚说明](docs/gpio.md) | A1 GPIO 引脚定义与 UART 映射 |
| [UART 驱动说明](docs/uart驱动.md) | A1 UART API 使用说明 |

### 开发参考

| 文档 | 内容 |
|------|------|
| [环境搭建指南](docs/环境搭建指南.md) | Docker + ROS2 环境详解 |
| [容器操作手册](docs/容器操作手册.md) | Docker 日常操作 |
| [STM32 控制板](src/stm32_akm_driver/README.md) | WHEELTEC C50X 固件与协议 |
| [ROS2 工作区](src/ros2_ws/README.md) | ROS2 包与构建 |
| [Aurora 伴侣工具](tools/aurora/README.md) | 可视化 + 固件烧录 |

### 暂缓模块

| 文档 | 内容 |
|------|------|
| [RPLidar 接入指南](docs/雷达SDK接入指南.md) | 激光雷达 SDK 集成（暂未安装） |
| [YOLOv8 训练指南](docs/YOLOv8训练指南.md) | 训练 → 导出 → 部署 |
| [数据集说明](data/yolov8_dataset/README.md) | YOLOv8 数据集格式 |

## License

本项目遵循 MIT 许可证。第三方组件遵循各自的许可协议。
