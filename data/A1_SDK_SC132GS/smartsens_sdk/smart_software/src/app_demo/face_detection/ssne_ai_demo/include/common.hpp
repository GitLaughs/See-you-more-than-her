/*
 * @Filename: common.hpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
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
