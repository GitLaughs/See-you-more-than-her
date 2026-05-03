/**
 * chassis_controller.cpp - STM32 chassis control implementation
 *
 * Control frame (11 bytes, A1 to STM32):
 *   [0]  0x7B          frame header
 *   [1]  Cmd           0x00 = normal control
 *   [2]  0x00          reserved
 *   [3]  Vx_H          X-axis velocity high byte (mm/s, int16)
 *   [4]  Vx_L          X-axis velocity low byte
 *   [5]  Vy_H          Y-axis velocity high byte (AKM = 0)
 *   [6]  Vy_L          Y-axis velocity low byte
 *   [7]  Vz_H          Z-axis angular velocity/front wheel angle high byte
 *   [8]  Vz_L          Z-axis angular velocity/front wheel angle low byte
 *   [9]  BCC           XOR(bytes[0..8])
 *   [10] 0x7D          frame tail
 *
 * Telemetry frame (24 bytes, STM32 to A1):
 *   [0]  0x7B
 *   [1]  0x00
 *   [2..7]  Vx, Vy, Vz (int16 x 3)
 *   [8..13] Ax, Ay, Az (int16 x 3, raw / 1000 = m/s^2)
 *   [14..19] Gx, Gy, Gz (int16 x 3, raw / 1000 = rad/s)
 *   [20..21] Volt (uint16, raw / 100 = V)
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

    gpio_h_ = gpio_init();
    if (!gpio_h_) {
        fprintf(stderr, "[chassis] gpio_init failed\n");
        return false;
    }

    gpio_set_alternate(gpio_h_, GPIO_PIN_0, GPIO_AF_INPUT_NONE,    GPIO_AF_OUTPUT_UART_TX0);
    gpio_set_alternate(gpio_h_, GPIO_PIN_2, GPIO_AF_INPUT_UART_RX0, GPIO_AF_OUTPUT_NONE);

    uart_h_ = uart_init();
    if (!uart_h_) {
        fprintf(stderr, "[chassis] uart_init failed\n");
        gpio_close(gpio_h_);
        gpio_h_ = nullptr;
        return false;
    }

    uart_set_baudrate(uart_h_, UART_TX0, 115200);
    uart_set_baudrate(uart_h_, UART_RX0, 115200);
    uart_set_parity(uart_h_,  UART_TX0, UART_PARITY_NONE);
    uart_set_parity(uart_h_,  UART_RX0, UART_PARITY_NONE);

    initialized_ = true;
    printf("[chassis] UART initialized, baudrate 115200\n");
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
