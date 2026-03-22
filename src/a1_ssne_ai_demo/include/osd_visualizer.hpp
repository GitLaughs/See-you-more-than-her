#pragma once

#include "osd-device.hpp"
#include "yolov8_detector.hpp"
#include <vector>
#include <string>

namespace ssne_demo {

/// Color palette index for each class (indexes into the OSD LUT)
/// Layer 0: face detection (green), Layer 1: person (blue), Layer 2: gesture (yellow),
/// Layer 3: obstacle (red), Layer 4: reserved for info overlay
static constexpr int kColorFace     = 0;  // LUT index for face (green)
static constexpr int kColorPerson   = 1;  // LUT index for person (blue)
static constexpr int kColorGesture  = 2;  // LUT index for gesture (yellow)
static constexpr int kColorObstacle = 3;  // LUT index for obstacle (red)
static constexpr int kColorDefault  = 1;  // default color

/// OSD layer assignment
static constexpr int kLayerFace     = 0;
static constexpr int kLayerYolo     = 1;
static constexpr int kLayerInfo     = 2;

/// Map class_id to an OSD color index
inline int ClassIdToColor(int class_id) {
  switch (class_id) {
    case 0: return kColorPerson;    // person
    case 1: return kColorObstacle;  // car / obstacle
    default: return kColorDefault;
  }
}

/// Map class_id to an OSD layer for per-class drawing
inline int ClassIdToLayer(int class_id) {
  // All yolov8 detections go to layer 1 to avoid conflict with face layer 0
  (void)class_id;
  return kLayerYolo;
}

/// Enhanced visualizer supporting both face detection and YOLOv8 multi-class results.
///
/// Uses OSD hardware layers:
///   - Layer 0: Face detection boxes (SCRFD)
///   - Layer 1: YOLOv8 object detection boxes (per-class colored)
///   - Layer 2: Info overlay / status text region
class OsdVisualizer {
 public:
  void Initialize(std::array<int, 2>& img_shape);
  void Release();

  /// Draw face detection boxes on Layer 0
  void DrawFaces(const std::vector<std::array<float, 4>>& boxes);

  /// Draw YOLOv8 detection results on Layer 1 (colored per class)
  void DrawDetections(const DetectionResult& result);

  /// Draw an info overlay region on Layer 2 (e.g., lidar distance warning)
  /// `box` defines the OSD region, `color` selects the LUT entry
  void DrawInfoRegion(const std::array<float, 4>& box, int color);

  /// Clear all OSD layers
  void ClearAll();

 private:
  sst::device::osd::OsdDevice osd_device_;
};

}  // namespace ssne_demo
