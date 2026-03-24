/**
 * @file demo_face_drive.cpp
 * @brief 人脸检测驱动兼容性测试入口
 *
 * 硬件兼容性测试: 检测到人脸时小车直行, 未检测到时停车。
 * 验证 A1 SSNE NPU → GPIO UART → STM32 AKM 的完整数据通路。
 */
#include "include/face_drive_app.hpp"

int main() {
  ssne_demo::FaceDriveApp app;
  return app.Run();
}
