/*
 * @Filename: yolov8_gray.cpp
 * @Description: YOLOv8 灰度图人物检测器实现 (head6 切分模型, CPU 后处理)
 *
 * 处理流程:
 *   SC132GS 1280×720 Y8
 *     → RunAiPreprocessPipe 硬件缩放 → 640×360 Y8
 *     → ssne_inference (M1 NPU)
 *     → ssne_getoutput (6 head: cv3×3 分类 + cv2×3 回归)
 *     → DFL softmax decode + sigmoid + NMS (CPU)
 *     → 坐标 ×2 映射回 1280×720
 *
 * 输出 head 格式 (NPU 输出, NHWC 布局):
 *   outputs[0]: cls stride-8  [H0, W0, num_cls]
 *   outputs[1]: cls stride-16 [H1, W1, num_cls]
 *   outputs[2]: cls stride-32 [H2, W2, num_cls]
 *   outputs[3]: reg stride-8  [H0, W0, 64]
 *   outputs[4]: reg stride-16 [H1, W1, 64]
 *   outputs[5]: reg stride-32 [H2, W2, 64]
 *
 * 特征图尺寸 (640×360 输入, ceil 整除):
 *   stride  8: W=80, H=45
 *   stride 16: W=40, H=23  (ceil(45/2)=23)
 *   stride 32: W=20, H=12  (ceil(23/2)=12)
 */

#include "../include/common.hpp"
#include "../include/utils.hpp"
#include "../project_paths.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <sys/time.h>
#include <unistd.h>

// ─── DFL/YOLOv8 专用常量 ─────────────────────────────────────────────────────
static constexpr int kNumClasses  = cfg::YOLO_NUM_CLASSES;  // 训练类别数
static constexpr int kRegBins     = cfg::YOLO_REG_BINS;     // DFL bins (16)
static constexpr int kRegChannels = kRegBins * 4;           // = 64

namespace {
uint64_t monotonic_time_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000ULL +
           static_cast<uint64_t>(tv.tv_usec);
}
}

// ─── 特征图尺寸计算辅助 ───────────────────────────────────────────────────────
static inline int ceildiv(int a, int b) { return (a + b - 1) / b; }

// ─────────────────────────────────────────────────────────────────────────────
// DecodeHeadOutputs  —  解码单个 FPN 尺度的输出
// ─────────────────────────────────────────────────────────────────────────────
void YOLOV8::DecodeHeadOutputs(
        const float* cls_head, const float* reg_head,
        int height, int width, int stride,
        float conf_threshold,
        std::vector<std::array<float, 4>>& boxes,
        std::vector<float>&               scores,
        std::vector<int>&                 class_ids)
{
    std::array<float, kRegBins> softmax_buf = {};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {

            // ── 1. 找最高置信度类别 (sigmoid 激活) ────────────────────────
            const int cls_offset = (y * width + x) * kNumClasses;
            int   best_cls   = 0;
            float best_score = Sigmoid(cls_head[cls_offset]);

            for (int c = 1; c < kNumClasses; ++c) {
                float s = Sigmoid(cls_head[cls_offset + c]);
                if (s > best_score) {
                    best_score = s;
                    best_cls   = c;
                }
            }

            if (best_score < conf_threshold) continue;

            // ── 2. DFL 解码 4 个边距 (left/top/right/bottom) ────────────
            const int reg_offset = (y * width + x) * kRegChannels;
            std::array<float, 4> dist = {};

            for (int side = 0; side < 4; ++side) {
                const int side_offset = reg_offset + side * kRegBins;

                // softmax (numerically stable: subtract max first)
                float max_logit = reg_head[side_offset];
                for (int b = 1; b < kRegBins; ++b)
                    max_logit = std::max(max_logit, reg_head[side_offset + b]);

                float sum = 0.0f;
                for (int b = 0; b < kRegBins; ++b) {
                    softmax_buf[b] = std::exp(reg_head[side_offset + b] - max_logit);
                    sum += softmax_buf[b];
                }

                float expectation = 0.0f;
                for (int b = 0; b < kRegBins; ++b)
                    expectation += (softmax_buf[b] / sum) * static_cast<float>(b);

                dist[side] = expectation;
            }

            // ── 3. 锚点中心 → xyxy 绝对坐标 ──────────────────────────────
            const float anchor_x = static_cast<float>(x) + 0.5f;
            const float anchor_y = static_cast<float>(y) + 0.5f;

            float x1 = (anchor_x - dist[0]) * static_cast<float>(stride);
            float y1 = (anchor_y - dist[1]) * static_cast<float>(stride);
            float x2 = (anchor_x + dist[2]) * static_cast<float>(stride);
            float y2 = (anchor_y + dist[3]) * static_cast<float>(stride);

            // 钳位到检测输入分辨率范围内
            x1 = std::max(0.0f, std::min(x1, static_cast<float>(det_shape[0])));
            y1 = std::max(0.0f, std::min(y1, static_cast<float>(det_shape[1])));
            x2 = std::max(0.0f, std::min(x2, static_cast<float>(det_shape[0])));
            y2 = std::max(0.0f, std::min(y2, static_cast<float>(det_shape[1])));

            boxes.push_back({x1, y1, x2, y2});
            scores.push_back(best_score);
            class_ids.push_back(best_cls);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialize
// ─────────────────────────────────────────────────────────────────────────────
void YOLOV8::Initialize(std::string&        model_path,
                        std::array<int, 2>* in_img_shape,
                        std::array<int, 2>* in_det_shape)
{
    img_shape = *in_img_shape;
    det_shape = *in_det_shape;

    // 坐标缩放比: det -> sensor
    w_scale = static_cast<float>(img_shape[0]) / static_cast<float>(det_shape[0]);
    h_scale = static_cast<float>(img_shape[1]) / static_cast<float>(det_shape[1]);

    printf("[YOLOV8] 初始化: img=%dx%d  det=%dx%d  scale=%.1fx%.1f\n",
           img_shape[0], img_shape[1], det_shape[0], det_shape[1],
           w_scale, h_scale);

    if (access(model_path.c_str(), F_OK) != 0) {
        fprintf(stderr, "[YOLOV8][ERROR] 模型文件不存在: %s\n", model_path.c_str());
    } else {
        printf("[YOLOV8] 模型文件存在: %s\n", model_path.c_str());
    }

    // 加载 NPU 模型
    char* path = const_cast<char*>(model_path.c_str());
    model_id   = ssne_loadmodel(path, SSNE_STATIC_ALLOC);
    if (model_id < 0) {
        fprintf(stderr, "[YOLOV8][ERROR] ssne_loadmodel failed, model_id=%d, path=%s\n",
                model_id, model_path.c_str());
    } else {
        printf("[YOLOV8] 模型加载完成, model_id=%d\n", model_id);
    }

    // 创建推理输入 tensor (640×360 Y8)
    const uint32_t dw = static_cast<uint32_t>(det_shape[0]);
    const uint32_t dh = static_cast<uint32_t>(det_shape[1]);
    inputs[0] = create_tensor(dw, dh, SSNE_Y_8, SSNE_BUF_AI);
    printf("[YOLOV8] 输入 tensor: %ux%u Y8\n", dw, dh);
}

// ─────────────────────────────────────────────────────────────────────────────
// Predict
// ─────────────────────────────────────────────────────────────────────────────
void YOLOV8::Predict(ssne_tensor_t* img, FaceDetectionResult* result,
                     float conf_threshold)
{
    static uint64_t last_error_log_us = 0;
    static uint64_t last_detect_log_us = 0;
    if (model_id < 0) {
        const uint64_t now_us = monotonic_time_us();
        if (now_us - last_error_log_us >= 5000000ULL) {
            fprintf(stderr, "[YOLOV8][ERROR] 跳过推理: model_id=%d (模型未成功加载)\n", model_id);
            last_error_log_us = now_us;
        }
        return;
    }

    // ── 1. 硬件预处理: 1280×720 → 640×360 ────────────────────────────────
    int ret = RunAiPreprocessPipe(pipe_offline, *img, inputs[0]);
    if (ret != 0) {
        const uint64_t now_us = monotonic_time_us();
        if (now_us - last_error_log_us >= 5000000ULL) {
            printf("[YOLOV8][ERROR] RunAiPreprocessPipe failed, ret=%d\n", ret);
            last_error_log_us = now_us;
        }
        return;
    }

    // ── 2. NPU 推理 ───────────────────────────────────────────────────────
    ret = ssne_inference(model_id, 1, inputs);
    if (ret != 0) {
        const uint64_t now_us = monotonic_time_us();
        if (now_us - last_error_log_us >= 5000000ULL) {
            fprintf(stderr, "[YOLOV8][ERROR] ssne_inference failed, ret=%d, model_id=%d\n",
                    ret, model_id);
            last_error_log_us = now_us;
        }
        return;
    }

    // ── 3. 获取 6 个 head 输出 ────────────────────────────────────────────
    ssne_getoutput(model_id, 6, outputs);

    // cls heads: outputs[0..2]  |  reg heads: outputs[3..5]
    const float* cls0 = static_cast<const float*>(get_data(outputs[0]));
    const float* cls1 = static_cast<const float*>(get_data(outputs[1]));
    const float* cls2 = static_cast<const float*>(get_data(outputs[2]));
    const float* reg0 = static_cast<const float*>(get_data(outputs[3]));
    const float* reg1 = static_cast<const float*>(get_data(outputs[4]));
    const float* reg2 = static_cast<const float*>(get_data(outputs[5]));

    // ── 4. 三个 FPN 尺度逐一解码 ─────────────────────────────────────────
    //   特征图尺寸 (640×360 输入, ceil 整除):
    //     stride  8: W=80, H=45
    //     stride 16: W=40, H=23
    //     stride 32: W=20, H=12
    const int w0 = det_shape[0] / 8,                   h0 = det_shape[1] / 8;
    const int w1 = det_shape[0] / 16,                  h1 = ceildiv(det_shape[1], 16);
    const int w2 = det_shape[0] / 32,                  h2 = ceildiv(det_shape[1], 32);

    std::vector<std::array<float, 4>> boxes;
    std::vector<float>                scores_vec;
    std::vector<int>                  class_ids;

    boxes.reserve(h0*w0 + h1*w1 + h2*w2);
    scores_vec.reserve(h0*w0 + h1*w1 + h2*w2);
    class_ids.reserve(h0*w0 + h1*w1 + h2*w2);

    DecodeHeadOutputs(cls0, reg0,  h0, w0,  8, conf_threshold, boxes, scores_vec, class_ids);
    DecodeHeadOutputs(cls1, reg1,  h1, w1, 16, conf_threshold, boxes, scores_vec, class_ids);
    DecodeHeadOutputs(cls2, reg2,  h2, w2, 32, conf_threshold, boxes, scores_vec, class_ids);

    // ── 5. 填充检测结果 (person-only, 复用现有 NMS) ─────────────────────
    result->Clear();
    result->Reserve(static_cast<int>(boxes.size()));
    for (size_t i = 0; i < boxes.size(); ++i) {
        result->boxes.push_back(boxes[i]);
        result->scores.push_back(scores_vec[i]);
        result->class_ids.push_back(class_ids[i]);
    }

    // ── 6. NMS ────────────────────────────────────────────────────────────
    utils::NMS(result, nms_threshold, top_k);

    // ── 7. 坐标映射回原图 (640×360 → 1280×720) ───────────────────────────
    const int final_count = std::min(static_cast<int>(result->boxes.size()), keep_top_k);
    result->Resize(final_count);
    for (int i = 0; i < final_count; ++i) {
        result->boxes[i][0] *= w_scale;
        result->boxes[i][1] *= h_scale;
        result->boxes[i][2] *= w_scale;
        result->boxes[i][3] *= h_scale;
    }

    const uint64_t now_us = monotonic_time_us();
    if (now_us - last_detect_log_us >= 5000000ULL) {
        printf("[YOLOV8] 检测到 %d 个目标 (conf>=%.2f)\n",
               final_count, conf_threshold);
        last_detect_log_us = now_us;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Release
// ─────────────────────────────────────────────────────────────────────────────
void YOLOV8::Release()
{
    release_tensor(inputs[0]);
    for (int i = 0; i < 6; ++i)
        release_tensor(outputs[i]);
    ReleaseAIPreprocessPipe(pipe_offline);
    printf("[YOLOV8] 资源已释放\n");
}
