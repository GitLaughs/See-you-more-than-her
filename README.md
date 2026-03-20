# A1 Vision Robot Stack

基于飞凌微 A1 开发套件与指定图像传感器的智能小车项目。

## 项目目标

- YOLOv8 训练与部署（ONNX）
- 手势识别、目标跟踪、视觉检测
- 避障与底盘控制（GPIO、串口）
- 深度感知与环境点云生成
- 前端视频输出与可视化

## 仓库结构

```text
.
├── docker/                              # 容器构建与运行配置
├── data/                                # SDK 与训练数据模板
├── models/                              # 模型文件（ONNX、训练导出）
├── src/
│   ├── a1_ssne_ai_demo/                 # 官方 demo 同步副本
│   └── ros2_ws/                         # ROS2 工作区
│       └── src/a1_robot_stack/
└── docs/
```

## Docker 配置核对结果

已检查 Docker 相关配置，结论如下：

- 容器名是 A1_Builder
- 服务名是 dev
- 镜像名是 a1-sdk-builder:latest
- 挂载关系正常：
  - data 挂载到 /app/smartsens_sdk
  - src 挂载到 /app/src
  - models 挂载到 /app/models
  - output 挂载到 /app/output

因此本文中的 docker exec A1_Builder ... 命令与实际配置一致。

## 从零开始部署（新手版）

### Step 0：准备环境

- Windows + Docker Desktop
- Git
- Python 3.9（用于 YOLOv8 训练）

### Step 1：第一次克隆代码

```powershell
git clone https://github.com/GitLaughs/See-you-more-than-her.git
cd See-you-more-than-her
```

### Step 2：固定 SDK 版本（含 libssne 更新）

```powershell
cd data/A1_SDK_SC132GS
git fetch --all --tags --prune
git checkout 989a51550af0d474191436617eb1eebf94cb4424
cd ../..
```

### Step 3：启动开发容器

```powershell
docker compose -f docker/docker-compose.yml up -d
docker compose -f docker/docker-compose.yml ps
```

如果你要停止容器：

```powershell
docker compose -f docker/docker-compose.yml down
```

### Step 4：首次建议执行 CRLF 清理

SDK 文件若被 Windows 工具改写，可能出现 bash\r 报错。

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; grep -rIl $'\r' . | xargs -r sed -i 's/\r$//'"
```

### Step 5：编译基础 SDK 库

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; make BR2_EXTERNAL=./smart_software smartsens_m1pro_release_defconfig"
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; make m1_sdk_lib-rebuild"
```

### Step 6：执行整包构建

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh"
```

可选日志输出：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh > /app/output/a1_sc132gs_build.log 2>&1"
```

### Step 7：安装并构建 RPLidar SDK

```powershell
docker exec A1_Builder /bin/bash -c "mkdir -p /app/src/ros2_ws/src/a1_robot_stack/third_party && cd /app/src/ros2_ws/src/a1_robot_stack/third_party && git clone https://github.com/slamtec/rplidar_sdk.git rplidar_sdk --depth 1"
docker exec A1_Builder /bin/bash -c "cd /app/src/ros2_ws/src/a1_robot_stack/third_party/rplidar_sdk/sdk && make"
```

### Step 8：编译 ROS2

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; source /opt/ros/jazzy/setup.bash; colcon build --symlink-install"
```

### Step 9：启动系统

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; source /opt/ros/jazzy/setup.bash; source install/setup.bash; ros2 launch a1_robot_stack bringup.launch.py"
```

低负载核心启动（推荐先用于板端联调）：

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; source /opt/ros/jazzy/setup.bash; source install/setup.bash; ros2 launch a1_robot_stack bringup_a1_core.launch.py"
```

### Step 10：YOLOv8 训练入口

YOLOv8 的详细中文手册见 docs/YOLOV8_TRAINING.md。

### Step 11：ROS 编译体检（新增）

该脚本位于官方 SDK 脚本目录，便于统一维护：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/ros_a1_compile_test.sh"
```

如需先跑官方整包编译再体检 ROS：

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/ros_a1_compile_test.sh --with-sdk"
```

报告输出：`output/ros_compile_test_report.txt`

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
