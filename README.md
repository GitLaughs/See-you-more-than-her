# A1 Vision Robot Stack

基于 SmartSens A1 开发板的智能视觉机器人软件栈，集成目标检测、人脸识别、激光雷达避障和 ROS2 底盘控制。

## 功能概览

| 模块 | 说明 | 状态 |
|------|------|------|
| YOLOv8 目标检测 | 基于 SSNE NPU 的多类别检测（人物/手势/障碍物） | 开发中 |
| SCRFD 人脸检测 | 灰度图多尺度人脸检测 | ✅ 已完成 |
| RPLidar 激光雷达 | 360° 点云采集与避障决策 | ✅ 已完成 |
| OSD 硬件叠加 | 多图层检测框渲染（DMA 硬件加速） | ✅ 已完成 |
| ROS2 底盘控制 | UART 底盘驱动 + 导航 + SLAM | ✅ 已完成 |
| 调试接口 | TCP JSON 数据流 → Aurora 桌面工具可视化 | ✅ 已完成 |
| Aurora 伴侣工具 | 三维点云 / 障碍区域 / 检测结果可视化 + CH347 固件烧录 | ✅ 已完成 |

## 仓库结构

```text
├── data/
│   ├── A1_SDK_SC132GS/          # SmartSens SDK（Buildroot + NPU 工具链）
│   └── yolov8_dataset/          # YOLOv8 训练数据集模板
├── docker/                      # Docker 容器配置（编译环境）
├── docs/                        # 详细开发文档
├── models/                      # 模型文件（.m1model / .onnx）
├── output/                      # 编译产物（EVB 固件）
├── scripts/                     # 构建脚本（全量 / 增量）
├── src/
│   ├── a1_ssne_ai_demo/         # 主应用（YOLOv8 + 人脸 + 雷达 + OSD）
│   ├── ros2_ws/                 # ROS2 工作区（STM32 AKM底盘 / 雷达 / 导航）
│   │   └── src/
│   │       ├── base_control_ros2/       # STM32 AKM UART驱动
│   │       ├── hardware_driver/         # RPLidar驱动
│   │       ├── a1_robot_stack/          # 硬件bringup
│   │       └── object_information_msgs_ros2/  # 消息定义
│   └── stm32_akm_driver/        # STM32 AKM控制板源代码和集成文档
├── third_party/
│   └── ultralytics/             # YOLOv8 训练框架
└── tools/
    ├── aurora/                  # Aurora 伴侣工具（点云/障碍/检测/烧录）
    └── yolov8/                  # 标注、划分、训练脚本
```

## 快速开始

> 📖 **新人必读**：[快速上手指南](docs/快速上手指南.md) - 零基础快速入门，30 分钟跑通开发环境

### 环境要求

- Windows 10/11 + Docker Desktop 或 Linux + Docker Engine 24+
- A1 SDK Docker 镜像（`a1-sdk-builder:latest`）
- 磁盘空间：约 20GB（镜像 + SDK + 编译缓存）
- 内存：建议 8GB+（分配给 Docker 至少 4GB）
- GPU 训练环境（可选）：Python 3.9 + CUDA 12.1 + RTX 显卡

### 1. 启动编译容器

```powershell
# 构建 Docker 镜像（首次）
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .

# 启动容器
docker compose -f docker/docker-compose.yml up -d

# 验证容器运行
docker compose ps
```

### 2. 编译 SDK 与应用

```powershell
# 全量编译（首次构建，约 15-20 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_src_all.sh"

# 增量编译：仅编译综合视觉 Demo
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_vision_demo"

# 增量编译：仅编译 ROS2 工作区
docker exec A1_Builder bash -lc "bash /app/scripts/build_ros2_ws.sh"
```

### 3. 生成 EVB 固件

编译完成后，可写入主板的固件位于：

```text
output/evb/zImage.smartsens-m1-evb  ← 推荐（收集后的产物）
data/A1_SDK_SC132GS/smartsens_sdk/output/images/zImage.smartsens-m1-evb
```

### 4. 启动 Aurora 伴侣工具

```powershell
# 启动 Aurora + 伴侣工具（图像/OSD + 点云/障碍/检测面板）
cd tools/aurora
.\launch.ps1

# 演示模式（无需连接硬件，模拟数据）
.\launch.ps1 -Demo

# 固件烧录（通过 CH347 SPI）
.\launch.ps1 -Flash

# 指定 A1 IP 地址
.\launch.ps1 -Host "10.0.0.50"
```

### 5. 训练 YOLOv8 模型（可选）

```powershell
# 在 Windows 本地执行（需要 GPU 环境）
cd tools\yolov8
pip install -r requirements.txt
.\train_gpu.ps1 -Model yolov8n.pt -Epochs 100 -Batch 16
```

## 文档索引

### 📚 新人入门（按阅读顺序）

| 优先级 | 文档 | 内容 | 阅读时间 |
|--------|------|------|---------|
| ⭐⭐⭐ | [快速上手指南](docs/快速上手指南.md) | **新人必读**：项目概览 + 环境搭建 + 快速启动 | 30 分钟 |
| ⭐⭐⭐ | [常见问题](docs/常见问题.md) | 编译、运行、调试常见错误及解决方案 | 随时查阅 |
| ⭐⭐ | [编译手册](docs/编译手册.md) | SDK / ROS2 / 增量编译完整流程 | 15 分钟 |
| ⭐⭐ | [双板架构设计](docs/双板架构设计.md) | A1 + STM32 通信协议和系统架构 | 20 分钟 |
| ⭐ | [环境搭建指南](docs/环境搭建指南.md) | Docker 环境 + ROS2 安装详细步骤 | 10 分钟 |

### 🔧 开发文档

| 文档 | 内容 |
|------|------|
| [容器操作手册](docs/容器操作手册.md) | Docker 日常操作和调试 |
| [YOLOv8 训练指南](docs/YOLOv8训练指南.md) | 标注 → 划分 → 训练 → 导出 |
| [RPLidar 接入指南](docs/雷达SDK接入指南.md) | 激光雷达 SDK 集成说明 |
| [硬件连接说明](docs/硬件连接说明.md) | A1 开发板接口定义 |

### 📦 模块说明

| 文档 | 内容 |
|------|------|
| [Demo 工程说明](src/a1_ssne_ai_demo/README.md) | 主应用架构与模块说明 |
| [ROS2 工作区](src/ros2_ws/README.md) | ROS2 包列表与构建方法 |
| [STM32 AKM 集成指南](src/stm32_akm_driver/README.md) | STM32F407 小车控制板接入 |
| [Aurora 伴侣工具](tools/aurora/README.md) | 点云/障碍/检测可视化 + 固件烧录 |
| [数据集说明](data/yolov8_dataset/README.md) | YOLOv8 数据集格式与工具 |

### 📋 项目管理

| 文档 | 内容 |
|------|------|
| [项目规划](docs/项目规划.md) | 任务分工与里程碑 |

## 技术架构

```text
┌──────────────────────────────────────────────────────────┐
│                    A1 开发板 (主循环)                      │
├──────────┬──────────┬──────────┬──────────┬──────────────┤
│ 图像采集  │ 人脸检测  │ YOLOv8   │ RPLidar  │ OSD 渲染     │
│ 720×1280 │ SCRFD    │ 目标检测  │ 360° 扫描 │ 硬件叠加     │
│ Y8 灰度   │ 640×480  │ 640×640  │ 串口采集  │ 5 DMA 图层   │
└────┬─────┴────┬─────┴────┬─────┴────┬─────┴──────┬───────┘
     │          │          │          │            │
     │          ▼          ▼          ▼            │
     │     检测结果    检测结果    点云数据         │
     │          │          │          │            │
     │          └────┬─────┴──────────┘            │
     │               ▼                             │
     │      TCP JSON 调试接口 ──→ Aurora 桌面工具    │
     │               │                             │
     │      ┌────────┴────────┐                    │
     │      │  3D 点云可视化    │                    │
     │      │  避障信息显示     │                    │
     │      │  检测框叠加      │                    │
     │      └─────────────────┘                    │
     └─────────────────────────────────────────────┘
```

## License

本项目遵循 MIT 许可证。第三方组件遵循各自的许可协议。
