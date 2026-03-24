/**
 * @file chassis_controller.cpp
 * @brief STM32 AKM 底盘 UART 控制器实现
 *
 * 通过 POSIX termios 操作 A1 开发板 GPIO UART0,
 * 向 STM32 发送 10 字节运动控制帧。
 */

#include "../include/chassis_controller.hpp"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>

namespace ssne_demo {

ChassisController::~ChassisController() { Close(); }

bool ChassisController::Open(const std::string& port, int baudrate) {
  if (fd_ >= 0) {
    Close();
  }

  fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    std::cerr << "[Chassis] 无法打开串口 " << port << ": " << strerror(errno)
              << std::endl;
    return false;
  }

  // 配置串口参数: 115200 8N1, 无流控
  struct termios tty;
  memset(&tty, 0, sizeof(tty));

  if (tcgetattr(fd_, &tty) != 0) {
    std::cerr << "[Chassis] tcgetattr 失败: " << strerror(errno) << std::endl;
    Close();
    return false;
  }

  // 选择波特率
  speed_t baud_flag;
  switch (baudrate) {
    case 9600:   baud_flag = B9600;   break;
    case 19200:  baud_flag = B19200;  break;
    case 38400:  baud_flag = B38400;  break;
    case 57600:  baud_flag = B57600;  break;
    case 115200: baud_flag = B115200; break;
    default:
      std::cerr << "[Chassis] 不支持的波特率: " << baudrate << std::endl;
      Close();
      return false;
  }

  cfsetispeed(&tty, baud_flag);
  cfsetospeed(&tty, baud_flag);

  // 8N1, 无流控
  tty.c_cflag &= ~PARENB;   // 无校验
  tty.c_cflag &= ~CSTOPB;   // 1停止位
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;        // 8数据位
  tty.c_cflag &= ~CRTSCTS;  // 无硬件流控
  tty.c_cflag |= CREAD | CLOCAL;  // 启用接收, 忽略控制线

  // 原始模式 (非规范)
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // 无软件流控
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
  tty.c_oflag &= ~OPOST;  // 原始输出

  // 非阻塞读
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;

  if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    std::cerr << "[Chassis] tcsetattr 失败: " << strerror(errno) << std::endl;
    Close();
    return false;
  }

  // 清空缓冲区
  tcflush(fd_, TCIOFLUSH);

  std::cout << "[Chassis] 串口已打开: " << port << " @ " << baudrate
            << " baud" << std::endl;
  return true;
}

void ChassisController::Close() {
  if (fd_ >= 0) {
    // 关闭前发送停止指令
    SendStop();
    ::close(fd_);
    fd_ = -1;
    std::cout << "[Chassis] 串口已关闭" << std::endl;
  }
}

bool ChassisController::SendVelocity(int16_t vx, int16_t vy, int16_t vz,
                                     uint8_t mode) {
  return SendFrame(mode, vx, vy, vz);
}

bool ChassisController::SendStop() {
  return SendFrame(0x00, 0, 0, 0);
}

bool ChassisController::SendFrame(uint8_t mode, int16_t vx, int16_t vy,
                                  int16_t vz) {
  if (fd_ < 0) {
    return false;
  }

  uint8_t frame[10];
  frame[0] = 0x5a;  // 帧头
  frame[1] = mode;
  frame[2] = static_cast<uint8_t>((static_cast<uint16_t>(vx)) >> 8);
  frame[3] = static_cast<uint8_t>(vx & 0xFF);
  frame[4] = static_cast<uint8_t>((static_cast<uint16_t>(vy)) >> 8);
  frame[5] = static_cast<uint8_t>(vy & 0xFF);
  frame[6] = static_cast<uint8_t>((static_cast<uint16_t>(vz)) >> 8);
  frame[7] = static_cast<uint8_t>(vz & 0xFF);

  // BCC 校验: XOR(byte[1] .. byte[7])
  uint8_t checksum = 0;
  for (int i = 1; i <= 7; ++i) {
    checksum ^= frame[i];
  }
  frame[8] = checksum;
  frame[9] = 0x5e;  // 帧尾

  ssize_t written = ::write(fd_, frame, sizeof(frame));
  if (written != sizeof(frame)) {
    std::cerr << "[Chassis] 写入失败: wrote " << written << "/10 bytes"
              << std::endl;
    return false;
  }
  return true;
}

}  // namespace ssne_demo
