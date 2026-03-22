#pragma once

#include <array>
#include <string>
#include <vector>
#include "smartsoc/ssne_api.h"

namespace ssne_demo {

/// Single detection result from YOLOv8
struct Detection {
  std::array<float, 4> box;  // [x1, y1, x2, y2] in original image coords
  float score;               // confidence score
  int class_id;              // class index
};

/// Container for YOLOv8 detection results
struct DetectionResult {
  std::vector<Detection> detections;

  void Clear() { detections.clear(); }
  void Reserve(int n) { detections.reserve(n); }
};

/// YOLOv8 object detector running on SSNE (SmartSens Neural Engine)
///
/// Supports multi-class detection for person, gesture, obstacle, etc.
/// Model must be pre-converted to .m1model format using the SmartSens toolchain.
///
/// Architecture: YOLOv8n with 80-class or custom-class output head.
/// Input: grayscale image resized to det_shape.
/// Output: decoded bounding boxes with class IDs and scores after NMS.
class Yolov8Detector {
 public:
  /// @param model_path Path to .m1model on device filesystem
  /// @param img_shape  Original sensor image size [W, H]
  /// @param det_shape  Model input size [W, H] (e.g. 640x480 or 640x640)
  /// @param num_classes Number of detection classes
  /// @param conf_threshold Minimum confidence to keep a detection
  /// @param nms_threshold  IoU threshold for NMS
  void Initialize(const std::string& model_path,
                  std::array<int, 2> img_shape,
                  std::array<int, 2> det_shape,
                  int num_classes,
                  float conf_threshold = 0.25f,
                  float nms_threshold = 0.45f);

  /// Run detection on a sensor image tensor
  void Predict(ssne_tensor_t* img_in, DetectionResult* result);

  void Release();

  /// Get the class name by ID (returns empty string if out of range)
  const std::string& ClassName(int id) const;

  /// Set class names (order must match training dataset.yaml)
  void SetClassNames(const std::vector<std::string>& names);

 private:
  void Postprocess(DetectionResult* result);
  void NMS(DetectionResult* result);

  uint16_t model_id_{0};
  ssne_tensor_t input_{};

  // YOLOv8 has 3 detection heads (P3/P4/P5) at strides 8, 16, 32
  // Each head outputs: num_anchors × (4 + num_classes) per grid cell
  static constexpr int kNumHeads = 3;
  static constexpr int kStrides[kNumHeads] = {8, 16, 32};
  // For YOLOv8 anchor-free: reg_max=16, 4 DFL channels per box coord
  static constexpr int kRegMax = 16;

  // Output tensors: 3 detection head outputs
  // Each output shape: [1, num_anchors, 4 + num_classes]
  ssne_tensor_t outputs_[kNumHeads];

  AiPreprocessPipe pipe_offline_;

  std::array<int, 2> img_shape_{};
  std::array<int, 2> det_shape_{};
  int num_classes_{80};
  float conf_threshold_{0.25f};
  float nms_threshold_{0.45f};
  float w_scale_{1.0f};
  float h_scale_{1.0f};

  std::vector<std::string> class_names_;
};

}  // namespace ssne_demo
