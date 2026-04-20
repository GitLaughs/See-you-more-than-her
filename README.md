# A1 Vision Robot Stack

基于 SmartSens A1 开发板的智能视觉机器人软件栈。当前阶段为摄像头人脸检测 + WHEELTEC 底盘控制的硬件兼容性验证。

## 当前功能

| 模块 | 说明 | 状态 |
|------|------|------|
| SCRFD 人脸检测 | 灰度图多尺度人脸检测 (SSNE NPU 加速)（已被YOLOv8替代） | ⏸ 已弃用 |
| OSD 硬件叠加 | DMA 硬件加速检测框渲染 | ✅ 已完成 |
| 底盘控制 | A1 GPIO UART → STM32 WHEELTEC C50X 协议 | ✅ 已完成 |
| Aurora 拍照工具 | SC132GS 摄像头采集 + 训练集制作 + **STM32 底盘调试 UI** | ✅ 已完成 |
| RPLidar 激光雷达 | 360° 点云采集与避障 | ⏸ 暂时禁用 |
| YOLOv8 目标检测 | 1280×720 → 640×360缩放 → NPU推理，4类别，head6切分+CPU后处理 | ✅ 已完成 |
| ROS2 底盘控制 | UART 底盘驱动 + 导航 + SLAM | ⏸ 后续集成 |

## 仓库结构

> **注意**：`output/` 目录受 `.gitignore` 排除，不会进入版本库。EVB 固件产物（`ssne_ai_demo`、`zImage`）需在本地运行构建脚本生成，详见[协作与代码同步](#协作与代码同步)。

```text
├── data/
│   ├── A1_SDK_SC132GS/          # SmartSens SDK（Buildroot + NPU 工具链）
│   └── yolov8_dataset/          # YOLOv8 训练数据集模板
├── docker/                      # Docker 编译环境（a1-sdk-builder）
├── docs/                        # 开发文档
├── models/                      # 模型文件（.onnx / .m1model，已纳入版本库）
├── output/                      # ⚠ gitignored — 编译产物，不在版本库中
│   └── evb/<YYYYMMDD_HHMMSS>/   #   每次构建的带时间戳产物目录
│       ├── ssne_ai_demo         #     ARM ELF 应用二进制
│       └── zImage.smartsens-m1-evb  # 完整内核镜像（内嵌应用，可直接烧录）
├── scripts/                     # 构建脚本（6 个，见下方说明）
├── src/
│   ├── app_demo/ → data/.../app_demo  # SDK app_demo 挂载（核心应用）
│   │   └── face_detection/
│   │       └── ssne_ai_demo/
│   │           ├── demo_face.cpp      #   入口（主循环：采图→检测→底盘控制→OSD）
│   │           ├── project_paths.hpp  #   全局配置（模型路径、YOLOv8常量、阈值、波特率等）
│   │           ├── include/           #   头文件
│   │           │   ├── chassis_controller.hpp # WHEELTEC 协议控制器
│   │           │   ├── common.hpp             # YOLOV8/SCRFD 类型定义
│   │           │   ├── osd-device.hpp         # VISUALIZER OSD 绘制类
│   │           │   └── utils.hpp              # NMS / 排序工具
│   │           ├── src/               #   源文件
│   │           │   ├── chassis_controller.cpp # GPIO UART 底盘通信
│   │           │   ├── yolov8_gray.cpp        # YOLOv8 head6 + DFL decode + NMS
│   │           │   ├── scrfd_gray.cpp         # SCRFD 人脸检测（保留备用）
│   │           │   ├── pipeline_image.cpp     # 全分辨率采集 1280×720（无裁剪）
│   │           │   ├── osd-device.cpp         # OSD 绘制实现
│   │           │   └── utils.cpp              # NMS / 排序工具实现
│   │           ├── app_assets/        #   板端资源（模型 + OSD LUT）
│   │           └── cmake_config/      #   交叉编译路径配置
│   ├── buildroot_pkg/           # Buildroot 外部包定义
│   ├── ros2_ws/                 # ROS2 工作区（后续集成）
│   └── stm32_akm_driver/       # STM32 AKM 控制板文档
├── third_party/
│   └── ultralytics/             # YOLOv8 训练框架（⚠ gitignored）
├── tools/
│   ├── aurora/                  # Aurora 拍照工具（SC132GS 摄像头采集）
│   └── yolov8/                  # 标注、划分、训练脚本
└── WHEELTEC_C50X_2025.12.26/    # WHEELTEC 小车 STM32 固件（⚠ gitignored）
```

### scripts/ 目录说明

| 脚本 | 用途 |
|------|------|
| `build_complete_evb.sh` | ⭐ 主构建：完整 EVB（SDK + 应用 + zImage），支持 `--app-only` 快速重编 |
| `build_incremental.sh` | 仅重编指定应用，不打包 zImage（调试用） |
| `bootstrap.sh` | 新成员一键初始化开发环境 |
| `build_docker.sh` | 构建 Docker 编译容器 |
| `build_ros2_ws.sh` | 编译 ROS2 工作区 |
| `install_ros2_jazzy.sh` | 容器内安装 ROS2 Jazzy |

## 技术架构

```text
┌────────────────────────────────────────────┐
│            A1 开发板 (主循环)                │
├──────────┬──────────┬──────────────────────┤
│ 图像采集  │ 人脸检测  │     OSD 渲染          │
│ SC132GS  │ SCRFD    │   硬件叠加检测框       │
│ 1280×720 │ 640×360  │   DMA 图层            │
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
# ① 完整构建（首次或 SDK 基础库变更时）— 约 30-40 分钟
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"

# ② 快速重编 Demo + zImage（代码迭代时使用，约 5-10 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

产物自动写入 `output/evb/<时间戳>/`，包含：
- `ssne_ai_demo` — ARM ELF 应用二进制
- `zImage.smartsens-m1-evb` — 完整内核镜像（已内嵌最新应用，用于烧录）

### 3. 烧录到主板

```powershell
# 使用 burn_tool 烧录（launch.ps1 仅用于 Windows 测试）
docker exec A1_Builder bash -lc "cd /app/data/A1_SDK_SC132GS/smartsens_sdk && ./tools/burn_tool/x86_linux/burn_tool -f /app/output/evb/<时间戳>/zImage.smartsens-m1-evb"
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

# 基础拍照工具 (端口 5000)
python aurora_capture.py

# 增强伴侣工具（美化界面 + 断联恢复 + STM32 底盘调试）
python aurora_companion.py  # 访问 http://localhost:5001
```

aurora_companion.py 提供双 Tab 界面：
- **摄像头采集**：实时预览、拍照、缩略图画廊
- **底盘调试**：串口连接、运动控制（WASD 键盘）、遥测显示、帧日志

## 协作与代码同步

### Git 追踪范围速查

理解哪些文件在 git 里、哪些被忽略，是避免协作冲突的基础。

| 路径 | Git 状态 | 说明 |
|------|----------|------|
| `src/`, `data/A1_SDK_SC132GS/`, `docs/`, `tools/`, `scripts/`, `docker/` | ✅ 已追踪 | 源码、脚本、文档；`git pull` 可自动更新 |
| `models/*.onnx`, `models/*.m1model` | ✅ 已追踪 | 模型文件随源码一起提交 |
| `output/` | ❌ gitignored | 编译产物（EVB 固件、日志），**需本地构建生成** |
| `third_party/ultralytics/` | ❌ gitignored | 训练框架，需手动克隆或用 `bootstrap.sh` 初始化 |
| `WHEELTEC_C50X_2025.12.26/` | ❌ gitignored | 硬件厂商固件，不进入版本库 |
| `src/ros2_ws/build/`, `src/ros2_ws/install/` | ❌ gitignored | ROS2 编译缓存，需本地编译生成 |
| `data/yolov8_dataset/raw/` | ❌ gitignored | 原始训练图片（体积大），需另行传输 |

> **快速判断某文件是否被追踪**：
> ```powershell
> git ls-files --error-unmatch <文件路径>
> # 有输出 = 已追踪；报错 "did not match" = gitignored 或未添加
> ```

---

### 拉取最新代码（标准流程）

```powershell
# 1. 检查本地是否有未提交的修改
git status

# 2. 有修改时先暂存
git stash          # 暂存（pull 后可用 git stash pop 恢复）
# 或提交到本地
git add . && git commit -m "WIP: 保存本地进度"

# 3. 拉取远端最新
git fetch origin

# 4a. 在 main 分支：快进合并
git checkout main
git merge --ff-only origin/main

# 4b. 在工作分支：变基到最新 main
git checkout <你的分支>
git rebase origin/main
```

---

### 强制用远端代码覆盖本地修改

> **适用于**：本地改乱了某些被追踪的文件，想完全还原到远端版本。

```powershell
# 丢弃指定文件的本地修改（还原到最近一次提交）
git restore <文件路径>

# 丢弃所有未提交的本地修改（危险！不可恢复）
git restore .

# 强制同步到远端分支（包括 origin/main 上没有的本地提交也会被覆盖）
git fetch origin
git reset --hard origin/main    # 慎用：会丢失所有本地提交

# 只重置某个子目录下的文件
git checkout origin/main -- src/app_demo/
```

> ⚠ `git reset --hard` 和 `git restore .` 只影响 **已追踪的文件**，不会删除 gitignored 文件（如 `output/`）。

---

### gitignored 文件的处理

由于 `output/` 完全被忽略，协作者拉取代码后**不会自动获得 EVB 固件**，必须自行构建：

```powershell
# 首次或 SDK 基础库变更后（约 30-40 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"

# 只改了应用代码（ssne_ai_demo），快速重编 + 打包 zImage（约 5-10 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

产物位于 `output/evb/<时间戳>/`：
- `ssne_ai_demo` — 应用二进制
- `zImage.smartsens-m1-evb` — **完整内核镜像**，可直接烧录到 A1 EVB

> `models/` 中的 `.onnx` 和 `.m1model` 文件**已纳入 git**，`git pull` 会自动更新，无需手动处理。

---

### 常见协作陷阱

| 场景 | 现象 | 解决方法 |
|------|------|----------|
| 拉取后 `output/evb/` 没有新固件 | `output/` 在 gitignore，pull 不影响它 | 运行 `build_complete_evb.sh --app-only` |
| 本地有 `output/evb/` 旧固件，想清空 | git 不会清理 gitignored 文件 | 手动 `Remove-Item output\evb\* -Recurse` |
| 拉取后发现 `.gitignore` 本身被修改 | 新规则只对未追踪文件生效 | 已被追踪的文件需 `git rm --cached <路径>` 取消追踪 |
| `third_party/ultralytics/` 缺失 | gitignored，不在仓库中 | 运行 `bash scripts/bootstrap.sh` 初始化 |
| rebase 时出现冲突 | 本地和远端修改了同一文件 | 解决后 `git add <文件>` + `git rebase --continue` |
| 误删已追踪的文件想恢复 | 文件被删除但未提交 | `git restore <文件路径>` |

## 运行时配置

集中在 `src/app_demo/face_detection/ssne_ai_demo/include/project_paths.hpp`：

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `image_shape` | `{1280, 720}` | 传感器全分辨率（W×H） |
| `det_shape` | `{640, 360}` | SCRFD 推理输入尺寸（RunAiPreprocessPipe 缩放） |
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
| Aurora 伴侣工具 | [tools/aurora/README.md](tools/aurora/README.md) | 摄像头采集 + 底盘调试 UI |

### 项目管理

| 文档 | 内容 |
| --- | --- |
| [12 项目规划](docs/12_项目规划.md) | 功能规划与分工 |
| [13 贡献指南](docs/13_贡献指南.md) | GitHub Issues 建议与贡献说明 |
| [数据集说明](data/yolov8_dataset/README.md) | YOLOv8 数据集格式 |

## License

本项目遵循 MIT 许可证。第三方组件遵循各自的许可协议。
