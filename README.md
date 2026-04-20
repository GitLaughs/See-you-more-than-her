# See-you-more-than-her

SmartSens A1 + WHEELTEC C50X 的视觉机器人工作区。

当前仓库的主线是三条：

- A1 开发板侧的 SSNE 视觉 Demo：`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo`
- Windows 侧的 Aurora 工具链：`tools/aurora`
- 文档与项目规范：`docs`

## 现在能做什么

- A1 侧：SC132GS 全分辨率采集、YOLOv8 人物检测、硬件 OSD 叠框、WHEELTEC 底盘联动
- Windows 侧：纯摄像头预览、本地 YOLOv8 + OSD、A1 板端预览、串口扫描与底盘调试
- 构建侧：Docker 中编译 SDK / Demo / ROS2，生成 `output/evb/latest/zImage.smartsens-m1-evb`
- 烧录侧：使用 `Aurora-2.0.0-ciciec.14\Aurora.exe` 进行 A1 EVB 图形化烧录

## 目录说明

```text
data/                           SmartSens SDK 与训练数据
docs/                           全部使用中的项目文档
models/                         训练/部署模型
scripts/                        构建脚本
tools/aurora/                   Windows 测试与联调工具
src/ros2_ws/                    ROS2 工作区
src/stm32_akm_driver/           STM32 侧说明
Aurora-2.0.0-ciciec.14/         Aurora.exe 烧录工具
```

## 标准工作流

1. 启动 Docker 编译环境。
2. 运行 `scripts/build_complete_evb.sh` 或 `--app-only` 生成产物。
3. 使用 `Aurora-2.0.0-ciciec.14\Aurora.exe` 打开 `output/evb/latest/zImage.smartsens-m1-evb` 并完成烧录。
4. 在板端运行 `/app_demo/scripts/run.sh` 验证 Demo。
5. 在 Windows 侧运行 `tools/aurora/launch.ps1` 做预览、串口与底盘联调。

## 文档索引

- [01 快速上手](docs/01_快速上手.md)
- [02 环境搭建](docs/02_环境搭建.md)
- [03 编译与烧录](docs/03_编译与烧录.md)
- [06 程序概览](docs/06_程序概览.md)
- [07 架构设计](docs/07_架构设计.md)
- [11 常见问题](docs/11_常见问题.md)
- [15 AI 模型转换与部署](docs/15_AI模型转换与部署.md)

## 相关入口

- [Aurora 工具说明](tools/aurora/README.md)
- [A1 Vision Demo 说明](data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/README.md)
