/*
 * @Filename: yolov8_detector.cpp
 * @Description: YOLOv8 head6 detector — 640×640 RGB, 6 outputs, DFL + per-class NMS
 */
#include "../include/common.hpp"
#include "../include/utils.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace {
constexpr int kNumClasses  = 4;
constexpr int kRegBins     = 16;
constexpr int kRegChannels = 64;
constexpr std::array<int, 3> kStrides  = {8, 16, 32};
constexpr std::array<int, 3> kHeights  = {80, 40, 20};
constexpr std::array<int, 3> kWidths   = {80, 40, 20};
}

float YOLOV8::Sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

float YOLOV8::IoU(const std::array<float, 4>& a, const std::array<float, 4>& b) {
    float inter_x1 = std::max(a[0], b[0]);
    float inter_y1 = std::max(a[1], b[1]);
    float inter_x2 = std::min(a[2], b[2]);
    float inter_y2 = std::min(a[3], b[3]);
    if (inter_x2 <= inter_x1 || inter_y2 <= inter_y1) return 0.0f;
    float inter_area = (inter_x2 - inter_x1) * (inter_y2 - inter_y1);
    float area_a = (a[2] - a[0]) * (a[3] - a[1]);
    float area_b = (b[2] - b[0]) * (b[3] - b[1]);
    return inter_area / (area_a + area_b - inter_area);
}

bool YOLOV8::Initialize(std::string& model_path, std::array<int, 2>* in_img_shape,
                        std::array<int, 2>* in_det_shape) {
    img_shape = *in_img_shape;
    det_shape = *in_det_shape;

    int crop_size = std::min(img_shape[0], img_shape[1]);
    crop_x0 = (img_shape[0] - crop_size) / 2;
    int crop_y0 = (img_shape[1] - crop_size) / 2;

    w_scale = static_cast<float>(crop_size) / det_shape[0];
    h_scale = static_cast<float>(crop_size) / det_shape[1];

    printf("[YOLOV8] img=%dx%d det=%dx%d crop=%d offset=(%d,%d) scale=%.3f\n",
           img_shape[0], img_shape[1], det_shape[0], det_shape[1],
           crop_size, crop_x0, crop_y0, w_scale);

    char* model_path_char = const_cast<char*>(model_path.c_str());
    model_id = ssne_loadmodel(model_path_char, SSNE_STATIC_ALLOC);
    if (model_id == 0) {
        printf("[YOLOV8] ssne_loadmodel failed: %s\n", model_path.c_str());
        return false;
    }

    uint32_t det_w = static_cast<uint32_t>(det_shape[0]);
    uint32_t det_h = static_cast<uint32_t>(det_shape[1]);
    inputs[0] = create_tensor(det_w, det_h, SSNE_RGB, SSNE_BUF_AI);

    int ret = SetCrop(pipe_offline, crop_x0, crop_y0, crop_x0 + crop_size, crop_y0 + crop_size);
    if (ret != 0) {
        printf("[YOLOV8] SetCrop failed: %d\n", ret);
        return false;
    }
    ret = SetNormalize(pipe_offline, model_id);
    if (ret != 0) {
        printf("[YOLOV8] SetNormalize failed: %d\n", ret);
        return false;
    }

    printf("[YOLOV8] initialized, model_id=%u\n", model_id);
    return true;
}

void YOLOV8::DecodeHeadOutputs(const float* cls_head, const float* reg_head,
                               int height, int width, int stride,
                               float conf_threshold,
                               std::vector<std::array<float, 4>>& boxes,
                               std::vector<float>& scores,
                               std::vector<int>& class_ids) {
    std::array<float, kRegBins> softmax_buf;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // ── Classification: sigmoid + best class ──
            const int cls_offset = (y * width + x) * kNumClasses;
            int best_class = 0;
            float best_score = Sigmoid(cls_head[cls_offset]);
            for (int c = 1; c < kNumClasses; ++c) {
                float s = Sigmoid(cls_head[cls_offset + c]);
                if (s > best_score) { best_score = s; best_class = c; }
            }
            if (best_score < conf_threshold) continue;

            // ── Regression: DFL softmax decode (4 sides × 16 bins) ──
            const int reg_offset = (y * width + x) * kRegChannels;
            std::array<float, 4> dist;
            for (int side = 0; side < 4; ++side) {
                const int side_off = reg_offset + side * kRegBins;
                float max_logit = reg_head[side_off];
                for (int bin = 1; bin < kRegBins; ++bin)
                    max_logit = std::max(max_logit, reg_head[side_off + bin]);
                float sum = 0.0f;
                for (int bin = 0; bin < kRegBins; ++bin) {
                    softmax_buf[bin] = std::exp(reg_head[side_off + bin] - max_logit);
                    sum += softmax_buf[bin];
                }
                float expect = 0.0f;
                for (int bin = 0; bin < kRegBins; ++bin)
                    expect += (softmax_buf[bin] / sum) * static_cast<float>(bin);
                dist[side] = expect;
            }

            // ── Bbox decode (anchor-free, center-point based) ──
            float cx = static_cast<float>(x) + 0.5f;
            float cy = static_cast<float>(y) + 0.5f;
            float x1 = (cx - dist[0]) * static_cast<float>(stride);
            float y1 = (cy - dist[1]) * static_cast<float>(stride);
            float x2 = (cx + dist[2]) * static_cast<float>(stride);
            float y2 = (cy + dist[3]) * static_cast<float>(stride);

            x1 = std::max(0.0f, std::min(x1, static_cast<float>(det_shape[0])));
            y1 = std::max(0.0f, std::min(y1, static_cast<float>(det_shape[1])));
            x2 = std::max(0.0f, std::min(x2, static_cast<float>(det_shape[0])));
            y2 = std::max(0.0f, std::min(y2, static_cast<float>(det_shape[1])));

            boxes.push_back({x1, y1, x2, y2});
            scores.push_back(best_score);
            class_ids.push_back(best_class);
        }
    }
}

void YOLOV8::Postprocess(std::vector<std::array<float, 4>>* boxes,
                         std::vector<float>* scores,
                         std::vector<int>* class_ids,
                         DetectionResult* result) {
    const int n = static_cast<int>(boxes->size());
    if (n == 0) { result->Clear(); return; }

    // sort by score descending
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return (*scores)[a] > (*scores)[b];
    });
    if (n > top_k) order.resize(top_k);

    // per-class NMS
    std::vector<int> keep;
    for (int idx : order) {
        bool suppressed = false;
        for (int kept_idx : keep) {
            if ((*class_ids)[idx] != (*class_ids)[kept_idx]) continue;
            if (IoU((*boxes)[idx], (*boxes)[kept_idx]) > nms_threshold) {
                suppressed = true;
                break;
            }
        }
        if (!suppressed) keep.push_back(idx);
        if (static_cast<int>(keep.size()) >= keep_top_k) break;
    }

    // scale to crop space, then offset to full image space
    result->Clear();
    for (int idx : keep) {
        auto box = (*boxes)[idx];
        box[0] = std::max(0.0f, box[0] * w_scale + static_cast<float>(crop_x0));
        box[1] = std::max(0.0f, box[1] * h_scale);
        box[2] = std::min(static_cast<float>(img_shape[0]), box[2] * w_scale + static_cast<float>(crop_x0));
        box[3] = std::min(static_cast<float>(img_shape[1]), box[3] * h_scale);
        result->boxes.emplace_back(box);
        result->scores.emplace_back((*scores)[idx]);
        result->class_ids.emplace_back((*class_ids)[idx]);
    }
}

void YOLOV8::Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold) {
    int ret = RunAiPreprocessPipe(pipe_offline, *img_in, inputs[0]);
    if (ret != 0) {
        printf("[YOLOV8] RunAiPreprocessPipe failed: %d\n", ret);
        result->Clear();
        return;
    }

    int dtype = -1;
    ssne_get_model_input_dtype(model_id, &dtype);
    set_data_type(inputs[0], dtype);

    ret = ssne_inference(model_id, 1, inputs);
    if (ret != 0) {
        printf("[YOLOV8] ssne_inference failed: %d\n", ret);
        result->Clear();
        return;
    }

    ssne_getoutput(model_id, 6, outputs);

    const float* cls0 = static_cast<const float*>(get_data(outputs[0]));
    const float* cls1 = static_cast<const float*>(get_data(outputs[1]));
    const float* cls2 = static_cast<const float*>(get_data(outputs[2]));
    const float* reg0 = static_cast<const float*>(get_data(outputs[3]));
    const float* reg1 = static_cast<const float*>(get_data(outputs[4]));
    const float* reg2 = static_cast<const float*>(get_data(outputs[5]));

    std::vector<std::array<float, 4>> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;

    DecodeHeadOutputs(cls0, reg0, kHeights[0], kWidths[0], kStrides[0],
                      conf_threshold, boxes, scores, class_ids);
    DecodeHeadOutputs(cls1, reg1, kHeights[1], kWidths[1], kStrides[1],
                      conf_threshold, boxes, scores, class_ids);
    DecodeHeadOutputs(cls2, reg2, kHeights[2], kWidths[2], kStrides[2],
                      conf_threshold, boxes, scores, class_ids);

    Postprocess(&boxes, &scores, &class_ids, result);
}

void YOLOV8::Release() {
    release_tensor(inputs[0]);
    for (int i = 0; i < 6; ++i) release_tensor(outputs[i]);
    ReleaseAIPreprocessPipe(pipe_offline);
    printf("[YOLOV8] released\n");
}
