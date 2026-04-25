# SSNE AI Demo

这个目录名沿用历史命名，当前实现支持双模型后端切换和 A1_TEST 串口调试协议。

## 核心特性

- 双推理后端支持：SCRFD（默认）/ YOLOv8，通过 `USE_SCRFD_BACKEND` 切换
- SC132GS 采集 720 × 1280 Y8 全分辨率原始帧（竖屏）
- `RunAiPreprocessPipe` 将输入缩放到 640 × 360 送入 NPU
- A1_TEST 串口调试协议支持，通过 COM13 下发控制和查询状态
- Link-Test 联通性测试模式：周期前进停车，验证 A1↔STM32 通信链路
- 键盘监听线程，支持 `help` / `q` / `move` / `stop` / `link_test` 等命令
- OSD 硬件叠加检测框
- WHEELTEC 底盘通信协议（0x7B 帧）

## 关键文件

```text
demo_face.cpp              主循环 + 键盘监听 + A1_TEST 命令处理
project_paths.hpp          全局配置（分辨率、模型路径、阈值、UART 参数）
include/common.hpp         公共类型定义
include/utils.hpp          工具函数
include/chassis_controller.hpp 底盘协议头文件
src/pipeline_image.cpp     全分辨率采集管线
src/yolov8_gray.cpp        YOLOv8 推理和后处理（可选）
src/scrfd_gray.cpp         SCRFD 推理和后处理（默认）
src/chassis_controller.cpp 底盘协议与串口发送
src/osd-device.cpp         OSD 叠框实现
src/utils.cpp              工具函数实现
```

## 主要参数

| 项目 | 值 |
| --- | --- |
| 传感器输入 | 720 × 1280 Y8 (竖屏) |
| 模型输入 | 640 × 360 Y8 |
| 默认推理后端 | SCRFD |
| 可选后端 | YOLOv8 (head6 + CPU 后处理) |
| 置信度阈值 | 0.4 |
| NMS 阈值 | 0.45 |
| 串口波特率 | 115200 |
| Link-Test 默认 | 开启 |
| Link-Test 前进速度 | 60 mm/s |

## A1_TEST 串口调试协议

可通过 COM13 发送以下命令与板端通信：

| 命令 | 说明 |
|------|------|
| `help` | 显示可用命令列表 |
| `status` | 查询系统状态，返回 JSON |
| `A1_TEST test_echo <msg>` | 回显测试 |
| `A1_TEST debug_status` | 查询调试状态快照 |
| `A1_TEST debug_frame` | 查询当前帧调试状态 |
| `A1_TEST debug_last` | 查询最近调试状态 |
| `A1_TEST link_test on` | 开启联通性测试模式 |
| `A1_TEST link_test off` | 关闭联通性测试模式 |
| `A1_TEST stop` | 发送停车指令 |
| `A1_TEST move <vx> <vy> <vz>` | 发送手动运动指令 |

所有命令返回 JSON 格式响应，包含 success/channel/message 等字段。

## Link-Test 模式说明

当前默认启用，用于验证 A1↔STM32 通信链路：

- 周期：5 秒
- 前进窗口：1 秒（vx=60 mm/s）
- 停车窗口：4 秒
- 与检测结果解耦，独立控制底盘

## 运行方式

### 在容器里编译

```bash
docker exec A1_Builder bash -lc "bash /app/scripts/build_complete_evb.sh --app-only"
```

### 在板端运行

```bash
/app_demo/scripts/run.sh
```

运行后可通过键盘输入命令：
- `help`：查看命令列表
- `q`：退出程序
- `status`：查看状态

## 接线

| A1 | STM32 | 说明 |
| --- | --- | --- |
| GPIO_PIN_0 (UART0 TX) | PB11 (UART3 RX) | A1 下发控制帧 |
| GPIO_PIN_2 (UART0 RX) | PB10 (UART3 TX) | STM32 回传遥测 |
| GND | GND | 必须共地 |

## 调试接口

板端同时暴露 COM13 调试串口：
- 用于发送 A1_TEST 命令
- Aurora Companion 通过此接口控制底盘
- 用于查询状态和调试

## 备注

- 当前 `best_a1_formal_head6.m1model` 实际是 SCRFD 灰度模型
- 要切换为 YOLOv8，请修改 `USE_SCRFD_BACKEND = false` 并提供对应模型
- Aurora 会在显示层将竖屏旋转，预览为横屏画面
