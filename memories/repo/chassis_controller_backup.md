# Chassis Controller Backup

## Source boundary
- Main-tree source only
- No `.claude/worktrees/**`
- No `output/**`
- No container-only copies

## Restore destination
Restore into:
`data/A1_SDK_SC132GS/smartsens_sdk/smart_software/src/app_demo/face_detection/ssne_ai_demo/`

## File: include/chassis_controller.hpp
```cpp
/**
 * chassis_controller.hpp — STM32 底盘控制接口 (WHEELTEC C50X)
 *
 * 使用 A1 UART TX0/RX0 (GPIO_PIN_0 / GPIO_PIN_2) 与 STM32 通信.
 * 控制帧: 11 字节 (0x7B ... 0x7D, BCC 校验)
 * 遥测帧: 24 字节 ← STM32
 */

#pragma once

#include <cstdint>
#include <smartsoc/gpio_api.h>
#include <smartsoc/uart_api.h>

struct ChassisState {
    int16_t  vx   = 0;    // mm/s
    int16_t  vy   = 0;    // mm/s
    int16_t  vz   = 0;    // mrad/s
    float    ax   = 0.f;  // m/s²
    float    ay   = 0.f;
    float    az   = 0.f;
    float    gx   = 0.f;  // rad/s
    float    gy   = 0.f;
    float    gz   = 0.f;
    float    volt = 0.f;  // V
    bool     stop_flag = false;
};

class ChassisController {
public:
    ChassisController();
    ~ChassisController();

    bool Init();
    void Release();

    /**
     * @brief 发送运动指令
     * @param vx   X 轴线速度 mm/s  (AKM: 纵向)
     * @param vy   Y 轴线速度 mm/s  (AKM 始终为 0)
     * @param vz   Z 轴角速度 mrad/s (AKM: 前轮转角)
     */
    void SendVelocity(int16_t vx, int16_t vy = 0, int16_t vz = 0);

    /** 读取最新遥测帧（非阻塞，返回是否有新数据） */
    bool ReadTelemetry(ChassisState& state);

    bool is_connected() const { return initialized_; }

private:
    gpio_handle_t gpio_h_ = nullptr;
    uart_handle_t uart_h_ = nullptr;
    bool initialized_ = false;

    static uint8_t bcc(const uint8_t* buf, int len);
    bool parse_telemetry(const uint8_t* buf, int len, ChassisState& s);
};
```

## File: src/chassis_controller.cpp
```cpp
/**
 * chassis_controller.cpp — STM32 底盘控制实现
 *
 * 控制帧 (11 字节, A1 → STM32):
 *   [0]  0x7B          帧头
 *   [1]  Cmd           0x00 = 正常控制
 *   [2]  0x00          保留
 *   [3]  Vx_H          X 轴速度高字节 (mm/s, int16)
 *   [4]  Vx_L          X 轴速度低字节
 *   [5]  Vy_H          Y 轴速度高字节 (AKM = 0)
 *   [6]  Vy_L          Y 轴速度低字节
 *   [7]  Vz_H          Z 轴角速度/前轮转角高字节
 *   [8]  Vz_L          Z 轴角速度/前轮转角低字节
 *   [9]  BCC           XOR(bytes[0..8])
 *   [10] 0x7D          帧尾
 *
 * 遥测帧 (24 字节, STM32 → A1):
 *   [0]  0x7B
 *   [1]  0x00
 *   [2..7]  Vx, Vy, Vz (int16×3)
 *   [8..13] Ax, Ay, Az (int16×3, raw, 需 /1000 转 m/s²)
 *   [14..19] Gx, Gy, Gz (int16×3, raw, 需 /1000 转 rad/s)
 *   [20..21] Volt (uint16, 需 /100 转 V)
 *   [22] BCC
 *   [23] 0x7D
 */

#include "../include/chassis_controller.hpp"

#include <cstdio>
#include <cstring>

ChassisController::ChassisController()  = default;
ChassisController::~ChassisController() { Release(); }

bool ChassisController::Init()
{
    if (initialized_) return true;

    // 初始化 GPIO (PIN0 默认已复用为 UART TX0, PIN2 为 UART RX0)
    gpio_h_ = gpio_init();
    if (!gpio_h_) {
        fprintf(stderr, "[chassis] gpio_init 失败\n");
        return false;
    }

    // 确保 PIN0=UART_TX0, PIN2=UART_RX0
    gpio_set_alternate(gpio_h_, GPIO_PIN_0, GPIO_AF_INPUT_NONE,    GPIO_AF_OUTPUT_UART_TX0);
    gpio_set_alternate(gpio_h_, GPIO_PIN_2, GPIO_AF_INPUT_UART_RX0, GPIO_AF_OUTPUT_NONE);

    // 初始化 UART
    uart_h_ = uart_init();
    if (!uart_h_) {
        fprintf(stderr, "[chassis] uart_init 失败\n");
        gpio_close(gpio_h_);
        gpio_h_ = nullptr;
        return false;
    }

    uart_set_baudrate(uart_h_, UART_TX0, 115200);
    uart_set_baudrate(uart_h_, UART_RX0, 115200);
    uart_set_parity(uart_h_,  UART_TX0, UART_PARITY_NONE);
    uart_set_parity(uart_h_,  UART_RX0, UART_PARITY_NONE);

    initialized_ = true;
    printf("[chassis] UART 初始化完成, 波特率 115200\n");
    return true;
}

void ChassisController::Release()
{
    if (uart_h_) { uart_close(uart_h_);  uart_h_ = nullptr; }
    if (gpio_h_) { gpio_close(gpio_h_); gpio_h_ = nullptr; }
    initialized_ = false;
}

uint8_t ChassisController::bcc(const uint8_t* buf, int len)
{
    uint8_t v = 0;
    for (int i = 0; i < len; ++i) v ^= buf[i];
    return v;
}

void ChassisController::SendVelocity(int16_t vx, int16_t vy, int16_t vz)
{
    if (!initialized_) return;

    uint8_t frame[11] = {};
    frame[0]  = 0x7B;
    frame[1]  = 0x00;
    frame[2]  = 0x00;
    frame[3]  = static_cast<uint8_t>((vx >> 8) & 0xFF);
    frame[4]  = static_cast<uint8_t>( vx       & 0xFF);
    frame[5]  = static_cast<uint8_t>((vy >> 8) & 0xFF);
    frame[6]  = static_cast<uint8_t>( vy       & 0xFF);
    frame[7]  = static_cast<uint8_t>((vz >> 8) & 0xFF);
    frame[8]  = static_cast<uint8_t>( vz       & 0xFF);
    frame[9]  = bcc(frame, 9);
    frame[10] = 0x7D;

    uart_send_data(uart_h_, UART_TX0, frame, sizeof(frame));
}

bool ChassisController::ReadTelemetry(ChassisState& state)
{
    if (!initialized_) return false;

    uint8_t buf[24];
    uint32_t received = 0;
    uart_receive_data(uart_h_, UART_RX0, buf, sizeof(buf), &received);

    if (received < 24) return false;
    return parse_telemetry(buf, static_cast<int>(received), state);
}

bool ChassisController::parse_telemetry(const uint8_t* buf, int len,
                                         ChassisState&  s)
{
    if (len < 24) return false;
    if (buf[0] != 0x7B || buf[23] != 0x7D) return false;

    // BCC 验证 (bytes 0..21)
    if (bcc(buf, 22) != buf[22]) return false;

    auto rd16 = [&](int off) -> int16_t {
        return static_cast<int16_t>((buf[off] << 8) | buf[off+1]);
    };
    auto ru16 = [&](int off) -> uint16_t {
        return static_cast<uint16_t>((buf[off] << 8) | buf[off+1]);
    };

    s.vx   = rd16(2);
    s.vy   = rd16(4);
    s.vz   = rd16(6);
    s.ax   = rd16(8)  / 1000.f;
    s.ay   = rd16(10) / 1000.f;
    s.az   = rd16(12) / 1000.f;
    s.gx   = rd16(14) / 1000.f;
    s.gy   = rd16(16) / 1000.f;
    s.gz   = rd16(18) / 1000.f;
    s.volt = ru16(20) / 100.f;
    s.stop_flag = false;
    return true;
}
```

## Direct references from demo_rps_game.cpp
```cpp
12: #include <thread>
13: #include <unistd.h>
14:
15: #include "include/chassis_controller.hpp"
16: #include "include/utils.hpp"
17:
18: using namespace std;
41: std::mutex g_runtime_mtx;
42: RuntimeState g_runtime;
43: VISUALIZER* g_visualizer = nullptr;
44: ChassisController* g_chassis = nullptr;
45:
46: void print_debug_response(const std::string& command, const std::string& body, bool success = true) {
47:     std::cout << "A1_DEBUG {\"command\":\"" << command << "\",\"success\":"
106:             action = "stop";
107:         }
108:         if (g_chassis != nullptr && snapshot.chassis_ready) {
109:             g_chassis->SendVelocity(vx, 0, 0);
110:             if (action != "stop") {
111:                 usleep(250000);
112:                 g_chassis->SendVelocity(0, 0, 0);
190:     VISUALIZER visualizer;
191:     visualizer.Initialize(img_shape, "shared_colorLUT.sscl");
192:
193:     ChassisController chassis;
194:     const bool chassis_ready = chassis.Init();
195:     g_visualizer = &visualizer;
196:     g_chassis = &chassis;
217:         ChassisState chassis_state;
218:
219:         if (!processor.Ready()) {
220:             chassis.ReadTelemetry(chassis_state);
221:             {
222:                 std::lock_guard<std::mutex> lock(g_runtime_mtx);
223:                 g_runtime = runtime;
239:         int16_t vz = 0;
240:         select_velocity(locked_label, &vx, &vy, &vz);
241:         if (runtime.chassis_ready) {
242:             chassis.SendVelocity(vx, vy, vz);
243:         }
244:
245:         chassis.ReadTelemetry(chassis_state);
272:     }
273:
274:     if (runtime.chassis_ready) {
275:         chassis.SendVelocity(0, 0, 0);
276:         chassis.Release();
277:     }
278:
```

## Build wiring from CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.0.0)

project(ssne_ai_demo)

# 引入路径配置文件
include(cmake_config/Paths.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
# 核心：禁用ABI变更提示
add_compile_options(-Wno-psabi)

# add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/online_ped_osd_thread.cpp)
# add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/online_ped_osd.cpp)
add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/demo_rps_game.cpp)
# add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/test_isp_debug.cpp)

# 添加头文件目录
include_directories(${M1_SDK_INC_DIR})
include_directories(${M1_SDK_INC_DIR}/smartsoc)

# 定义调用目录来源
set(SRC_DIR "src")
set(TARGET $ENV{BASE_DIR}/target)
include_directories(${CMAKE_SOURCE_DIR}/include)
message([<mode>] "message to display" ${TARGET})


set(SSNE_AI_DEMO_SOURCES
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/chassis_controller.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/osd-device.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/pipeline_image.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/rps_classifier.cpp"
    "${CMAKE_SOURCE_DIR}/${SRC_DIR}/utils.cpp"
)
target_sources(ssne_ai_demo PRIVATE ${SSNE_AI_DEMO_SOURCES})

target_link_libraries(${PROJECT_NAME}
                        ${M1_SSNE_LIB}
                        ${M1_CMABUFFER_LIB}
                        ${M1_OSD_LIB}
                        ${M1_GPIO_LIB}
                        ${M1_UART_LIB}
                        ${M1_SSZLOG_LIB}
                        ${M1_ZLOG_LIB}
                        ${M1_EMB_LIB}
)

install(TARGETS ssne_ai_demo DESTINATION bin)
```

## Integration notes from README.md
```md
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
```
