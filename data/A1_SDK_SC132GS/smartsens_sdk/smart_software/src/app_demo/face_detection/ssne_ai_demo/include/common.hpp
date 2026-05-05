/**
 * common.hpp — 图像处理器（IMAGEPROCESSOR）
 *
 * 封装 SSNE Online Pipeline，从摄像头 Sensor 采集 Y8 灰度图像。
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include "smartsoc/ssne_api.h"

class IMAGEPROCESSOR {
  public:
    bool Initialize(std::array<int, 2>* in_img_shape);
    void GetImage(ssne_tensor_t* img_sensor);
    void Release();

    std::array<int, 2> img_shape;

  private:
    uint8_t format_online;
};
