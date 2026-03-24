#pragma once

#include <array>
#include <string>

namespace ssne_demo {

struct ProjectPaths {
  // --- 传感器 ---
  std::array<int, 2> image_shape{720, 1280};
  std::array<int, 2> crop_shape{720, 540};
  int crop_offset_y{370};

  // --- SCRFD 人脸检测 ---
  std::array<int, 2> det_shape{640, 480};
  float confidence_threshold{0.4f};
  std::string face_model_path{"/app_demo/app_assets/models/face_640x480.m1model"};

  // --- STM32 底盘 (A1 GPIO UART0: PIN_0=TX, PIN_2=RX) ---
  uint32_t chassis_baudrate{115200};
};

}  // namespace ssne_demo