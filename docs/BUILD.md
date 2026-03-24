# 编译手册

在 A1_Builder Docker 容器中编译 SmartSens SDK、`ssne_face_drive_demo` 及 ROS2 工作区，最终生成 EVB 固件。

## 前提条件

- Windows + Docker Desktop（或 Linux + Docker Engine）
- 仓库根目录包含：`data/`、`src/`、`docker/`、`scripts/`、`models/`
- Docker 镜像 `a1-sdk-builder:latest` 已构建

## 目录挂载约定

| 主机路径 | 容器路径 | 说明 |
|---|---|---|
| `./data` | `/app/data` | SmartSens SDK 源码 |
| `./src` | `/app/src` | C++ 源码与 ROS2 工作区 |
| `./models` | `/app/models` | NPU 模型文件 |
| `./output` | `/app/output` | 构建产物输出 |
| `./scripts` | `/app/scripts` | 构建脚本 |

## 步骤 1：构建 Docker 镜像

```powershell
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .
```

## 步骤 2：启动容器

```powershell
docker compose -f docker/docker-compose.yml up -d
```

容器名：`A1_Builder`

## 步骤 3：完整 EVB 固件构建（推荐）

```powershell
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"
```

脚本流程：

```
[1] SDK 基础库（build_release_sdk.sh 第一次）
    ↓
[2] ssne_face_drive_demo（人脸检测 + 底盘控制）
    ↓
[3] ROS2 工作区（可选，--skip-ros 跳过）
    ↓
[4] 重新打包 zImage（build_release_sdk.sh 第二次，将最新应用写入 initramfs）
    ↓
[5] 产物保存到 output/evb/<YYYYMMDD_HHMMSS>/
```

> 每次构建产物均保存在独立的时间戳目录，`output/evb/latest` 软链接指向最近一次构建。

## 步骤 4：增量构建（开发迭代用）

```powershell
# 重编 Demo（改了 C++ 代码时用）
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk ssne_face_drive_demo"

# 重编 SDK 基础库
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh sdk m1_sdk_lib"

# 重编 ROS2 工作区
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh ros"

# 重编指定 ROS2 包
docker exec A1_Builder bash -lc "bash /app/scripts/build_incremental.sh ros base_control_ros2"
```

> 增量构建不会重打包 zImage。如需部署到板端，请使用 `build_complete_evb.sh`。

## 步骤 5：查看产物

```
output/evb/
    ├── latest/                         ← 软链接，指向最近一次构建
    └── 20260324_143022/                ← 时间戳目录（每次独立）
            ├── zImage.smartsens-m1-evb ← 板端固件（写入主板用）
            └── ssne_face_drive_demo    ← Demo 二进制
```

> **注意**：`-evb` 是文件名后缀，不是 `.evb` 扩展名。

## 构建目标说明

| Buildroot 目标 | 产物 | 说明 |
|---|---|---|
| `ssne_face_drive_demo` | `/app_demo/ssne_face_drive_demo` | 人脸检测 + 底盘控制 Demo |
| `m1_sdk_lib` | SDK 基础库 | SmartSens SSNE/OSD/GPIO/UART 库 |

## Buildroot 包配置

`ssne_face_drive_demo` 包定义位于：

```
src/buildroot_pkg/package/ssne_face_drive_demo/
    ├── Config.in
    └── ssne_face_drive_demo.mk
```

## 手动操作（调试场景）

```powershell
# 进入交互式容器
docker exec -it A1_Builder bash

# 在容器内
cd /app/data/A1_SDK_SC132GS/smartsens_sdk

# 重编 Demo
make BR2_EXTERNAL=./smart_software:/app/src/buildroot_pkg ssne_face_drive_demo-rebuild

# 查看编译日志
less output/build/ssne_face_drive_demo/build.log
```

## 常见问题

| 问题 | 解决方法 |
|---|---|
| `apt-get` 失败 | 必须在容器（Ubuntu）内执行，不能在 Windows 主机执行 |
| `zImage.smartsens-m1-evb` 未生成 | 检查 SDK scripts/a1_sc132gs_build.sh 日志 |
| ROS2 编译缺依赖 | 参考 [容器操作手册](容器操作手册.md) 中的 apt 安装命令 |
| SSNE 找不到模型 | 确认 `.m1model` 文件已放置到 `app_assets/models/` 并同步到容器 |

## P1 包屏蔽说明

以下 6 个 P1 增强包已通过 `COLCON_IGNORE` 文件**暂时屏蔽**编译，待板端资源评估后按 Sprint 计划逐步启用：

| 包名 | 目录 | 屏蔽原因 | 计划启用 |
|---|---|---|---|
| `wheeltec_robot_kcf` | `src/ros2_ws/src/wheeltec_robot_kcf/` | ~50MOPS，算力受限 | Sprint 4 |
| `wheeltec_robot_urdf` | `src/ros2_ws/src/wheeltec_robot_urdf/` | 依赖 RViz2，无显示环境 | Sprint 3 |
| `wheeltec_rviz2` | `src/ros2_ws/src/wheeltec_rviz2/` | 无板端显示环境 | Sprint 3 |
| `aruco_ros` | `src/ros2_ws/src/aruco_ros-humble-devel/` | ~100MOPS，超算力预算 | Sprint 4 |
| `usb_cam-ros2` | `src/ros2_ws/src/usb_cam-ros2/` | 与 SC132GS 驱动冲突 | Sprint 1 后评估 |
| `web_video_server-ros2` | `src/ros2_ws/src/web_video_server-ros2/` | ~50MOPS，带宽受限 | Sprint 4 |

**解屏蔽方法：**

```bash
# 解除单个包（以 wheeltec_robot_kcf 为例）
rm src/ros2_ws/src/wheeltec_robot_kcf/COLCON_IGNORE
# 重新编译
bash scripts/build_ros2_ws.sh
```

## 脚本索引

| 脚本 | 参考来源 | 说明 |
|---|---|---|
| `scripts/build_complete_evb.sh` | `a1_sc132gs_build.sh` | **完整 EVB 构建**（推荐），产物含时间戳目录 |
| `scripts/build_incremental.sh` | `build_app.sh` | 增量构建（开发迭代用） |
| `scripts/build_ros2_ws.sh` | `ros_a1_compile_test.sh` | ROS2 专项构建（含 P1 屏蔽提示） |
| `scripts/build_docker.sh` | — | Docker 容器内触发 build_complete_evb.sh |
| `data/.../scripts/a1_sc132gs_build.sh` | — | SDK 原厂构建脚本 |
| `data/.../scripts/build_release_sdk.sh` | — | SDK 基础库构建脚本（被 build_complete_evb.sh 调用两次） |
| `data/.../scripts/ros_a1_compile_test.sh` | — | ROS 编译测试参考脚本 |
