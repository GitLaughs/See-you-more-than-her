#pragma once

#include "osd-device.hpp"
#include <algorithm>

namespace utils {
  // 目标检测模型所需的函数
  /* 合并两段结果 */
  void Merge(FaceDetectionResult* result, size_t low, size_t mid, size_t high);
  /* 归并排序算法 */
  void MergeSort(FaceDetectionResult* result, size_t low, size_t high);
  /* 对检测结果进行排序 */
  void SortDetectionResult(FaceDetectionResult* result);
  /* 非极大值抑制 */
  void NMS(FaceDetectionResult* result, float iou_threshold, int top_k);
} // namespace utils

constexpr int DETECTION_LAYER_ID = 0;

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
