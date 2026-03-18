# A1 Vision Robot Stack

基于 飞凌微 A1 开发套件 + 思特威指定图像传感器 的智能小车系统。

目标能力：
- YOLOv8 训练与部署（ONNX）
- 手势识别、目标跟踪、视觉检测
- 避障与底盘控制（GPIO/串口）
- 深度感知与环境三维点云生成
- 前端视频输出与结果可视化

## 1. 仓库结构

```text
.
├── docker/                              # 容器构建与编译环境
├── models/                              # 模型文件（ONNX/训练导出）
├── src/
│   ├── a1_ssne_ai_demo/                 # 从官方 ssne_ai_demo 同步的基线代码
│   └── ros2_ws/                         # ROS2 工作区（主开发目录）
│       └── src/a1_robot_stack/
│           ├── include/
│           ├── launch/
│           └── src/
└── data/A1_SDK_SC132GS/smartsens_sdk/   # 原厂 SDK（构建链）
```

## 2. 代码介绍

当前 ROS2 包：`a1_robot_stack`

已实现节点：
- `perception_node`：视觉感知主循环，已预留 YOLO ONNX 推理接入位
- `lidar_ingest_node`：激光雷达数据接入与近障判断
- `safety_supervisor_node`：故障汇聚、心跳监测、急停保护
- `chassis_controller_node`：底盘控制指令输出（预留 UART/CAN/GPIO 对接）
- `performance_monitor_node`：FPS、P95 波动、丢帧率监控

已同步官方 demo 代码：
- 来源：`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo`
- 同步副本：`src/a1_ssne_ai_demo`

## 3. 从克隆到本地部署（Step-by-Step）

### Step 0: 环境准备

主机要求：
- Windows + Docker Desktop
- Git
- 建议安装 GitHub CLI（`gh`）用于协作管理

### Step 1: 克隆仓库

```powershell
git clone https://github.com/GitLaughs/See-you-more-than-her.git
cd See-you-more-than-her

# SDK 固定到验证通过版本（含 libssne 更新）
cd data/A1_SDK_SC132GS
git fetch --all --tags --prune
git checkout 989a51550af0d474191436617eb1eebf94cb4424
cd ../..
```

### Step 2: 启动开发容器

```powershell
cd docker
docker compose up -d
docker compose ps
```

### Step 3: 进入容器并修复换行（首次强烈建议）

SDK 若被 Windows 工具编辑过，可能出现 CRLF 导致 `bash\r` 错误。

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; grep -rIl $'\r' . | xargs -r sed -i 's/\r$//'"
```

### Step 4: 按 SDK 要求编译基础库

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; make BR2_EXTERNAL=./smart_software smartsens_m1pro_release_defconfig"
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; make m1_sdk_lib-rebuild"
```

### Step 5: 执行整包构建脚本

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh"
```

可选：写日志到文件

```powershell
docker exec A1_Builder bash -lc "cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk; bash scripts/a1_sc132gs_build.sh > /app/output/a1_sc132gs_build.log 2>&1"
```

### Step 6: 编译 ROS2 代码

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; source /opt/ros/jazzy/setup.bash; colcon build --symlink-install"
```

### Step 6.1: YOLOv8 训练环境（Python 3.9 + GPU）

```powershell
# 克隆官方训练仓库
git clone https://github.com/ultralytics/ultralytics.git third_party/ultralytics

# 创建并激活 Python 3.9 虚拟环境
py -3.9 -m venv .venv39
.\.venv39\Scripts\Activate.ps1

# 安装 CUDA 版 PyTorch + YOLOv8 + 标注工具
python -m pip install --upgrade pip
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121
pip install ultralytics labelimg
```

训练与数据准备文档见：`docs/YOLOV8_TRAINING.md`

### Step 7: 启动系统

```powershell
docker exec A1_Builder bash -lc "cd /app/src/ros2_ws; source /opt/ros/jazzy/setup.bash; source install/setup.bash; ros2 launch a1_robot_stack bringup.launch.py"
```

### Step 8: 结果验证

验证通道建议：
- 图像输出：屏幕 OSD/视频接口显示检测与跟踪结果
- 控制输出：串口/GPIO 命令日志
- 诊断输出：`/system/fault`、`/system/perf_diag`

## 4. 三人协作建议

建议分工：
- 成员 A（视觉线）：YOLOv8 训练、YOLO 视觉模块、手势识别模块
- 成员 B（控制线）：避障模块、GPIO 调车、跟踪联动小车
- 成员 C（感知线）：深度感知、三维点云、前端可视化

维护规则：
- 所有功能走 issue + PR
- 每周至少一次分支同步与可运行构建验证
- 每个模块交付最小可运行 Demo 和测试说明

## 5. Roadmap

- M1：YOLOv8 训练/导出 ONNX，接入 A1 推理链
- M2：HDR 切换 + 目标跟踪 + 避障联动
- M3：深度感知 + 点云生成 + 屏显融合
- M4：性能与鲁棒性优化（低功耗、稳帧、异常恢复）

## 6. 常见问题

- 问题：`/usr/bin/env: 'bash\r': No such file or directory`
	解决：执行 Step 3 的 CRLF 清理命令。

- 问题：`No rule to make target m1_sdk_lib-rebuild`
	解决：先执行 `make BR2_EXTERNAL=./smart_software smartsens_m1pro_release_defconfig`。

- 问题：`ld: .../libxxx.so: file format not recognized; treating as linker script`
	现象：常见于 Windows 挂载目录，原本应为软链接的 `*.so`/`*.so.1` 被转换成文本文件。
	解决：在容器内把文本占位文件恢复为软链接（按文件内容指向真实版本库），然后重跑：

```bash
cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk/output/opt/m1_sdk/usr/lib
file lib*.so* | grep 'ASCII text'

# 示例：把被文本化的链接修复为真实软链接
rm -f libzlog.so.1 && ln -s libzlog.so.1.2 libzlog.so.1
rm -f libzlog.so   && ln -s libzlog.so.1   libzlog.so

cd /app/smartsens_sdk/A1_SDK_SC132GS/smartsens_sdk
make m1_sdk_lib-rebuild
bash scripts/a1_sc132gs_build.sh
```

	建议：SDK 源码尽量避免在 Windows 侧改写软链接文件；必要时优先在 Linux 容器内执行修复。
