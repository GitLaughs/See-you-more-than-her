# A1 Vision Robot Stack

基于 SmartSens A1 开发板的嵌入式机器人软件栈，覆盖板端视觉推理、SDK 镜像打包、ROS2 底盘集成和 Windows Aurora 联调工具。

## 仓库由什么组成
- 板端 AI Demo：`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`
- SDK / 固件打包层：`data/A1_SDK_SC132GS/smartsens_sdk/`
- ROS2 工作区：`src/ros2_ws/`
- Windows Aurora 工具：`tools/aurora/`
- STM32 集成参考：`src/stm32_akm_driver/`

## 快速开始

### 1. 初始化环境
```bash
bash scripts/bootstrap.sh
```

如需先加载 SmartSens 基础镜像：

```bash
bash scripts/bootstrap.sh --load-image /path/to/a1-sdk-builder-latest.tar
```

### 2. 启动构建容器
```bash
docker build -f docker/Dockerfile -t a1-sdk-builder:latest .
docker compose -f docker/docker-compose.yml up -d
```

### 3. 生成 EVB 镜像
```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh"
```

常见变体：

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --skip-ros"
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

### 4. 板端运行验证
```bash
ssh root@<A1_IP>
/app_demo/scripts/run.sh
```

### 5. Windows 侧 Aurora 联调
```powershell
cd tools/aurora
pip install -r requirements.txt
.\launch.ps1
```

默认 Companion 地址：`http://127.0.0.1:5801`

## 仓库边界

优先修改：
- `scripts/`
- `tools/aurora/`
- `src/ros2_ws/`
- `docs/`
- `data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`

谨慎修改：
- `data/A1_SDK_SC132GS/smartsens_sdk/` 其余部分
- `src/ros2_ws/src/aruco_ros-humble-devel/`
- `src/ros2_ws/src/usb_cam-ros2/`
- `src/ros2_ws/src/web_video_server-ros2/`
- `third_party/ultralytics/`
- `WHEELTEC_C50X_2025.12.26/`
- `output/`

## 构建与部署路径

`scripts/build_complete_evb.sh` 会依次重建 Demo、可选构建 ROS2、再重新打包 SDK 镜像，因此最终可烧录产物是 `output/evb/<timestamp>/zImage.smartsens-m1-evb`，不是单独的 `ssne_ai_demo`。

常用脚本：
- 完整镜像：`scripts/build_complete_evb.sh`
- ROS2 工作区：`scripts/build_ros2_ws.sh`
- 定向增量：`scripts/build_incremental.sh`
- 初始化环境：`scripts/bootstrap.sh`

## 运行与联调入口

### 板端
- 板端主应用位于 `ssne_ai_demo/`
- 通过 `/app_demo/scripts/run.sh` 做基础运行验证
- A1_TEST、Link-Test、UART 底盘控制都在这条路径上

### ROS2
- `src/ros2_ws/` 是独立工作区和后续集成路径，不等于默认板端运行栈
- `scripts/build_ros2_ws.sh` 只扫描 `src/ros2_ws/src/` 下的包
- 部分可选包刻意保留 `COLCON_IGNORE`

### Aurora
- `tools/aurora/aurora_companion.py` 是 Windows 侧主入口
- 支持相机预览、A1_TEST 串口调试、直连 STM32 控制、ROS bridge 联调
- 当前接受的相机初始化流程：先打开 `Aurora.exe`，再由 Companion 接管

## 文档索引

### 入门
- [快速上手](docs/01_%E5%BF%AB%E9%80%9F%E4%B8%8A%E6%89%8B.md)
- [环境搭建](docs/02_%E7%8E%AF%E5%A2%83%E6%90%AD%E5%BB%BA.md)
- [编译与烧录](docs/03_%E7%BC%96%E8%AF%91%E4%B8%8E%E7%83%A7%E5%BD%95.md)
- [常见问题](docs/11_%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98.md)

### 模块与架构
- [程序概览](docs/06_%E7%A8%8B%E5%BA%8F%E6%A6%82%E8%A7%88.md)
- [架构设计](docs/07_%E6%9E%B6%E6%9E%84%E8%AE%BE%E8%AE%A1.md)
- [Aurora 工具说明](tools/aurora/README.md)
- [STM32 集成参考](src/stm32_akm_driver/README.md)

### 协作与后续
- [项目现状与后续方向](docs/12_%E9%A1%B9%E7%9B%AE%E8%A7%84%E5%88%92.md)
- [贡献指南](docs/13_%E8%B4%A1%E7%8C%AE%E6%8C%87%E5%8D%97.md)
- [后续开发建议](docs/14_%E5%90%8E%E7%BB%AD%E5%BC%80%E5%8F%91%E5%BB%BA%E8%AE%AE.md)

### 专题参考
- [ROS 底盘集成](docs/08_ROS%E5%BA%95%E7%9B%98%E9%9B%86%E6%88%90.md)
- [AI 模型训练](docs/09_AI%E6%A8%A1%E5%9E%8B%E8%AE%AD%E7%BB%83.md)
- [雷达集成](docs/10_%E9%9B%B7%E8%BE%BE%E9%9B%86%E6%88%90.md)
- [AI 模型转换与部署](docs/15_AI%E6%A8%A1%E5%9E%8B%E8%BD%AC%E6%8D%A2%E4%B8%8E%E9%83%A8%E7%BD%B2.md)
- [A1 深度感知与点云避障方案](docs/16_A1%E6%B7%B1%E5%BA%A6%E6%84%9F%E7%9F%A5%E4%B8%8E%E7%82%B9%E4%BA%91%E9%81%BF%E9%9A%9C%E6%96%B9%E6%A1%88.md)

## 协作注意事项
- `output/` 是本地构建产物，不随 Git 同步。
- `third_party/ultralytics/`、`WHEELTEC_C50X_2025.12.26/` 属于外部依赖或厂商内容。
- 入口文档描述当前默认路径；专题文档只在确有对应代码时声明“已支持”。

## 最小验证建议
- 改文档：检查链接、脚本名、端口、路径是否一致
- 改 Aurora：至少运行 `python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/relay_comm.py tools/aurora/qt_camera_bridge.py tools/aurora/chassis_comm.py tools/aurora/ros_bridge.py`
- 改 ROS2：优先做包级构建
- 改板端或镜像：优先做 `build_incremental.sh` 或 `build_complete_evb.sh`
