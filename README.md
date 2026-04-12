# A1 Vision Robot Stack

基于 SmartSens A1 开发板的智能视觉机器人软件栈。当前阶段为摄像头人脸检测 + WHEELTEC 底盘控制的硬件兼容性验证。

## 当前功能

| 模块 | 说明 | 状态 |
|------|------|------|
| SCRFD 人脸检测 | 灰度图多尺度人脸检测 (SSNE NPU 加速) | ✅ 已完成 |
| OSD 硬件叠加 | DMA 硬件加速检测框渲染 | ✅ 已完成 |
| 底盘控制 | A1 GPIO UART → STM32 WHEELTEC C50X 协议 | ✅ 已完成 |
| Aurora 拍照工具 | SC132GS 摄像头采集 + 训练集制作 | ✅ 已完成 |
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
│   ├── app_demo/ → data/.../app_demo  # SDK app_demo 挂载（核心应用）
│   │   └── face_detection/
│   │       └── ssne_ai_demo/
│   │           ├── demo_face.cpp      #   入口
│   │           ├── include/           #   头文件
│   │           │   ├── project_flow.hpp       # 应用主类
│   │           │   ├── chassis_controller.hpp # WHEELTEC 协议控制器
│   │           │   ├── project_paths.hpp      # 运行时配置
│   │           │   ├── common.hpp             # SCRFD/OSD 类型定义
│   │           │   └── utils.hpp              # OSD 绘制+标签
│   │           ├── src/               #   源文件
│   │           │   ├── project_flow.cpp       # 应用主循环
│   │           │   ├── chassis_controller.cpp # GPIO UART 底盘通信
│   │           │   ├── scrfd_gray.cpp         # SCRFD 人脸检测
│   │           │   ├── pipeline_image.cpp     # 图像采集管道
│   │           │   └── utils.cpp              # OSD 绘制
│   │           ├── app_assets/        #   板端资源（模型 + OSD LUT）
│   │           └── cmake_config/      #   交叉编译路径配置
│   ├── buildroot_pkg/           # Buildroot 外部包定义
│   ├── ros2_ws/                 # ROS2 工作区（后续集成）
│   └── stm32_akm_driver/       # STM32 AKM 控制板文档
├── third_party/
│   └── ultralytics/             # YOLOv8 训练框架
├── tools/
│   ├── aurora/                  # Aurora 拍照工具（SC132GS 摄像头采集）
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

### 2. 生成完整 EVB 镜像

```powershell
# 快速编译方式（推荐）— 跳过 ROS2，仅编译 SDK + Demo（约 20 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"

# 或：完整编译（含 SDK + Demo + ROS2，约 40 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"

# 或：增量编译（仅更新 Demo，快速迭代）
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_ai_demo"
```

### 3. 烧录到主板

```powershell
# 使用 Aurora 伴侣工具烧录
cd tools/aurora
.\launch.ps1 --flash ..\output\evb\zImage.smartsens-m1-evb
```

### 4. 板端验证

```bash
# SSH 进入 A1 开发板
ssh root@<A1_IP>

# 运行人脸检测 + 底盘控制 Demo
/app_demo/scripts/run.sh

# 预期输出：
# [INFO] FaceDriveApp 初始化完成
# [INFO] 检测到人脸 → 直行 100 mm/s
```

**详见** [03 编译与烧录指南](docs/03_编译与烧录.md)

### 5. Aurora 拍照工具

```powershell
cd tools/aurora
.\launch.ps1           # 启动 SC132GS 摄像头拍照
```

## 运行时配置

集中在 `src/app_demo/face_detection/ssne_ai_demo/include/project_paths.hpp`：

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
| --- | --- |
| [01 快速上手](docs/01_快速上手.md) | 新人必读：环境搭建 + 快速启动 |
| [02 环境搭建](docs/02_环境搭建.md) | Docker + SDK + ROS2 环境完整配置 |
| [03 编译与烧录](docs/03_编译与烧录.md) | SDK / Demo / ROS2 编译流程 + SDK 更新步骤 + EVB 烧录 |
| [04 容器操作](docs/04_容器操作.md) | Docker 日常操作命令 |
| [11 常见问题](docs/11_常见问题.md) | 编译和运行问题排查 |

### 架构与硬件

| 文档 | 内容 |
| --- | --- |
| [05 硬件参考](docs/05_硬件参考.md) | A1 接口定义、A1↔STM32 接线、GPIO API、UART API |
| [06 程序概览](docs/06_程序概览.md) | 系统架构、代码流程与新人导读 |
| [07 架构设计](docs/07_架构设计.md) | A1 + STM32 通信与系统设计 |

### 开发参考

| 文档 | 内容 |
| --- | --- |
| [08 ROS 底盘集成](docs/08_ROS底盘集成.md) | x3_src ROS 包集成与 0.8Tops 优化 |
| [09 AI 模型训练](docs/09_AI模型训练.md) | YOLOv8 训练 → 导出 → 部署 |
| [10 雷达集成](docs/10_雷达集成.md) | RPLidar SDK 接入（暂未安装） |
| [STM32 控制板](src/stm32_akm_driver/README.md) | WHEELTEC C50X 固件与协议 |
| [ROS2 工作区](src/ros2_ws/README.md) | ROS2 包与构建 |
| [Aurora 伴侣工具](tools/aurora/README.md) | 可视化 + 固件烧录 |

### 项目管理

| 文档 | 内容 |
| --- | --- |
| [12 项目规划](docs/12_项目规划.md) | 功能规划与分工 |
| [13 贡献指南](docs/13_贡献指南.md) | GitHub Issues 建议与贡献说明 |
| [数据集说明](data/yolov8_dataset/README.md) | YOLOv8 数据集格式 |

## License

本项目遵循 MIT 许可证。第三方组件遵循各自的许可协议。
