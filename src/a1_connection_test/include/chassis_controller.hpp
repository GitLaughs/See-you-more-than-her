#pragma once

#include <cstdint>

extern "C" {
#include "gpio_api.h"
#include "uart_api.h"
}

namespace ssne_demo {

/// 底盘控制器 — 通过 A1 GPIO UART0 向 STM32 AKM 发送运动指令
///
/// WHEELTEC C50X 协议 (11 字节):
///   [0x7B][Cmd][0x00][Vx_H][Vx_L][Vy_H][Vy_L][Vz_H][Vz_L][BCC][0x7D]
///
/// 硬件连接:
///   A1 GPIO_PIN_0 (UART TX0) → STM32 UART3 RX (PB11)
///   A1 GPIO_PIN_2 (UART RX0) → STM32 UART3 TX (PB10)
///   波特率: 115200, 8N1, 无校验
class ChassisController {
 public:
  ChassisController() = default;
  ~ChassisController();

  ChassisController(const ChassisController&) = delete;
  ChassisController& operator=(const ChassisController&) = delete;

  bool Open(uint32_t baudrate = 115200);
  void Close();
  bool SendVelocity(int16_t vx, int16_t vy, int16_t vz, uint8_t cmd = 0x00);
  bool SendStop();
  bool ReceiveStatus(int16_t* vx_out, int16_t* voltage_out);
  bool IsOpen() const { return uart_ != nullptr; }

 private:
  static constexpr uint8_t FRAME_HEADER = 0x7B;
  static constexpr uint8_t FRAME_TAIL   = 0x7D;
  static constexpr int FRAME_SIZE       = 11;

  bool SendFrame(uint8_t cmd, int16_t vx, int16_t vy, int16_t vz);
  static uint8_t CalcBCC(const uint8_t* data, int len);

  gpio_handle_t gpio_ = nullptr;
  uart_handle_t uart_ = nullptr;
};

}  // namespace ssne_demo
