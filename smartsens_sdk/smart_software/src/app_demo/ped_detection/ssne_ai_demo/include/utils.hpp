/*
 * @Filename: utils.hpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#pragma once

#include "osd-device.hpp"
#include <algorithm>

namespace utils {
  // 人脸检测模型所需的函数
  /* 合并两段结果 */
  void Merge(FaceDetectionResult* result, size_t low, size_t mid, size_t high);
  /* 归并排序算法 */
  void MergeSort(FaceDetectionResult* result, size_t low, size_t high);
  /* 对检测结果进行排序 */
  void SortDetectionResult(FaceDetectionResult* result);
  /* 非极大值抑制 */
  void NMS(FaceDetectionResult* result, float iou_threshold, int top_k);
} // namespace utils

class VISUALIZER {
  public:
    void Initialize(std::array<int, 2>& in_img_shape);
    void Release();
    void Draw();
    void Draw(const std::vector<std::array<float, 4>>& boxes);

  private:
    // OSD设备实例
    sst::device::osd::OsdDevice osd_device;
};
