/**
 * @file chassis_controller.cpp
 * @brief STM32 AKM 底盘 UART 控制器实现 (复用自 a1_ssne_ai_demo)
 *
 * 协议: WHEELTEC C50X 11 字节帧 (0x7B 帧头, 0x7D 帧尾, BCC 校验)
 *
 * 硬件:
 *   A1 GPIO_PIN_0 (UART TX0) ----> STM32 UART3 RX (PB11)
 *   A1 GPIO_PIN_2 (UART RX0) <---- STM32 UART3 TX (PB10)
 *   GND <----> GND
 */

#include "../include/chassis_controller.hpp"

#include <cstdio>
#include <cstring>

namespace ssne_demo {

ChassisController::~ChassisController() { Close(); }

bool ChassisController::Open(uint32_t baudrate) {
  if (uart_ != nullptr) {
    Close();
  }

  gpio_ = gpio_init();
  if (gpio_ == nullptr) {
    std::fprintf(stderr, "[底盘] GPIO 初始化失败\n");
    return false;
  }

  uart_ = uart_init();
  if (uart_ == nullptr) {
    std::fprintf(stderr, "[底盘] UART 初始化失败 (请确认已加载 uart_kmod.ko)\n");
    gpio_close(gpio_);
    gpio_ = nullptr;
    return false;
  }

  uart_set_baudrate(uart_, UART_TX0, baudrate);
  uart_set_baudrate(uart_, UART_RX0, baudrate);
  uart_set_parity(uart_, UART_TX0, UART_PARITY_NONE);
  uart_set_parity(uart_, UART_RX0, UART_PARITY_NONE);

  std::printf("[底盘] UART 已初始化, 波特率: %u\n", baudrate);
  return true;
}

void ChassisController::Close() {
  if (uart_ != nullptr) {
    SendStop();
    uart_close(uart_);
    uart_ = nullptr;
    std::printf("[底盘] UART 已关闭\n");
  }
  if (gpio_ != nullptr) {
    gpio_close(gpio_);
    gpio_ = nullptr;
  }
}

bool ChassisController::SendVelocity(int16_t vx, int16_t vy, int16_t vz,
                                     uint8_t cmd) {
  return SendFrame(cmd, vx, vy, vz);
}

bool ChassisController::SendStop() {
  return SendFrame(0x00, 0, 0, 0);
}

bool ChassisController::ReceiveStatus(int16_t* vx_out, int16_t* voltage_out) {
  if (uart_ == nullptr) return false;

  uint8_t buf[32];
  uint32_t received = 0;
  uart_receive_data(uart_, UART_RX0, buf, 24, &received);

  if (received < 24) return false;
  if (buf[0] != FRAME_HEADER || buf[23] != FRAME_TAIL) return false;
  if (buf[22] != CalcBCC(buf, 22)) return false;

  if (vx_out) {
    *vx_out = static_cast<int16_t>((buf[2] << 8) | buf[3]);
  }
  if (voltage_out) {
    *voltage_out = static_cast<int16_t>((buf[20] << 8) | buf[21]);
  }
  return true;
}

bool ChassisController::SendFrame(uint8_t cmd, int16_t vx, int16_t vy,
                                  int16_t vz) {
  if (uart_ == nullptr) return false;

  uint8_t frame[FRAME_SIZE];
  frame[0] = FRAME_HEADER;
  frame[1] = cmd;
  frame[2] = 0x00;
  frame[3] = static_cast<uint8_t>((static_cast<uint16_t>(vx)) >> 8);
  frame[4] = static_cast<uint8_t>(vx & 0xFF);
  frame[5] = static_cast<uint8_t>((static_cast<uint16_t>(vy)) >> 8);
  frame[6] = static_cast<uint8_t>(vy & 0xFF);
  frame[7] = static_cast<uint8_t>((static_cast<uint16_t>(vz)) >> 8);
  frame[8] = static_cast<uint8_t>(vz & 0xFF);
  frame[9] = CalcBCC(frame, 9);
  frame[10] = FRAME_TAIL;

  int ret = uart_send_data(uart_, UART_TX0, frame, FRAME_SIZE);
  if (ret != UART_SUCCESS) {
    std::fprintf(stderr, "[底盘] UART 发送失败\n");
    return false;
  }
  return true;
}

uint8_t ChassisController::CalcBCC(const uint8_t* data, int len) {
  uint8_t bcc = 0;
  for (int i = 0; i < len; ++i) {
    bcc ^= data[i];
  }
  return bcc;
}

}  // namespace ssne_demo
