/**
 * @file osd_visualizer.cpp
 * @brief Enhanced OSD visualizer supporting multi-class YOLOv8 detection display
 */

#include "../include/osd_visualizer.hpp"
#include <cstdio>

namespace ssne_demo {

void OsdVisualizer::Initialize(std::array<int, 2>& img_shape) {
  osd_device_.Initialize(img_shape[0], img_shape[1]);
  std::printf("[OSD] Initialized: %dx%d\n", img_shape[0], img_shape[1]);
}

void OsdVisualizer::Release() {
  osd_device_.Release();
}

void OsdVisualizer::DrawFaces(const std::vector<std::array<float, 4>>& boxes) {
  if (boxes.empty()) {
    // Clear face layer
    std::vector<sst::device::osd::OsdQuadRangle> empty;
    osd_device_.Draw(empty, kLayerFace);
    return;
  }

  std::vector<sst::device::osd::OsdQuadRangle> quads;
  quads.reserve(boxes.size());
  for (const auto& box : boxes) {
    sst::device::osd::OsdQuadRangle q;
    q.box = box;
    q.color = kColorFace;
    q.border = 3;
    q.alpha = fdevice::TYPE_ALPHA75;
    q.type = fdevice::TYPE_HOLLOW;
    q.layer_id = kLayerFace;
    quads.push_back(q);
  }
  osd_device_.Draw(quads, kLayerFace);
}

void OsdVisualizer::DrawDetections(const DetectionResult& result) {
  if (result.detections.empty()) {
    std::vector<sst::device::osd::OsdQuadRangle> empty;
    osd_device_.Draw(empty, kLayerYolo);
    return;
  }

  std::vector<sst::device::osd::OsdQuadRangle> quads;
  quads.reserve(result.detections.size());
  for (const auto& det : result.detections) {
    sst::device::osd::OsdQuadRangle q;
    q.box = det.box;
    q.color = ClassIdToColor(det.class_id);
    q.border = 3;
    q.alpha = fdevice::TYPE_ALPHA75;
    q.type = fdevice::TYPE_HOLLOW;
    q.layer_id = kLayerYolo;
    quads.push_back(q);
  }
  osd_device_.Draw(quads, kLayerYolo);

  std::printf("[OSD] Drew %zu detections\n", result.detections.size());
}

void OsdVisualizer::DrawInfoRegion(const std::array<float, 4>& box, int color) {
  std::vector<sst::device::osd::OsdQuadRangle> quads;
  sst::device::osd::OsdQuadRangle q;
  q.box = box;
  q.color = color;
  q.border = 2;
  q.alpha = fdevice::TYPE_ALPHA50;
  q.type = fdevice::TYPE_SOLID;
  q.layer_id = kLayerInfo;
  quads.push_back(q);
  osd_device_.Draw(quads, kLayerInfo);
}

void OsdVisualizer::ClearAll() {
  std::vector<sst::device::osd::OsdQuadRangle> empty;
  osd_device_.Draw(empty, kLayerFace);
  osd_device_.Draw(empty, kLayerYolo);
  osd_device_.Draw(empty, kLayerInfo);
}

}  // namespace ssne_demo
