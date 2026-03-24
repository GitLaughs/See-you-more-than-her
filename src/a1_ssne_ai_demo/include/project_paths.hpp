#pragma once

#include <array>
#include <string>
#include <vector>

namespace ssne_demo {

struct ProjectPaths {
  // --- Sensor ---
  std::array<int, 2> image_shape{720, 1280};
  std::array<int, 2> crop_shape{720, 540};
  int crop_offset_y{370};

  // --- Face detection (SCRFD) ---
  std::array<int, 2> det_shape{640, 480};
  float confidence_threshold{0.4f};
  std::string face_model_path{"/app_demo/app_assets/models/face_640x480.m1model"};

  // --- YOLOv8 object detection ---
  std::array<int, 2> yolo_det_shape{640, 640};
  float yolo_confidence_threshold{0.25f};
  float yolo_nms_threshold{0.45f};
  int yolo_num_classes{2};
  std::string yolo_model_path{"/app_demo/app_assets/models/yolov8n_640x640.m1model"};
  std::vector<std::string> yolo_class_names{"person", "car"};

  // --- RPLidar ---
  std::string lidar_serial_port{"/dev/ttyUSB0"};
  int lidar_baudrate{115200};

  // --- STM32 AKM 底盘 (GPIO UART0: PIN_0=TX, PIN_2=RX) ---
  std::string chassis_serial_port{"/dev/ttyS0"};
  int chassis_baudrate{115200};

  // --- Debug interface ---
  int debug_tcp_port{9090};
};

}  // namespace ssne_demo