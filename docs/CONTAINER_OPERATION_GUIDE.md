# 容器内操作手册

本文档用于在 Windows + Docker Desktop 环境下，完成 A1 项目的源码替换、Git 提交和固件编译。

## 1. 启动容器

```powershell
docker compose -f docker/docker-compose.yml up -d
docker compose -f docker/docker-compose.yml ps
```

容器名：`A1_Builder`

## 2. 进入 ROS 工作区

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws && pwd && ls"
```

当前 `ros2_ws/src` 里应包含：

- `base_control_ros2`
- `hardware_driver`
- `bingda_ros2_demos`
- `ncnn_ros2`
- `depend_pkg`
- `object_information_msgs_ros2`

## 3. 代码提交到 GitHub

```powershell
git status
git add README.md docs/CONTAINER_OPERATION_GUIDE.md docs/RPLIDAR_SDK_GUIDE.md data/A1_SDK_SC132GS/smartsens_sdk/scripts/ros_a1_compile_test.sh src/ros2_ws/README.md src/ros2_ws/src/base_control_ros2 src/ros2_ws/src/hardware_driver src/ros2_ws/src/bingda_ros2_demos src/ros2_ws/src/ncnn_ros2 src/ros2_ws/src/depend_pkg src/ros2_ws/src/object_information_msgs_ros2 src/a1_ssne_ai_demo
git commit -m "feat: replace ros workspace with upstream sources"
git push origin main
```

如果 `main` 不允许直接推送，请改用你的功能分支再发 PR。

## 4. 容器内编译 ROS2

如果是第一次在这台容器里编译，先补齐常用依赖：

```powershell
docker exec A1_Builder bash -lc "apt-get update && apt-get install -y ros-jazzy-camera-info-manager ros-jazzy-cv-bridge ros-jazzy-image-geometry ros-jazzy-image-publisher ros-jazzy-image-transport ros-jazzy-message-filters ros-jazzy-tf2-msgs ros-jazzy-tf2-sensor-msgs ros-jazzy-tf2-ros ros-jazzy-rclcpp-components ros-jazzy-class-loader ros-jazzy-vision-opencv libusb-1.0-0-dev libuvc-dev libgflags-dev libgoogle-glog-dev nlohmann-json3-dev"
```

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; rm -rf build install log; set +u; source /opt/ros/jazzy/setup.bash; set -u; colcon build --symlink-install"
```

编译后可执行：

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; set +u; source /opt/ros/jazzy/setup.bash; source install/setup.bash; set -u; ros2 launch base_control_ros2 base_control.launch.py"
```

## 5. 生成 EVB 镜像文件

如果 SDK 源码刚同步完成，建议先执行基础库编译：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; make BR2_EXTERNAL=./smart_software smartsens_m1pro_release_defconfig"
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; make m1_sdk_lib-rebuild"
```

`smartsens_sdk` 的官方编译脚本会输出 EVB 固件：

输出文件：`data/A1_SDK_SC132GS/smartsens_sdk/output/images/zImage.smartsens-m1-evb`

在容器中执行：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh"
```

若需要先执行 SDK 体检脚本：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/ros_a1_compile_test.sh --with-sdk"
```

## 6. 常见问题

- 如果 `source` 报 `AMENT_TRACE_SETUP_FILES: unbound variable`，请保持脚本里的 `set +u` / `set -u` 包裹。
- 如果生成的 `zImage.smartsens-m1-evb` 不在 `output/images/`，优先查看 `scripts/a1_sc132gs_build.sh` 的执行日志。
