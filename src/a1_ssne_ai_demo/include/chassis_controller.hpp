#pragma once

#include <cstdint>
#include <string>

namespace ssne_demo {

/// 底盘控制器 — 通过 GPIO UART 向 STM32 AKM 发送运动指令
///
/// 通信协议 (10 字节):
///   [0x5a][Mode][Vx_H][Vx_L][Vy_H][Vy_L][Vz_H][Vz_L][BCC][0x5e]
///
/// BCC = XOR(byte[1] .. byte[7])
///
/// 硬件连接:
///   A1 GPIO_PIN_0 (TX) → STM32 UART3 RX
///   A1 GPIO_PIN_2 (RX) → STM32 UART3 TX
///   波特率: 115200, 8N1
class ChassisController {
 public:
  ChassisController() = default;
  ~ChassisController();

  // 禁止拷贝
  ChassisController(const ChassisController&) = delete;
  ChassisController& operator=(const ChassisController&) = delete;

  /// 打开 UART 串口
  /// @param port 设备路径, 如 "/dev/ttyS0"
  /// @param baudrate 波特率, 默认 115200
  /// @return 成功返回 true
  bool Open(const std::string& port, int baudrate);

  /// 关闭串口
  void Close();

  /// 发送运动速度指令
  /// @param vx 前进速度 (mm/s), 正值前进, 负值后退
  /// @param vy 平移速度 (mm/s), AKM差速车固定为 0
  /// @param vz 转向角/角速度 (AKM=前轮转角), 直行为 0
  /// @param mode 运动模式: 0x00=正常, 0x01=充电, 0x02=导航充电
  /// @return 发送成功返回 true
  bool SendVelocity(int16_t vx, int16_t vy, int16_t vz, uint8_t mode = 0x00);

  /// 发送停止指令 (Vx=Vy=Vz=0)
  bool SendStop();

  bool IsOpen() const { return fd_ >= 0; }

 private:
  /// 构建并发送完整的 10 字节帧
  bool SendFrame(uint8_t mode, int16_t vx, int16_t vy, int16_t vz);

  int fd_ = -1;
};

}  // namespace ssne_demo
