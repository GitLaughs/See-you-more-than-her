# A1 Vision Robot Stack

基于 SmartSens A1 开发板的智能视觉机器人软件栈。支持 SCRFD/YOLOv8 双推理后端、A1_TEST 串口调试协议、Link-Test 联通性测试。

## 当前功能

| 模块 | 说明 | 状态 |
|------|------|------|
| SCRFD 人脸检测 | 灰度图多尺度人脸检测（SSNE NPU 加速） | ✅ 已完成（默认） |
| YOLOv8 目标检测 | 640×360 缩放 + NPU 推理，head6 切分 + CPU 后处理 | ✅ 已完成（可选） |
| OSD 硬件叠加 | DMA 硬件加速检测框渲染 | ✅ 已完成 |
| 底盘控制 | A1 GPIO UART → STM32 WHEELTEC C50X 协议 | ✅ 已完成 |
| A1_TEST 调试协议 | 通过 COM13 下发控制和查询状态 | ✅ 已完成 |
| Link-Test 模式 | 周期前进停车，验证 A1↔STM32 通信链路 | ✅ 已完成 |
| Aurora Companion | 摄像头采集预览 + 底盘调试 UI（双模式：直连/经由 A1） | ✅ 已完成 |
| RPLidar 激光雷达 | 360° 点云采集与避障 | ⏸ 暂时禁用 |
| ROS2 底盘控制 | UART 底盘驱动 + 导航 + SLAM | ⏸ 后续集成 |

## 仓库结构

> **注意**: `output/` 目录受 `.gitignore` 排除，不会进入版本库。EVB 固件产物（`ssne_ai_demo`、`zImage`）需在本地运行构建脚本生成，详见协作与代码同步。

```text
├── data/
│   ├── A1_SDK_SC132GS/          # SmartSens SDK（Buildroot + NPU 工具链）
│   │   └── smartsens_sdk/
│   │       └── smart_software/
│   │           └── src/
│   │               └── app_demo/
│   │                   └── face_detection/
│   │                       └── ssne_ai_demo/   # 板端应用（核心）
│   └── yolov8_dataset/          # YOLOv8 训练数据集模板
├── docker/                      # Docker 编译环境（a1-sdk-builder）
├── docs/                        # 开发文档
├── models/                      # 模型文件（.onnx / .m1model，已纳入版本库）
├── output/                      # ⚠ gitignored — 编译产物，不在版本库中
│   └── evb/<YYYYMMDD_HHMMSS/   # 每次构建的带时间戳产物目录
│       ├── ssne_ai_demo         # ARM ELF 应用二进制
│       └── zImage.smartsens-m1-evb  # 完整内核镜像（内嵌应用，可直接烧录）
├── scripts/                     # 构建脚本
├── src/
│   ├── buildroot_pkg/           # Buildroot 外部包定义
│   ├── ros2_ws/                 # ROS2 工作区（后续集成）
│   └── stm32_akm_driver/       # STM32 AKM 控制板文档
├── tools/
│   └── aurora/                  # Aurora 工具集合
│       ├── aurora_companion.py    # 主工具（摄像头预览 + 底盘调试 UI）
│       ├── serial_terminal.py      # A1 调试串口终端
│       └── ...
├── third_party/
│   └── ultralytics/             # YOLOv8 训练框架（⚠ gitignored）
└── WHEELTEC_C50X_2025.12.26/    # WHEELTEC 小车 STM32 固件（⚠ gitignored）
```

## 板端应用（ssne_ai_demo）

### 核心特性

- **双推理后端**：SCRFD（默认）/ YOLOv8，通过 `USE_SCRFD_BACKEND` 切换
- **A1_TEST 串口协议**：支持调试和控制
- **Link-Test 联通性测试**：周期前进停车
- **OSD 硬件叠加**：检测框渲染
- **WHEELTEC 底盘控制**：0x7B 协议帧

### 关键配置

配置位于 `project_paths.hpp`：

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `SENSOR_WIDTH / SENSOR_HEIGHT` | 720 × 1280 | 传感器竖屏采集分辨率（Y8） |
| `DET_WIDTH / DET_HEIGHT` | 640 × 360 | 推理输入分辨率 |
| `USE_SCRFD_BACKEND` | true | 使用 SCRFD 后端 |
| `MODEL_PATH` | `/app_demo/app_assets/models/best_a1_formal_head6.m1model` | 板端模型路径 |
| `DET_CONF_THRESH` | 0.4f | 检测置信度阈值 |
| `DET_NMS_THRESH` | 0.45f | NMS IoU 阈值 |
| `LINK_TEST_ENABLED` | true | 默认开启联通测试 |
| `LINK_TEST_FORWARD_VX` | 60 | 联通测试前进速度（mm/s） |

## Aurora Companion 工具

### 功能

- **摄像头预览**：支持 A1/Windows 摄像头、旋转、截图、画廊
- **双模式底盘调试**：
  - 直连 STM32
  - 经由 A1（使用 A1_TEST 协议）
- **A1 调试终端**：内置串口终端，支持发送 A1_TEST 命令
- **自动重连**：断线后自动恢复

### A1_TEST 命令

| 命令 | 说明 |
|------|------|
| `help` | 查看命令列表 |
| `status` | 系统状态查询 |
| `A1_TEST test_echo <msg>` | 回显测试 |
| `A1_TEST debug_status` | 查询调试状态 |
| `A1_TEST link_test on/off` | 开关联通测试 |
| `A1_TEST stop` | 停车 |
| `A1_TEST move <vx> <vy> <vz>` | 手动运动控制 |

## 技术架构

```text
┌─────────────────────────────────────────────────────────────────────┐
│                        A1 开发板（主循环）                        │
├──────────────┬──────────────┬──────────────────────────────────┤
│ 图像采集 │ 检测/推理 │ OSD 渲染 │
│ SC132GS │ SCRFD/YOLOv8 │ 硬件叠加检测框 │
│ 720×1280 │ 640×360 │ DMA 图层 │
└───────┬───┴───────┬───┴──────────────────────────────────┘
        │           │
        │           ▼
        │   检测结果 / Link-Test
        │           │
        │   ┌───────┴───────┐
        │   │  决策模块   │
        │   │ Link-Test   │
        │   └───────┬───────┘
        │           │
        │           ▼ GPIO UART0
        │   ┌─────────────────────┐
        │   │   STM32 AKM    │
        │   │  WHEELTEC C50X │
        │   │ 0x7B 协议帧    │
        │   └─────────────────────┘
        │
        ▼ COM13 调试串口
    ┌─────────────────────────────────┐
    │   Aurora Companion (PC 侧）  │
    │  摄像头预览 + 底盘调试 UI     │
    └─────────────────────────────────┘
```

### 硬件连接

| A1 端 | STM32 端 | 说明 |
|------|---------|------|
| GPIO_PIN_0 (UART0 TX) | PB11 (UART3 RX) | A1 → STM32 控制 |
| GPIO_PIN_2 (UART0 RX) | PB10 (UART3 TX) | STM32 → A1 状态 |
| GND | GND | 共地 |

| PC 端 | A1 端 | 说明 |
|------|-------|------|
| USB 串口（COM13） | A1 调试串口 | A1_TEST 协议通信 |

### WHEELTEC C50X 协议

发送帧（11 字节）：`[0x7B][Cmd][0x00][Vx_H][Vx_L][Vy_H][Vy_L][Vz_H][Vz_L][BCC][0x7D]

- **Cmd**：`0x00` = 正常运动
- **Vx/Vy/Vz**：int16 速度（mm/s）
- **BCC**：XOR(byte[0]..byte[8])

## 快速开始

### 环境要求

- Windows 10/11 + Docker Desktop 或 Linux + Docker Engine 24+
- A1 SDK Docker 镜像（`a1-sdk-builder:latest`）
- 磁盘空间：约 20GB（镜像 + SDK + 编译缓存）
- Python 3.9+（Windows 侧工具）

### 1. 启动编译容器

```powershell
# 构建 Docker 镜像（首次）
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .

# 启动容器
docker compose -f docker/docker-compose.yml up -d
```

### 2. 生成完整 EVB 镜像

```powershell
# 完整构建（首次或 SDK 基础库变更时）— 约 30-40 分钟
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"

# 快速重编 Demo + zImage（代码迭代时使用，约 5-10 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

产物自动写入 `output/evb/<timestamp>/`，包含：
- `ssne_ai_demo` — ARM ELF 应用二进制
- `zImage.smartsens-m1-evb` — 完整内核镜像（已内嵌最新应用，用于烧录）

### 3. 烧录到主板

使用官方 Aurora.exe 工具烧录 `zImage.smartsens-m1-evb`。

### 4. 板端验证

```bash
# SSH 进入 A1 开发板
ssh root@<A1_IP>

# 运行 ssne_ai_demo
/app_demo/scripts/run.sh
```

运行后可通过键盘输入命令：
- `help`：查看命令列表
- `q`：退出程序
- `status`：查看状态

### 5. Aurora Companion（Windows 侧调试）

```powershell
cd tools/aurora

# 启动主工具（推荐）
.\launch.ps1
```

访问 http://localhost:5001

功能：
- 摄像头预览（支持 A1/Windows 摄像头）
- 双模式底盘调试（直连/经由 A1）
- A1 调试终端（A1_TEST 协议）
- Link-Test 开关控制

## 文档索引

### 入门

| 文档 | 内容 |
|------|------|
| [docs/01_快速上手.md | 新人必读：环境搭建 + 快速启动 |
| [docs/02_环境搭建.md | Docker + SDK + ROS2 环境完整配置 |
| [docs/03_编译与烧录.md | SDK / Demo / ROS2 编译流程 + SDK 更新步骤 + EVB 烧录 |
| [docs/04_容器操作.md | Docker 日常操作命令 |
| [docs/11_常见问题.md | 编译和运行问题排查 |
| [tools/aurora/README.md | Aurora 工具使用说明 |
| [data/.../ssne_ai_demo/README.md | 板端应用说明 |

### 架构与硬件

| 文档 | 内容 |
|------|------|
| [docs/05_硬件参考.md | A1 接口定义、A1↔STM32 接线、GPIO API、UART API |
| [docs/06_程序概览.md | 系统架构、代码流程与新人导读 |
| [docs/07_架构设计.md | A1 + STM32 通信与系统设计 |
| [src/stm32_akm_driver/README.md | WHEELTEC C50X 固件与协议 |

### 开发参考

| 文档 | 内容 |
|------|------|
| [docs/08_ROS底盘集成.md | x3_src ROS 包集成与 0.8Tops 优化 |
| [docs/09_AI模型训练.md | YOLOv8 训练 → 导出 → 部署 |
| [docs/10_雷达集成.md | RPLidar SDK 接入（暂未安装） |
| [docs/15_AI模型转换与部署.md | 模型导出、切分、后处理实现、部署指南 |
| [docs/16_A1深度感知与点云避障方案.md | 深度感知与避障方案 |

### 项目管理

| 文档 | 内容 |
|------|------|
| [docs/12_项目规划.md | 功能规划与分工 |
| [docs/13_贡献指南.md | GitHub Issues 建议与贡献说明 |
| [docs/14_后续开发建议.md | 后续开发建议 |
| [data/yolov8_dataset/README.md | YOLOv8 数据集格式 |

## 协作与代码同步

### Git 追踪范围速览

理解哪些文件在 git 里、哪些被忽略，是避免协作冲突的基础。

| 路径 | Git 状态 | 说明 |
|------|----------|------|
| `src/`, `data/A1_SDK_SC132GS/`, `docs/`, `tools/`, `scripts/`, `docker/` | ✅ 已追踪 | 源码、脚本、文档；`git pull` 可自动更新 |
| `models/*.onnx`, `models/*.m1model` | ✅ 已追踪 | 模型文件随源码一起提交 |
| `output/` | ❌ gitignored | 编译产物（EVB 固件、日志），需本地构建生成 |
| `third_party/ultralytics/` | ❌ gitignored | 训练框架，需手动克隆或用 `bootstrap.sh` 初始化 |
| `WHEELTEC_C50X_2025.12.26/` | ❌ gitignored | 硬件厂商固件，不进入版本库 |
| `src/ros2_ws/build/`, `src/ros2_ws/install/` | ❌ gitignored | ROS2 编译缓存，需本地编译生成 |
| `data/yolov8_dataset/raw/` | ❌ gitignored | 原始训练图片（体积大），需另行传输 |

> **快速判断某文件是否被追踪**:
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

> **适用于**: 本地改乱了某些被追踪的文件，想完全还原到远端版本。

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

由于 `output/` 完全被忽略，协作者拉取代码后 **不会自动获得 EVB 固件**，必须自行构建：

```powershell
# 首次或 SDK 基础库变更后（约 30-40 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"

# 只改了应用代码（ssne_ai_demo），快速重编 + 打包 zImage（约 5-10 分钟）
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

产物位于 `output/evb/<timestamp>/`：
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

## License

本项目遵循 MIT 许可证。第三方组件遵循各自的许可协议。
