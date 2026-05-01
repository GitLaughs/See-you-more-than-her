# SSNE AI 演示项目

## 项目概述

本项目当前为基于 SmartSens SSNE 的 demo-rps 显示基线版本。
运行时保留完整视频/OSD 管线与背景位图显示，并将 `P / R / S` 分类结果映射到底盘前进 / 停止 / 后退控制。

## 文件结构

```text
ssne_ai_demo/
├── demo_rps_game.cpp          # 主程序：RPS分类 + 底盘控制
├── include/
│   ├── chassis_controller.hpp # 底盘控制接口
│   ├── common.hpp             # IMAGEPROCESSOR / RPS_CLASSIFIER 等声明
│   ├── log.hpp                # 日志宏
│   ├── osd-device.hpp         # OSD设备接口
│   └── utils.hpp              # VISUALIZER 等工具声明
├── src/
│   ├── chassis_controller.cpp # GPIO/UART 底盘控制实现
│   ├── osd-device.cpp         # OSD图层与位图绘制
│   ├── pipeline_image.cpp     # 1920x1080 在线视频管线
│   ├── rps_classifier.cpp     # 手势分类模型封装
│   └── utils.cpp              # 可视化与工具实现
├── app_assets/
│   ├── background.ssbmp       # 背景位图
│   ├── 1.ssbmp                # 叠加位图资源
│   ├── shared_colorLUT.sscl   # 位图颜色LUT
│   └── models/
│       └── model_rps.m1model  # RPS分类模型
├── cmake_config/
│   └── Paths.cmake
├── scripts/
│   └── run.sh                 # 运行脚本
└── CMakeLists.txt             # 构建配置
```

## 当前运行流程

1. `demo_rps_game.cpp` 初始化 SSNE、`IMAGEPROCESSOR`、`RPS_CLASSIFIER`、`VISUALIZER`、`ChassisController`
2. `pipeline_image.cpp` 输出 `1920x1080` 的 `SSNE_YUV422_16` 在线图像
3. `demo_rps_game.cpp` 在图层 2 绘制 `background.ssbmp`
4. `RPS_CLASSIFIER` 对每帧做手势分类
5. 连续 3 帧稳定后锁定标签
6. 标签映射关系：
   - `P` -> `vx = 100`
   - `R` -> `vx = 0`
   - `S` -> `vx = -100`
7. 若底盘初始化成功，则通过 `ChassisController` 发送速度命令
8. 程序每约 2 秒输出一次状态日志

## 关键约束

- 当前入口文件为 `demo_rps_game.cpp`，不再使用 `demo_face.cpp`
- 当前模型为 `app_assets/models/model_rps.m1model`
- 当前运行脚本直接启动 `./ssne_ai_demo`，不再传入 `app_config.json`
- OSD 使用 5 个图层，其中背景位图使用图层 2

## 运行方式

在板端 `app_demo` 目录执行：

```bash
./scripts/run.sh
```
