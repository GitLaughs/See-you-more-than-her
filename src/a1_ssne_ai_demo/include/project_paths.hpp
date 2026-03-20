#pragma once

#include <array>
#include <string>

namespace ssne_demo {

struct ProjectPaths {
  std::array<int, 2> image_shape{720, 1280};
  std::array<int, 2> crop_shape{720, 540};
  std::array<int, 2> det_shape{640, 480};
  int crop_offset_y{370};
  float confidence_threshold{0.4f};
  std::string face_model_path{"/app_demo/app_assets/models/face_640x480.m1model"};
};

}  // namespace ssne_demo