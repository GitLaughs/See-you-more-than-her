# A1 Vision Robot Stack

这个仓库把 A1 平台的视觉、雷达和 ROS2 入口收拢到一起，目标是让新同事能在容器里快速复现构建结果。

## 项目目标

- YOLOv8 训练与部署（ONNX）
- 手势识别、目标跟踪、视觉检测
- 避障与底盘控制（GPIO、串口）
- 深度感知与环境点云生成
- 前端视频输出与可视化

## 仓库结构

```text
。
├── docker/                              # 容器构建与运行配置
├── data/                                # SDK 与训练数据模板
├── models/                              # 模型文件（ONNX、训练导出）
├── src/
│   ├── a1_ssne_ai_demo/                 # SDK demo 与基础开发骨架
│   └── ros2_ws/                         # ROS2 工作区（仅上游仓库）
│       └── src/
│           ├── base_control_ros2/
│           ├── hardware_driver/
│           ├── bingda_ros2_demos/
│           ├── ncnn_ros2/
│           ├── depend_pkg/
│           └── object_information_msgs_ros2/
└── docs/
```

## Docker 配置核对结果

仓库默认的 Docker 挂载关系如下：

- 容器名是 A1_Builder
- 服务名是 dev
- 镜像名是 a1-sdk-builder:latest
- 挂载关系正常：
  - data 挂载到 /app/smartsens_sdk
  - src 挂载到 /app/src
  - models 挂载到 /app/models
  - output 挂载到 /app/output

因此本文里的 `docker exec A1_Builder ...` 命令可以直接对照当前配置使用。

## 项目文档索引

下面这些 README 是按功能拆开的，建议新队友按这个顺序看：

- [SDK 总说明](data/A1_SDK_SC132GS/README.md)
- [SDK demo 人脸检测工程](src/a1_ssne_ai_demo/README.md)
- [ROS2 工作区说明](src/ros2_ws/README.md)
- [容器操作手册](docs/CONTAINER_OPERATION_GUIDE.md)
- [详细编译手册](docs/BUILD.md)
- [增量编译脚本](scripts/build_incremental.sh)
- [RPLidar SDK 接入指南](docs/RPLIDAR_SDK_GUIDE.md)
- [YOLOv8 训练说明](docs/YOLOV8_TRAINING.md)
- [数据集说明](data/yolov8_dataset/README.md)
- [YDLidar 驱动说明](src/ros2_ws/src/hardware_driver/lidar/ydlidar_ros2_driver/README.md)
- [RPLidar ROS2 驱动说明](src/ros2_ws/src/hardware_driver/lidar/rplidar_ros2/README.md)
- [SLLidar 驱动说明](src/ros2_ws/src/hardware_driver/lidar/sllidar_ros2/README.md)
- [NVILidar 驱动说明](src/ros2_ws/src/hardware_driver/lidar/nvilidar_ros2/README.md)
- [GMapping 说明](src/ros2_ws/src/depend_pkg/slam_gmapping/README.md)

## ROS 源码替换结果

当前 `src/ros2_ws/src` 已替换为你提供的上游仓库集合：

- `base_control_ros2`
- `hardware_driver`
- `hardware_driver/lidar/rplidar_ros2`
- `bingda_ros2_demos`
- `ncnn_ros2`
- `depend_pkg`
- `object_information_msgs_ros2`

`hardware_driver/lidar/rplidar_ros_upstream` 是官方 ROS1 仓库的参考副本，不参与当前 ROS2 编译。

自研的 `a1_robot_stack` 已从工作区删除，不再作为参考保留。

## 从零开始安装 SDK 和 ROS（给新队友的完整流程）

下面这套流程假设你是第一次接触这个仓库，只想在 Windows + Docker Desktop 环境里把 SDK、ROS2 和对应固件都跑起来。

### 1. 准备本地环境

- Windows 10/11
- Docker Desktop
- Git
- Python 3.9 或更高版本（如果还要做 YOLOv8 训练）

### 2. 克隆仓库

```powershell
git clone https://github.com/GitLaughs/See-you-more-than-her.git
cd See-you-more-than-her
```

### 3. 启动 Docker 容器

```powershell
docker compose -f docker/docker-compose.yml up -d
docker compose -f docker/docker-compose.yml ps
```

容器名固定为 `A1_Builder`。如果需要停止容器，执行：

```powershell
docker compose -f docker/docker-compose.yml down
```

### 4. 确认 SDK 源码已经在位

SDK 源码目录在 `data/A1_SDK_SC132GS`。如果你拿到的是一个不完整的工作区，请先把这个目录同步完整，再继续后面的编译步骤。

如果团队需要锁定 SDK 版本，再按约定的 tag 或 commit 固定即可；日常开发默认跟随当前仓库主分支。

### 5. 进入容器后先做一次基础清理

Windows 挂载目录有时会把换行改坏，导致 `bash\r` 报错。首次接入时建议先清理一次：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; grep -rIl $'\r' . | xargs -r sed -i 's/\r$//'"
```

### 6. 安装 SDK 构建依赖

如果容器里还没有 SDK 相关依赖，先执行一次基础工具与库安装。团队里新机器通常先跑这一段：

```powershell
docker exec A1_Builder bash -lc "apt-get update && apt-get install -y build-essential cmake git rsync unzip libgflags-dev libgoogle-glog-dev libusb-1.0-0-dev libuvc-dev"
```

### 7. 编译 SDK 基础库

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; make BR2_EXTERNAL=./smart_software smartsens_m1pro_release_defconfig"
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; make m1_sdk_lib-rebuild"
```

如果你要把 SDK 的完整编译日志保存下来，可以重定向到 `output/`：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh 2>&1 | tee /app/output/a1_sc132gs_build.log"
```

### 8. 生成 EVB 镜像文件

官方构建脚本会输出可烧录的 EVB 镜像。标准路径如下：

```text
/app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk/output/images/zImage.smartsens-m1-evb
```

直接执行整包构建：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh"
```

### 9. 安装 ROS2 运行依赖

当前工作区使用的是 ROS2 Jazzy。为了让 `hardware_driver`、`astra_camera`、`ncnn_ros2` 等包能编译，通过容器补齐这些常用依赖：

```powershell
docker exec A1_Builder bash -lc "apt-get update && apt-get install -y ros-jazzy-camera-info-manager ros-jazzy-cv-bridge ros-jazzy-image-geometry ros-jazzy-image-publisher ros-jazzy-image-transport ros-jazzy-message-filters ros-jazzy-tf2-msgs ros-jazzy-tf2-sensor-msgs ros-jazzy-tf2-ros ros-jazzy-rclcpp-components ros-jazzy-class-loader ros-jazzy-vision-opencv libusb-1.0-0-dev libuvc-dev libgflags-dev libgoogle-glog-dev nlohmann-json3-dev"
```

如果后续编译还提示缺包，再按日志继续补安装即可。

### 10. 确认 ROS 源码替换完成

`src/ros2_ws/src` 里应该是这组上游仓库：

- `base_control_ros2`
- `hardware_driver`
- `bingda_ros2_demos`
- `ncnn_ros2`
- `depend_pkg`
- `object_information_msgs_ros2`

旧的 `a1_robot_stack` 只保留参考，不参与 colcon 编译。

### 11. 编译 ROS2 工作区

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; rm -rf build install log; set +u; source /opt/ros/jazzy/setup.bash; set -u; colcon build --symlink-install"
```

如果要留日志，建议这样跑：

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; rm -rf build install log; set +u; source /opt/ros/jazzy/setup.bash; set -u; colcon build --symlink-install 2>&1 | tee /app/output/ros2_colcon_build.log"
```

### 12. 验证构建结果

编译完成后，先加载工作区环境，再启动对应功能：

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; set +u; source /opt/ros/jazzy/setup.bash; source install/setup.bash; set -u; ros2 launch base_control_ros2 base_control.launch.py"
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; set +u; source /opt/ros/jazzy/setup.bash; source install/setup.bash; set -u; ros2 launch robot_navigation_ros2 robot_lidar.launch.py"
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; set +u; source /opt/ros/jazzy/setup.bash; source install/setup.bash; set -u; ros2 launch robot_vslam_ros2 robot_rgbd_lidar.launch.py"
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; set +u; source /opt/ros/jazzy/setup.bash; source install/setup.bash; set -u; ros2 launch robot_vision_ros2 camera.launch.py"
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; set +u; source /opt/ros/jazzy/setup.bash; source install/setup.bash; set -u; ros2 launch ncnn_ros2 yolov8_ros.launch.py"
```

### 13. ROS 编译体检

仓库里额外准备了一个统一体检脚本，适合在安装完 SDK 和 ROS 依赖后快速确认环境：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/ros_a1_compile_test.sh --with-sdk"
```

报告输出：`output/ros_compile_test_report.txt`

### 14. YOLOv8 训练入口

YOLOv8 的详细中文手册见 [docs/YOLOV8_TRAINING.md](docs/YOLOV8_TRAINING.md)。

## 新手 Git 操作手册（拉取与提交）

### 每天开工前：先拉取最新代码

如果你在主分支：

```powershell
git checkout main
git pull --rebase origin main
```

如果你在功能分支：

```powershell
git checkout 你的分支名
git pull --rebase origin 你的分支名
```

### 开发建议：不要直接在 main 长期改代码

推荐流程：

```powershell
git checkout main
git pull --rebase origin main
git checkout -b feat/你的功能名
```

### 提交代码（一步一步）

1. 查看改动文件。

```powershell
git status
```

1. 查看改动详情。

```powershell
git diff
```

1. 将要提交的文件加入暂存区。

```powershell
git add 文件1 文件2
```

1. 再次检查暂存状态。

```powershell
git status
```

1. 提交。

```powershell
git commit -m "feat: 本次改动说明"
```

1. 推送。

如果在主分支：

```powershell
git push origin main
```

如果在功能分支：

```powershell
git push origin 你的分支名
```

### 提交后建议

- 功能分支上 GitHub 发起 PR
- 在 PR 描述里写明：改了什么、怎么验证、是否影响现有功能

## 常见问题

- 问题：/usr/bin/env: bash\r: No such file or directory
  - 解决：执行 Step 4 的 CRLF 清理命令。

- 问题：No rule to make target m1_sdk_lib-rebuild
  - 解决：先执行 smartsens_m1pro_release_defconfig，再执行 m1_sdk_lib-rebuild。

- 问题：ld 提示 file format not recognized
  - 现象：Windows 挂载目录下，原本软链接的 so 文件被转换成文本文件。
  - 解决：在容器内把文本占位文件恢复为软链接，再重跑 m1_sdk_lib-rebuild 和 a1_sc132gs_build.sh。

- 问题：docker compose 启动后找不到 A1_Builder
  - 解决：先执行 docker compose -f docker/docker-compose.yml ps 检查状态，再执行 up -d。

- 问题：git push 被拒绝（non-fast-forward）
  - 解决：先 git pull --rebase origin main，处理冲突后再 push。
 
注：改 demo C++ 后：
docker run --rm -v $(pwd):/workspace -w /workspace a1_builder bash -lc "bash scripts/build_incremental.sh sdk ssne_ai_demo"
改 ROS2 包后：
docker run --rm -v $(pwd):/workspace -w /workspace a1_builder bash -lc "bash build_incremental.sh ros --clean robot_navigation_ros2 ncnn_ros2"
