# A1 Vision Robot Stack

基于 SmartSens A1 开发板的嵌入式机器人软件栈，覆盖板端视觉推理、SDK 镜像打包、STM32 底盘集成和 Windows Aurora 联调工具。

## 仓库由什么组成
- 板端 AI Demo：`data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`
- SDK / 固件打包层：`data/A1_SDK_SC132GS/smartsens_sdk/`
- Windows 工具：`tools/aurora/`、`tools/PC/`、`tools/A1/`
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
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

### 4. 板端运行验证
```bash
ssh root@<A1_IP>
cd /app_demo
./scripts/run.sh
```

### 5. Windows 侧联调工具
```powershell
cd tools/aurora
pip install -r requirements.txt
.\launch.ps1
```

默认地址：
- Aurora 视频 / COM13 终端：`http://127.0.0.1:6201`
- PC 直连 STM32：`http://127.0.0.1:6202`
- A1 中继控制：`http://127.0.0.1:6203`

## 仓库边界

优先修改：
- `scripts/`
- `tools/aurora/`
- `docs/`
- `data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`

谨慎修改：
- `data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk/` 其余部分
- `third_party/ultralytics/`
- `WHEELTEC_C50X_2025.12.26/`
- `output/`

## 构建与部署路径

`scripts/build_complete_evb.sh` 会在内层 SDK 构建根 `data/A1_SDK_SC132GS/smartsens_sdk/smartsens_sdk/` 下重建 `ssne_ai_demo`、重新打包 SDK 镜像，并把最终可烧录产物落到 `output/evb/<timestamp>/zImage.smartsens-m1-evb`（同时更新 `output/evb/latest/zImage.smartsens-m1-evb`），不是单独的 `ssne_ai_demo`。

常用脚本：
- 完整镜像：`scripts/build_complete_evb.sh`
- 定向增量：`scripts/build_incremental.sh`
- 初始化环境：`scripts/bootstrap.sh`

## 运行与联调入口

### 板端
- 板端主应用位于 `ssne_ai_demo/`
- 通过 `cd /app_demo && ./scripts/run.sh` 做基础运行验证
- A1_TEST、Link-Test、UART 底盘控制都在这条路径上

### Windows 工具
- `tools/aurora/`：视频预览、拍照、COM13 终端、A1_TEST 手动测试
- `tools/PC/`：电脑直连 STM32 调试
- `tools/A1/`：COM13 → A1_TEST → STM32 中继控制
- 当前 Aurora 默认会由启动脚本自动尝试拉起 `Aurora.exe` 做相机初始化；如果失败，Companion 仍会继续启动，必要时可使用 `-SkipAurora`

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
- [PC 工具说明](tools/PC/README.md)
- [A1 工具说明](tools/A1/README.md)
- [STM32 集成参考](src/stm32_akm_driver/README.md)

### 协作与后续
- [项目现状与后续方向](docs/12_%E9%A1%B9%E7%9B%AE%E8%A7%84%E5%88%92.md)
- [贡献指南](docs/13_%E8%B4%A1%E7%8C%AE%E6%8C%87%E5%8D%97.md)
- [后续开发建议](docs/14_%E5%90%8E%E7%BB%AD%E5%BC%80%E5%8F%91%E5%BB%BA%E8%AE%AE.md)

### 专题参考
- [AI 模型训练](docs/09_AI%E6%A8%A1%E5%9E%8B%E8%AE%AD%E7%BB%83.md)
- [AI 模型转换与部署](docs/15_AI%E6%A8%A1%E5%9E%8B%E8%BD%AC%E6%8D%A2%E4%B8%8E%E9%83%A8%E7%BD%B2.md)
- [A1 深度感知与点云避障方案](docs/16_A1%E6%B7%B1%E5%BA%A6%E6%84%9F%E7%9F%A5%E4%B8%8E%E7%82%B9%E4%BA%91%E9%81%BF%E9%9A%9C%E6%96%B9%E6%A1%88.md)

## 协作注意事项
- `output/` 是本地构建产物，不随 Git 同步。
- `third_party/ultralytics/`、`WHEELTEC_C50X_2025.12.26/` 属于外部依赖或厂商内容。
- 入口文档描述当前默认路径；专题文档只在确有对应代码时声明“已支持”。

## 最小验证建议
- 改文档：检查链接、脚本名、端口、路径是否一致
- 改 Windows 工具：至少运行 `python -m py_compile tools/aurora/aurora_companion.py tools/aurora/serial_terminal.py tools/aurora/qt_camera_bridge.py tools/PC/pc_tool.py tools/PC/pc_chassis.py tools/A1/a1_tool.py tools/A1/a1_relay.py tools/A1/a1_serial.py`
- 改板端或镜像：优先做 `build_incremental.sh` 或 `build_complete_evb.sh`
