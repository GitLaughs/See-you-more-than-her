/**
 * @file yolov8_detector.cpp
 * @brief YOLOv8 object detector implementation on SSNE (SmartSens Neural Engine)
 *
 * This implementation follows the YOLOv8 anchor-free detection head design:
 * - 3 detection heads at strides 8, 16, 32
 * - Each head outputs 4 bbox regression values + num_classes confidence scores
 * - Distribution Focal Loss (DFL) for box regression (reg_max=16)
 * - NMS for post-processing
 *
 * The model must be converted from ONNX to .m1model using the SmartSens
 * model conversion toolchain before deployment.
 */

#include "../include/yolov8_detector.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <numeric>

namespace ssne_demo {

// Static member definition
constexpr int Yolov8Detector::kStrides[kNumHeads];

/// Softmax over a contiguous float array of length `len`, write result in-place
static void Softmax(float* data, int len) {
  float max_val = *std::max_element(data, data + len);
  float sum = 0.0f;
  for (int i = 0; i < len; ++i) {
    data[i] = std::exp(data[i] - max_val);
    sum += data[i];
  }
  for (int i = 0; i < len; ++i) {
    data[i] /= sum;
  }
}

/// DFL (Distribution Focal Loss) decode: softmax over reg_max bins then weighted sum
static float DFLDecode(float* data, int reg_max) {
  Softmax(data, reg_max);
  float val = 0.0f;
  for (int i = 0; i < reg_max; ++i) {
    val += static_cast<float>(i) * data[i];
  }
  return val;
}

void Yolov8Detector::Initialize(const std::string& model_path,
                                std::array<int, 2> img_shape,
                                std::array<int, 2> det_shape,
                                int num_classes,
                                float conf_threshold,
                                float nms_threshold) {
  img_shape_ = img_shape;
  det_shape_ = det_shape;
  num_classes_ = num_classes;
  conf_threshold_ = conf_threshold;
  nms_threshold_ = nms_threshold;

  w_scale_ = static_cast<float>(img_shape_[0]) / static_cast<float>(det_shape_[0]);
  h_scale_ = static_cast<float>(img_shape_[1]) / static_cast<float>(det_shape_[1]);

  // Load model on NPU
  char* path = const_cast<char*>(model_path.c_str());
  model_id_ = ssne_loadmodel(path, SSNE_STATIC_ALLOC);

  // Create input tensor
  uint32_t w = static_cast<uint32_t>(det_shape_[0]);
  uint32_t h = static_cast<uint32_t>(det_shape_[1]);
  input_ = create_tensor(w, h, SSNE_Y_8, SSNE_BUF_AI);

  // Default class names if not set
  if (class_names_.empty()) {
    class_names_ = {"person", "car"};
  }

  std::printf("[YOLOv8] Initialized: model=%s, input=%dx%d, classes=%d\n",
              model_path.c_str(), det_shape_[0], det_shape_[1], num_classes_);
}

void Yolov8Detector::Predict(ssne_tensor_t* img_in, DetectionResult* result) {
  result->Clear();

  // Preprocess: resize + normalize via SSNE hardware pipeline
  int ret = RunAiPreprocessPipe(pipe_offline_, *img_in, input_);
  if (ret != 0) {
    std::fprintf(stderr, "[YOLOv8] Preprocess failed (ret=%d)\n", ret);
    return;
  }

  // NPU forward inference
  if (ssne_inference(model_id_, 1, &input_)) {
    std::fprintf(stderr, "[YOLOv8] Inference failed!\n");
    return;
  }

  // Get output tensors
  ssne_getoutput(model_id_, kNumHeads, outputs_);

  // Decode all detection heads
  Postprocess(result);
}

void Yolov8Detector::Postprocess(DetectionResult* result) {
  // YOLOv8 anchor-free head output format per grid cell:
  //   [4 * reg_max (DFL box regression), num_classes (class scores)]
  // Total per cell = 4 * reg_max + num_classes
  const int output_per_cell = 4 * kRegMax + num_classes_;

  for (int head = 0; head < kNumHeads; ++head) {
    float* out_data = reinterpret_cast<float*>(get_data(outputs_[head]));
    if (out_data == nullptr) continue;

    int stride = kStrides[head];
    int grid_w = det_shape_[0] / stride;
    int grid_h = det_shape_[1] / stride;
    int num_anchors = grid_w * grid_h;

    for (int idx = 0; idx < num_anchors; ++idx) {
      float* cell = out_data + idx * output_per_cell;

      // Find best class score
      float* cls_scores = cell + 4 * kRegMax;
      int best_cls = 0;
      float best_score = cls_scores[0];
      for (int c = 1; c < num_classes_; ++c) {
        if (cls_scores[c] > best_score) {
          best_score = cls_scores[c];
          best_cls = c;
        }
      }

      // Apply sigmoid to get probability
      best_score = 1.0f / (1.0f + std::exp(-best_score));

      if (best_score < conf_threshold_) continue;

      // Grid position
      int gx = idx % grid_w;
      int gy = idx / grid_w;

      // DFL decode for ltrb (left, top, right, bottom) distances
      float dfl_buf[kRegMax];
      float ltrb[4];
      for (int d = 0; d < 4; ++d) {
        std::copy(cell + d * kRegMax, cell + (d + 1) * kRegMax, dfl_buf);
        ltrb[d] = DFLDecode(dfl_buf, kRegMax);
      }

      // Convert ltrb to xyxy in detection image coords
      float cx = (static_cast<float>(gx) + 0.5f) * stride;
      float cy = (static_cast<float>(gy) + 0.5f) * stride;

      float x1 = cx - ltrb[0] * stride;
      float y1 = cy - ltrb[1] * stride;
      float x2 = cx + ltrb[2] * stride;
      float y2 = cy + ltrb[3] * stride;

      // Clamp to detection image bounds
      x1 = std::max(0.0f, std::min(x1, static_cast<float>(det_shape_[0])));
      y1 = std::max(0.0f, std::min(y1, static_cast<float>(det_shape_[1])));
      x2 = std::max(0.0f, std::min(x2, static_cast<float>(det_shape_[0])));
      y2 = std::max(0.0f, std::min(y2, static_cast<float>(det_shape_[1])));

      // Scale to original image coords
      Detection det;
      det.box = {x1 * w_scale_, y1 * h_scale_, x2 * w_scale_, y2 * h_scale_};
      det.score = best_score;
      det.class_id = best_cls;
      result->detections.push_back(det);
    }
  }

  // Run NMS per-class
  NMS(result);
}

void Yolov8Detector::NMS(DetectionResult* result) {
  if (result->detections.empty()) return;

  // Sort by score descending
  std::sort(result->detections.begin(), result->detections.end(),
            [](const Detection& a, const Detection& b) {
              return a.score > b.score;
            });

  std::vector<bool> suppressed(result->detections.size(), false);

  for (size_t i = 0; i < result->detections.size(); ++i) {
    if (suppressed[i]) continue;
    for (size_t j = i + 1; j < result->detections.size(); ++j) {
      if (suppressed[j]) continue;
      // Only suppress within same class
      if (result->detections[i].class_id != result->detections[j].class_id)
        continue;

      const auto& a = result->detections[i].box;
      const auto& b = result->detections[j].box;

      float inter_x1 = std::max(a[0], b[0]);
      float inter_y1 = std::max(a[1], b[1]);
      float inter_x2 = std::min(a[2], b[2]);
      float inter_y2 = std::min(a[3], b[3]);
      float inter_w = std::max(0.0f, inter_x2 - inter_x1);
      float inter_h = std::max(0.0f, inter_y2 - inter_y1);
      float inter_area = inter_w * inter_h;

      float area_a = (a[2] - a[0]) * (a[3] - a[1]);
      float area_b = (b[2] - b[0]) * (b[3] - b[1]);
      float iou = inter_area / (area_a + area_b - inter_area + 1e-6f);

      if (iou > nms_threshold_) {
        suppressed[j] = true;
      }
    }
  }

  DetectionResult filtered;
  for (size_t i = 0; i < result->detections.size(); ++i) {
    if (!suppressed[i]) {
      filtered.detections.push_back(result->detections[i]);
    }
  }
  *result = std::move(filtered);
}

void Yolov8Detector::Release() {
  release_tensor(input_);
  for (int i = 0; i < kNumHeads; ++i) {
    release_tensor(outputs_[i]);
  }
  ReleaseAIPreprocessPipe(pipe_offline_);
  std::printf("[YOLOv8] Released\n");
}

const std::string& Yolov8Detector::ClassName(int id) const {
  static const std::string kEmpty;
  if (id < 0 || id >= static_cast<int>(class_names_.size())) return kEmpty;
  return class_names_[id];
}

void Yolov8Detector::SetClassNames(const std::vector<std::string>& names) {
  class_names_ = names;
}

}  // namespace ssne_demo
