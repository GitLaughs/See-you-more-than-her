# 编译手册

本手册描述在 A1_Builder Docker 容器中编译 SmartSens SDK、`ssne_ai_demo`/`ssne_vision_demo` 及 ROS2 工作区，最终生成可写入主板的 EVB 固件的完整流程。

## 前提条件

- Windows + Docker Desktop（或 Linux + Docker Engine）
- 仓库根目录包含：`data/`、`src/`、`docker/`、`scripts/`、`models/`
- Docker 镜像 `a1-sdk-builder:latest` 已构建（见下方步骤 1）

## 目录挂载约定

| 主机路径 | 容器路径 | 说明 |
|---|---|---|
| `./data` | `/app/smartsens_sdk` | SmartSens SDK 源码 |
| `./src` | `/app/src` | C++ 源码与 ROS2 工作区 |
| `./models` | `/app/models` | NPU 模型文件 |
| `./output` | `/app/output` | 构建产物输出 |

## 步骤 1：构建 Docker 镜像

```powershell
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .
```

## 步骤 2：启动容器

```powershell
docker compose -f docker/docker-compose.yml up -d
docker compose -f docker/docker-compose.yml ps
```

容器名：`A1_Builder`

## 步骤 3：全量一键构建（推荐）

使用仓库新增的全量构建脚本，依次完成 SDK 基础库、SSNE Demo（两个目标）和 ROS2 工作区编译，并收集 EVB 产物：

```powershell
docker exec A1_Builder bash -lc "bash /app/src/scripts/build_vision_stack.sh"
```

脚本流程：

```
[1] 构建 SDK 基础库（build_release_sdk.sh）
    ↓
[2] 构建 ssne_ai_demo（人脸检测 Demo）
    ↓
[3] 构建 ssne_vision_demo（YOLOv8+OSD+雷达综合 Demo）
    ↓
[4] 构建 ROS2 工作区（colcon build）
    ↓
[5] 收集 EVB 产物到 output/evb/
```

## 步骤 4：增量构建（仅重编特定部分）

```powershell
# 只重编 ssne_ai_demo（人脸 Demo，改了 C++ 业务代码时用）
docker exec A1_Builder bash -lc "bash /app/src/scripts/build_incremental.sh sdk ssne_ai_demo"

# 只重编 ssne_vision_demo（综合视觉 Demo，改了 YOLOv8/OSD 等代码时用）
docker exec A1_Builder bash -lc "bash /app/src/scripts/build_incremental.sh sdk ssne_vision_demo"

# 只重编 ROS2 工作区
docker exec A1_Builder bash -lc "bash /app/src/scripts/build_incremental.sh ros"

# 只重编指定 ROS2 包
docker exec A1_Builder bash -lc "bash /app/src/scripts/build_incremental.sh ros ncnn_ros2 base_control_ros2"
```

## 步骤 5：查看产物

构建完成后检查以下路径：

```
data/A1_SDK_SC132GS/smartsens_sdk/output/images/
    └── zImage.smartsens-m1-evb    ← 板端固件（写入主板用）

output/evb/
    └── zImage.smartsens-m1-evb    ← 同上（收集副本）
```

> **注意**：`-evb` 是文件名后缀，不是 `.evb` 扩展名。

## 构建目标说明

| Buildroot 目标 | 产物 | 说明 |
|---|---|---|
| `ssne_ai_demo` | `/app_demo/ssne_ai_demo` | SCRFD 人脸检测 Demo |
| `ssne_vision_demo` | `/app_demo/ssne_vision_demo` | YOLOv8+OSD+雷达综合 Demo（新） |
| `ssne_m1_lib` | SDK 基础库 | SmartSens SSNE/OSD/Camera 库 |

## Buildroot 包配置

新增 `ssne_vision_demo` Buildroot 包，配置文件位于：

```
data/A1_SDK_SC132GS/smartsens_sdk/smart_software/package/ssne_vision_demo/
    ├── Config.in
    └── ssne_vision_demo.mk
```

编译时通过以下命令触发：

```bash
cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk
make BR2_EXTERNAL=./smart_software ssne_vision_demo
```

## 手动操作（调试场景）

```powershell
# 进入交互式容器
docker exec -it A1_Builder bash

# 在容器内执行
cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk

# 配置 defconfig
make BR2_EXTERNAL=./smart_software smartsens_m1pro_release_defconfig

# 重编 SDK 库
make m1_sdk_lib-rebuild

# 编译 ssne_vision_demo
make BR2_EXTERNAL=./smart_software ssne_vision_demo-rebuild

# 查看编译日志
less output/build/ssne_vision_demo/build.log
```

## 常见问题

| 问题 | 解决方法 |
|---|---|
| `apt-get` 失败 | 必须在容器（Ubuntu）内执行，不能在 Windows 主机执行 |
| `zImage.smartsens-m1-evb` 未生成 | 检查 `data/A1_SDK_SC132GS/smartsens_sdk/scripts/a1_sc132gs_build.sh` 日志 |
| ROS2 编译缺依赖 | 参考 [CONTAINER_OPERATION_GUIDE.md](CONTAINER_OPERATION_GUIDE.md) 中的 apt 安装命令 |
| SSNE 找不到模型 | 确认 `.m1model` 文件已放置到 `app_assets/models/` 并同步到容器 |

## 脚本索引

| 脚本 | 说明 |
|---|---|
| `scripts/build_vision_stack.sh` | 全量构建脚本（新增，推荐） |
| `scripts/build_incremental.sh` | 增量构建脚本 |
| `scripts/build_ros2_ws.sh` | ROS2 专项构建脚本 |
| `scripts/collect_evb_artifacts.sh` | 收集 EVB 产物 |
| `data/.../scripts/a1_sc132gs_build.sh` | SDK 原厂构建脚本 |
| `data/.../scripts/build_release_sdk.sh` | SDK 基础库构建脚本 |
