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
int raw_candidate_count_current = 0;
float top_score_current = 0.0f;
int top_class_current = -1;
int score_over_005_current = 0;
int score_over_010_current = 0;
int score_over_025_current = 0;
int score_over_040_current = 0;
std::array<float, 3> head_top_scores_current = {0.0f, 0.0f, 0.0f};
std::array<int, 3> head_top_classes_current = {-1, -1, -1};
constexpr int kOutputTensorCount = 6;
constexpr int kOutputSampleCount = 5;

struct OutputShape {
    int n;
    int h;
    int w;
    int c;
};

constexpr std::array<OutputShape, kOutputTensorCount> kOutputShapes = {{
    {1, 80, 80, 4},
    {1, 40, 40, 4},
    {1, 20, 20, 4},
    {1, 80, 80, 64},
    {1, 40, 40, 64},
    {1, 20, 20, 64},
}};

void PrintOutputTensorDebug(uint64_t frame_index, ssne_tensor_t* outputs) {
    printf("[YOLOV8_TENSOR_OUTPUT_BEGIN] frame=%llu\n", static_cast<unsigned long long>(frame_index));
    printf("Output tensor count: %d\n", kOutputTensorCount);
    for (int i = 0; i < kOutputTensorCount; ++i) {
        const auto& shape = kOutputShapes[i];
        const int tensor_size = shape.n * shape.h * shape.w * shape.c;
        const float* data = static_cast<const float*>(get_data(outputs[i]));
        printf("Output[%d] shape: [%d, %d, %d, %d]\n", i, shape.n, shape.h, shape.w, shape.c);
        printf("Output[%d] sdk_width=%u sdk_height=%u sdk_bytes=%zu sdk_dtype=%u\n",
               i, get_width(outputs[i]), get_height(outputs[i]), get_mem_size(outputs[i]), get_data_type(outputs[i]));
        printf("First 5 values: ");
        for (int j = 0; j < kOutputSampleCount && j < tensor_size; ++j) {
            printf("%f ", data ? data[j] : 0.0f);
        }
        printf("\n");
    }
    printf("[YOLOV8_TENSOR_OUTPUT_END] frame=%llu\n", static_cast<unsigned long long>(frame_index));
}
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
    printf("[YOLOV8] loaded model_id=%u path=%s\n", model_id, model_path.c_str());

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
    int head_index = 0;
    if (stride == 16) head_index = 1;
    else if (stride == 32) head_index = 2;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int cls_offset = (y * width + x) * kNumClasses;
            int best_class = 0;
            float best_score = Sigmoid(cls_head[cls_offset]);
            for (int c = 1; c < kNumClasses; ++c) {
                float s = Sigmoid(cls_head[cls_offset + c]);
                if (s > best_score) { best_score = s; best_class = c; }
            }
            ++raw_candidate_count_current;
            if (best_score > top_score_current) {
                top_score_current = best_score;
                top_class_current = best_class;
            }
            if (best_score > head_top_scores_current[head_index]) {
                head_top_scores_current[head_index] = best_score;
                head_top_classes_current[head_index] = best_class;
            }
            if (best_score >= 0.05f) ++score_over_005_current;
            if (best_score >= 0.10f) ++score_over_010_current;
            if (best_score >= 0.25f) ++score_over_025_current;
            if (best_score >= 0.40f) ++score_over_040_current;
            if (best_score < conf_threshold) continue;

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
    if (n == 0) {
        result->boxes.clear();
        result->scores.clear();
        result->class_ids.clear();
        return;
    }

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

    result->boxes.clear();
    result->scores.clear();
    result->class_ids.clear();
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

void YOLOV8::Predict(ssne_tensor_t* img_in, DetectionResult* result, float conf_threshold, uint64_t frame_index, bool print_tensor_dump) {
    result->Clear();

    int ret = RunAiPreprocessPipe(pipe_offline, *img_in, inputs[0]);
    if (ret != 0) {
        printf("[YOLOV8] RunAiPreprocessPipe failed: %d\n", ret);
        result->error_stage = "preprocess";
        result->error_code = ret;
        return;
    }
    result->preprocess_ok = true;

    int dtype = -1;
    ssne_get_model_input_dtype(model_id, &dtype);
    result->input_dtype = dtype;
    set_data_type(inputs[0], dtype);

    ret = ssne_inference(model_id, 1, inputs);
    if (ret != 0) {
        printf("[YOLOV8] ssne_inference failed: %d\n", ret);
        result->error_stage = "inference";
        result->error_code = ret;
        return;
    }
    result->inference_ok = true;

    ssne_getoutput(model_id, 6, outputs);
    if (print_tensor_dump) {
        PrintOutputTensorDebug(frame_index, outputs);
        result->tensor_dump_printed = true;
    }

    const float* cls0 = static_cast<const float*>(get_data(outputs[0]));
    const float* cls1 = static_cast<const float*>(get_data(outputs[1]));
    const float* cls2 = static_cast<const float*>(get_data(outputs[2]));
    const float* reg0 = static_cast<const float*>(get_data(outputs[3]));
    const float* reg1 = static_cast<const float*>(get_data(outputs[4]));
    const float* reg2 = static_cast<const float*>(get_data(outputs[5]));

    std::vector<std::array<float, 4>> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;

    raw_candidate_count_current = 0;
    top_score_current = 0.0f;
    top_class_current = -1;
    score_over_005_current = 0;
    score_over_010_current = 0;
    score_over_025_current = 0;
    score_over_040_current = 0;
    head_top_scores_current = {0.0f, 0.0f, 0.0f};
    head_top_classes_current = {-1, -1, -1};

    DecodeHeadOutputs(cls0, reg0, kHeights[0], kWidths[0], kStrides[0],
                      conf_threshold, boxes, scores, class_ids);
    DecodeHeadOutputs(cls1, reg1, kHeights[1], kWidths[1], kStrides[1],
                      conf_threshold, boxes, scores, class_ids);
    DecodeHeadOutputs(cls2, reg2, kHeights[2], kWidths[2], kStrides[2],
                      conf_threshold, boxes, scores, class_ids);

    const int decoded_candidates = static_cast<int>(boxes.size());
    Postprocess(&boxes, &scores, &class_ids, result);
    result->raw_candidates = raw_candidate_count_current;
    result->top_score = top_score_current;
    result->top_class_id = top_class_current;
    result->decoded_candidates = decoded_candidates;
    result->after_nms_count = static_cast<int>(result->boxes.size());
    result->score_over_005 = score_over_005_current;
    result->score_over_010 = score_over_010_current;
    result->score_over_025 = score_over_025_current;
    result->score_over_040 = score_over_040_current;
    result->head_top_scores = head_top_scores_current;
    result->head_top_classes = head_top_classes_current;
}

void YOLOV8::Release() {
    release_tensor(inputs[0]);
    for (int i = 0; i < 6; ++i) release_tensor(outputs[i]);
    ReleaseAIPreprocessPipe(pipe_offline);
    printf("[YOLOV8] released\n");
}
