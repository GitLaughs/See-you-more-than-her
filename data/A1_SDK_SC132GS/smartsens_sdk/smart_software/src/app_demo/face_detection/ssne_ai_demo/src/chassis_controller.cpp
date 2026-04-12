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
