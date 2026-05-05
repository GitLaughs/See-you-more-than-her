/*
 * @Filename: rps_classifier.cpp
 * @Description: 5-class single-label classifier implementation
 */
#include "../include/rps_classifier.hpp"
#include "../include/utils.hpp"

#include <algorithm>
#include <cstdio>

namespace {
constexpr const char* kLabels[] = {"person", "stop", "forward", "obstacle", "NoTarget"};
constexpr int kNumClasses = 5;
constexpr float kNoTargetThreshold = 0.6f;
}

bool RPS_CLASSIFIER::Initialize(std::string& model_path, std::array<int, 2>* in_img_shape,
                                std::array<int, 2>* in_cls_shape) {
    img_shape = *in_img_shape;
    cls_shape = *in_cls_shape;

    char* model_path_char = const_cast<char*>(model_path.c_str());
    model_id = ssne_loadmodel(model_path_char, SSNE_STATIC_ALLOC);
    const int input_num = ssne_get_model_input_num(model_id);
    if (input_num != 1) {
        printf("[ERROR] classifier model load failed: %s model_id=%u input_num=%d\n",
               model_path.c_str(), model_id, input_num);
        return false;
    }

    inputs[0] = create_tensor(static_cast<uint32_t>(cls_shape[0]),
                              static_cast<uint32_t>(cls_shape[1]),
                              SSNE_Y_8, SSNE_BUF_AI);

    const int crop_w = cls_shape[0];
    const int crop_h = cls_shape[1];
    const int crop_x0 = (img_shape[0] - crop_w) / 2;
    const int crop_y0 = (img_shape[1] - crop_h) / 2;
    SetCrop(pipe_offline, crop_x0, crop_y0, crop_x0 + crop_w, crop_y0 + crop_h);
    SetNormalize(pipe_offline, model_id);

    printf("[RPS] classifier initialized model=%s input=%dx%d camera=%dx%d crop=(%d,%d,%d,%d)\n",
           model_path.c_str(), cls_shape[0], cls_shape[1], img_shape[0], img_shape[1],
           crop_x0, crop_y0, crop_x0 + crop_w, crop_y0 + crop_h);
    return true;
}

void RPS_CLASSIFIER::Predict(ssne_tensor_t* img, std::string& out_label, float& out_score, float out_scores[5]) {
    const int preprocess_ret = RunAiPreprocessPipe(pipe_offline, *img, inputs[0]);
    if (preprocess_ret != 0) {
        printf("[ERROR] classifier preprocess failed ret=%d\n", preprocess_ret);
        out_label = "NoTarget";
        out_score = 0.0f;
        if (out_scores) {
            for (int i = 0; i < kNumClasses; ++i) out_scores[i] = 0.0f;
        }
        return;
    }

    int dtype = -1;
    ssne_get_model_input_dtype(model_id, &dtype);
    set_data_type(inputs[0], dtype);

    if (ssne_inference(model_id, 1, inputs)) {
        fprintf(stderr, "[ERROR] classifier inference failed\n");
        out_label = "NoTarget";
        out_score = 0.0f;
        if (out_scores) {
            for (int i = 0; i < kNumClasses; ++i) out_scores[i] = 0.0f;
        }
        return;
    }

    ssne_getoutput(model_id, 1, outputs);
    float* data = static_cast<float*>(get_data(outputs[0]));

    float scores[kNumClasses] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < kNumClasses; ++i) {
        scores[i] = data[i];
    }
    if (out_scores) {
        for (int i = 0; i < kNumClasses; ++i) {
            out_scores[i] = scores[i];
        }
    }

    int max_idx = 0;
    float max_score = scores[0];
    for (int i = 1; i < kNumClasses; ++i) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            max_idx = i;
        }
    }

    out_score = max_score;
    out_label = max_score >= kNoTargetThreshold ? kLabels[max_idx] : "NoTarget";
}

void RPS_CLASSIFIER::Release() {
    release_tensor(inputs[0]);
    release_tensor(outputs[0]);
    ReleaseAIPreprocessPipe(pipe_offline);
}
