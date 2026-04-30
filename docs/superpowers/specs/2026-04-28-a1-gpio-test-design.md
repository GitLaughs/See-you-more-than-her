# A1 GPIO 测试入口设计

- 日期：2026-04-28
- 范围：`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`
- 目标：在现有 `app_demo` 基础上增加临时 GPIO 测试入口，用于验证 A1 开发板 GPIO 对 Windows 侧 DAQ / 逻辑分析仪的发送与接收能力；验证完成后可整体删除。

## 1. 目标

本次改动解决两个验证问题：

1. A1 是否能稳定把指定 GPIO 输出到外部观测设备。
2. A1 是否能稳定读取来自 Windows 侧 DAQ / 逻辑分析仪的输入电平变化。

成功标准：

- 固件整包可重编并正常打包。
- `app_demo` 仅在显式传入 `--gpio-test` 时进入 GPIO 测试模式；正常检测/OSD/底盘逻辑保持不变。
- 测试模式支持按参数选择 `GPIO_PIN_0/2/8/9/10`。
- `tx` 模式下，Windows 侧能观测到 A1 周期性翻转输出。
- `rx` 模式下，A1 日志能报告 Windows 侧驱动的输入电平变化。
- `GPIO_PIN_0/2` 在测试模式中可从 UART 复用切回普通 GPIO，并完成基础收发验证。

## 2. 现状

### 2.1 GPIO 能力与约束

现有仓库文档已经给出 GPIO API 和引脚约束：

- GPIO API 通过 `gpio_api.h` 与 `libgpio.so` 提供。
- 可用引脚范围以 `GPIO_PIN_0/2/8/9/10` 为主。
- GPIO 输出电平为 1.8V；本次外部电平适配已由硬件接线侧处理。
- `GPIO_PIN_0` 和 `GPIO_PIN_2` 默认复用到 UART，需要显式切回普通 GPIO 才能做测试。

### 2.2 `app_demo` 现状

当前 `ssne_ai_demo` 已经：

- 通过 `CMakeLists.txt` 链接 `${M1_GPIO_LIB}`，无需新增 GPIO 依赖类型。
- 在 `demo_face.cpp` 中维护独立命令入口、运行态和主循环。
- 在 `src/chassis_controller.cpp` 中把 `GPIO_PIN_0/2` 配置为 UART TX/RX，用于底盘链路。

因此，GPIO 测试入口不应混入底盘 UART 主流程，而应在 `main()` 早期分流，避免测试模式与正常 UART 初始化互相干扰。

## 3. 设计原则

1. 只做临时验证所需最小改动，不扩展成通用诊断框架。
2. GPIO 测试模式与正常 demo 模式严格分流。
3. 测试代码集中在独立实现文件，后续删除成本低。
4. 对 `GPIO_PIN_0/2` 的复用切换只在测试模式发生，不回流到正常底盘路径。
5. 首版不绑定某个特定 Windows SDK；Windows 侧只承担观测和驱动职责。

## 4. 方案设计

### 4.1 入口设计

在 `demo_face.cpp` 的 `main()` 入口增加命令行解析：

- 无 `--gpio-test`：保持现有人脸检测 demo 流程。
- 有 `--gpio-test`：进入 GPIO 专用测试流程，并在该流程结束后直接退出程序。

建议命令格式：

```bash
./ssne_ai_demo --gpio-test --pin 8 --mode tx --period-ms 500 --duration-s 10
./ssne_ai_demo --gpio-test --pin 8 --mode rx --duration-s 10
./ssne_ai_demo --gpio-test --pin 8 --mode loop --period-ms 500 --duration-s 10
```

参数约束：

- `--pin`：必填，仅允许 `0/2/8/9/10`
- `--mode`：必填，允许 `tx` / `rx` / `loop`
- `--period-ms`：`tx` / `loop` 使用，默认可设 500ms
- `--duration-s`：可选，限制测试总时长，避免误跑死循环

### 4.2 代码结构

新增独立测试实现，建议拆为：

- `include/gpio_test_runner.hpp`
- `src/gpio_test_runner.cpp`

职责边界：

- 参数结构体定义与校验
- pin 号到 `GPIO_PIN_x` 掩码映射
- GPIO 初始化、模式切换、复用切换、资源释放
- `tx` / `rx` / `loop` 三种测试行为
- 统一日志输出

`demo_face.cpp` 只保留：

- 是否进入 GPIO 测试模式的判断
- 参数收集与错误提示
- 调用 `RunGpioTest(...)`

### 4.3 引脚与复用处理

所有测试模式都先执行：

1. `gpio_init()` 获取句柄
2. pin 参数映射到目标掩码
3. 若 pin 为 `0` 或 `2`，调用 `gpio_set_alternate(..., GPIO_AF_INPUT_NONE, GPIO_AF_OUTPUT_NONE)` 切回普通 GPIO
4. 根据模式调用 `gpio_set_mode(..., GPIO_MODE_OUTPUT)` 或 `GPIO_MODE_INPUT`
5. 结束时 `gpio_close()`

注意：

- `GPIO_PIN_0/2` 的复用切换只在测试程序内部生效；测试结束后进程直接退出，不再继续初始化底盘 UART。
- 若某 pin 在实际板级运行中返回失败，日志必须明确提示 pin、模式和失败点。

### 4.4 模式行为

#### `tx`

用途：验证 A1 输出能力。

行为：

- 设置目标 pin 为输出
- 按 `period-ms` 周期交替输出高/低电平
- 每次翻转打印时间戳、pin、level
- 运行到 `duration-s` 到期后退出

Windows 侧预期：DAQ / 逻分仪看到稳定翻转边沿和周期。

#### `rx`

用途：验证 A1 输入能力。

行为：

- 设置目标 pin 为输入
- 轮询 `gpio_read_pin()`
- 仅在电平变化时打印时间戳、pin、old/new level
- 结束时打印变化次数汇总

Windows 侧预期：DAQ 驱动稳定高/低电平或方波，A1 日志出现对应变化。

#### `loop`

用途：快速检查程序和日志链路是否正常。

行为：

- 以固定周期触发输出翻转
- 同时打印本次写入结果
- 如硬件允许同线回读，则补充读取日志；否则保留为“输出+日志”模式

说明：`loop` 只用于快速联调，不能替代 Windows 侧真实观测结果。

### 4.5 日志与退出

日志统一使用单行文本，至少包含：

- 时间戳
- 模式
- pin
- 操作类型（set/read/change/summary/error）
- 电平值或错误信息

退出条件：

- 达到 `duration-s`
- 参数非法
- `gpio_init()` 或模式设置失败
- 用户手动中断进程

### 4.6 构建与运行脚本

构建侧：

- `CMakeLists.txt` 已有 `${M1_GPIO_LIB}`，只需把 `src/gpio_test_runner.cpp` 加入源文件列表。

运行侧：

- 检查并补充 `scripts/run.sh`，确保运行前加载 `gpio_kmod.ko`。
- 若当前脚本未加载 GPIO 内核模块，则按文档补上：

```bash
insmod /lib/modules/$(uname -r)/extra/gpio_kmod.ko
```

## 5. 验证方案

### 5.1 编译验证

- 运行完整 EVB 打包流程，确认 `ssne_ai_demo` 和最终镜像均构建成功。
- 启动正常 demo，确认非 `--gpio-test` 路径无回归。

### 5.2 板级验证

最小验证矩阵：

1. `GPIO_PIN_8` `tx`
2. `GPIO_PIN_8` `rx`
3. `GPIO_PIN_0` `tx`
4. `GPIO_PIN_2` `rx`

通过标准：

- `tx`：Windows 侧看到与日志一致的边沿节拍。
- `rx`：A1 侧日志看到与 Windows 驱动一致的电平变化。
- `GPIO_PIN_0/2`：在测试模式下完成复用切换后也能工作。

### 5.3 风险点

- `GPIO_PIN_0/2` 与底盘 UART 共用物理资源，若测试模式未在 `main()` 早期分流，可能和 `ChassisController::Init()` 冲突。
- 文档允许的 pin 范围不代表每块板在当前固件配置下都完全可用；需要日志精确定位失败点。
- 轮询读取频率过高可能刷屏，应只在电平变化时打印。

## 6. 删除策略

此功能为临时验证入口，后续删除应只涉及：

- `demo_face.cpp` 中的 `--gpio-test` 分支
- `gpio_test_runner` 头/源文件
- 如有新增脚本参数说明，同步删除对应文档

保留现有 `libgpio.so` 链接不构成风险，因为项目本身已在使用 GPIO 相关依赖。
