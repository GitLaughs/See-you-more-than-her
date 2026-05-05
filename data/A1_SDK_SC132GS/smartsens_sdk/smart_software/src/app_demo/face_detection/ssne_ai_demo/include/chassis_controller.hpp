/**
 * chassis_controller.hpp — STM32 底盘控制接口（WHEELTEC C50X）
 *
 * 使用 A1 UART TX0/RX0（GPIO_PIN_0 / GPIO_PIN_2）与 STM32 通信。
 * 控制帧：11 字节（0x7B 帧头 ... 0x7D 帧尾，BCC 异或校验）
 * 遥测帧：24 字节（STM32 → A1）
 *
 * Chassis control interface over A1 UART TX0/RX0 (GPIO_PIN_0 / GPIO_PIN_2).
 * Control frame: 11 bytes (0x7B ... 0x7D, BCC checksum)
 * Telemetry frame: 24 bytes from STM32 to A1
 */

#pragma once

#include <cstdint>
#include <smartsoc/gpio_api.h>
#include <smartsoc/uart_api.h>

struct ChassisState {
    int16_t  vx   = 0;    // mm/s
    int16_t  vy   = 0;    // mm/s
    int16_t  vz   = 0;    // mrad/s
    float    ax   = 0.f;  // m/s^2
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
     * @brief Send motion command.
     * @param vx X-axis linear velocity in mm/s (AKM: longitudinal).
     * @param vy Y-axis linear velocity in mm/s (AKM: always 0).
     * @param vz Z-axis angular velocity in mrad/s (AKM: front wheel steering angle).
     */
    void SendVelocity(int16_t vx, int16_t vy = 0, int16_t vz = 0);

    /** Read latest telemetry frame; non-blocking, returns whether new data exists. */
    bool ReadTelemetry(ChassisState& state);

    bool is_connected() const { return initialized_; }

private:
    gpio_handle_t gpio_h_ = nullptr;
    uart_handle_t uart_h_ = nullptr;
    bool initialized_ = false;

    static uint8_t bcc(const uint8_t* buf, int len);
    bool parse_telemetry(const uint8_t* buf, int len, ChassisState& s);
};
