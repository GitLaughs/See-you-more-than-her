# ROS2 Workspace 集成和编译指南

## 概述

本项目包含针对 SmartSens A1 SSNE 开发板的完整 ROS2 Jazzy 工作区。包含了来自 x3_src_250401 的精选 ROS 包，适配 0.8Tops 算力限制。

## 项目结构

```
src/
├── ros2_ws/                      # ROS2 工作区
│   ├── src/                      # ROS 包源代码
│   │   ├── wheeltec_robot_msg/   # 消息定义包
│   │   ├── turn_on_wheeltec_robot/ # 启动包
│   │   ├── wheeltec_multi/       # 多功能驱动
│   │   ├── wheeltec_robot_kcf/   # 目标跟踪
│   │   ├── wheeltec_robot_keyboard/ # 键盘控制
│   │   ├── wheeltec_robot_urdf/  # 机器人 URDF 模型
│   │   ├── wheeltec_rviz2/       # RViz 配置
│   │   ├── aruco_ros-humble-devel/ # ArUco 标记检测
│   │   ├── usb_cam-ros2/         # USB 摄像头驱动
│   │   ├── web_video_server-ros2/ # 网络视频传输
│   │   └── ...（其他包）
│   ├── build/                    # 编译输出目录
│   ├── install/                  # 安装目录
│   ├── log/                      # 编译日志
│   └── package.xml               # 工作区配置
│
├── stm32_akm_driver/             # STM32 AKM 小车驱动
│   ├── Keil/                     # STM32 Keil 工程
│   ├── README.md                 # 硬件接口说明
│   └── ...
│
└── a1_ssne_ai_demo/              # A1 主应用
```

## 已集成的 ROS 包

### P0 核心包（必需）

| 包名 | 功能 | 来源 |
|-----|------|------|
| `wheeltec_robot_msg` | ROS 消息类型定义 | x3_src_250401 |
| `turn_on_wheeltec_robot` | 小车启动和配置 | x3_src_250401 |
| `wheeltec_multi` | 底盘驱动和多传感器融合 | x3_src_250401 |
| `wheeltec_robot_keyboard` | 键盘遥控 | x3_src_250401 |

### P1 增强包（暂时屏蔽 — COLCON_IGNORE）

以下包已在各自目录放置 `COLCON_IGNORE` 文件，在执行 `colcon build` 时**不会被编译**，待硬件资源评估后按计划逐步启用：

| 包名 | 功能 | 算力需求 | 屏蔽原因 | 计划启用 |
|-----|------|---------|---------|---------|
| `wheeltec_robot_kcf` | 目标追踪（KCF 算法） | ~50MOPS | 算力受限 | Sprint 4 |
| `wheeltec_robot_urdf` | 机器人模型定义 | 无 | 依赖 RViz2 | Sprint 3 |
| `wheeltec_rviz2` | 可视化配置 | 无 | 无板端显示 | Sprint 3 |
| `aruco_ros` | ArUco 标记检测 | ~100MOPS | 超算力预算 | Sprint 4 |
| `usb_cam-ros2` | 摄像头驱动 | 无 | 驱动冲突 | Sprint 1 后评估 |
| `web_video_server-ros2` | 网络视频流 | ~50MOPS | 带宽受限 | Sprint 4 |

> 解屏蔽方法：删除对应包目录下的 `COLCON_IGNORE` 文件，再执行 `bash scripts/build_ros2_ws.sh`

## 编译环境要求

### Docker 环境（推荐）

```bash
# 启动 Docker 容器
cd docker/
docker-compose up -d a1-builder

# 进入容器
docker-compose exec a1-builder bash
```

### 本地环境

**依赖项：**
- ROS 2 Jazzy（来自官方源）
- colcon-core
- CMake >= 3.22
- GCC >= 11
- Python 3.11+

**安装依赖：**
```bash
# Ubuntu/Debian
sudo apt install -y \
  ros-jazzy-desktop \
  python3-colcon-common-extensions \
  python3-rosdep \
  cmake make g++

# 初始化 rosdep
sudo rosdep init
rosdep update
```

## 编译方法

### 1. 验证工作区配置

```bash
./scripts/build_verify.sh
```

输出应显示：
- ✓ 10+ ROS 包已发现
- ✓ ROS 2 Jazzy 环境可用
- ✓ 关键工具（cmake, make, colcon）可用

### 2. 构建 ROS2 工作区

**清洁构建（推荐首次）：**
```bash
./scripts/build_ros2_ws.sh --clean
```

**增量构建（仅重新编译修改的包）：**
```bash
./scripts/build_ros2_ws.sh
```

**构建特定包：**
```bash
./scripts/build_ros2_ws.sh wheeltec_robot_msg turn_on_wheeltec_robot
```

**详细输出：**
```bash
./scripts/build_ros2_ws.sh --verbose
```

### 3. 在 Docker 中构建

**完整编译（SDK + ROS2）：**
```bash
./scripts/build_docker.sh
```

**仅 ROS2 编译：**
```bash
./scripts/build_docker.sh --ros-only
```

**清洁编译：**
```bash
./scripts/build_docker.sh --clean
```

### 4. 增量编译

编辑源码后，可使用增量编译加快速度：

```bash
./scripts/build_incremental.sh ros --clean wheeltec_multi
```

### 5. 完整视觉栈编译

包含 SDK + Demo + ROS：

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
```

## 编译结果

成功的编译应产生：

```
src/ros2_ws/
├── build/           # 中间编译产物
├── install/         # 可安装的目标
│   ├── bin/         # 执行文件
│   ├── lib/         # 库文件
│   ├── share/       # 配置和数据文件
│   └── setup.bash   # 工作区环境设置
└── log/             # 编译日志
```

## 使用编译的 ROS 包

### 在本地使用

```bash
# 在新的 shell 中设置环境
source src/ros2_ws/install/setup.bash

# 列出所有可用包
ros2 pkg list

# 运行特定的 ROS node
ros2 run wheeltec_multi wheeltec_multi_node
```

### 在 Docker 中使用

容器已自动配置了工作区环境：

```bash
docker-compose exec a1-builder bash
ros2 run wheeltec_multi wheeltec_multi_node
```

## 常见问题解决

### 编译失败：找不到 ROS2 Jazzy

**原因：** ROS 2 Jazzy 未安装

**解决方案：**
```bash
# 在 Docker 中运行（推荐）
./scripts/build_docker.sh

# 或在本地安装 ROS2
sudo apt install ros-jazzy-desktop
```

### 编译失败：依赖缺失

**示例：**
```
cmake ERROR: Could not find a package configuration file provided by "sensor_msgs"
```

**解决方案：**
```bash
# 安装依赖
rosdep install --from-paths src --ignore-src -r -y
```

### 编译太慢

**原因：** 磁盘 I/O 慢或 CPU 核心数不足

**加速方案：**
```bash
# 使用并行编译（N = CPU 核数）
colcon build -j N --symlink-install
```

### 某个包编译失败，但其他包成功

**继续编译其他包：**
```bash
colcon build --continue-on-error --symlink-install
```

## 版本控制信息

- **ROS2 版本：** Jazzy
- **Python 版本：** 3.11+
- **CMake 最低版本：** 3.22
- **编译时间：** ~5-15 分钟（取决于硬件和是否清洁构建）

## 扩展阅读

- [ROS 2 Jazzy 官方文档](https://docs.ros.org/en/jazzy/)
- [colcon 构建工具文档](https://colcon.readthedocs.io/)
- [项目硬件架构文档](../../docs/双板架构设计.md)
- [STM32 AKM 小车集成](../docs/stm32_akm_driver/README.md)

## 许可证

本项目遵循相关 ROS 包的原始许可证。
