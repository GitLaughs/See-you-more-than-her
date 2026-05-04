/*
 * @Filename: common.hpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#pragma once

#include <stdio.h>
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <math.h>
#include "smartsoc/ssne_api.h"

class IMAGEPROCESSOR {
  public:
    /** \brief pipe初始化。
      *
      * \param[in] in_img_shape 输入图像尺寸(w, h)。
      * \param[in] in_scale online输入图像和online输出图像之间的尺度倍数（保留参数以兼容接口，但不使用）。
      * \return none
      */
    // void Initialize(std::array<int, 2>* in_img_shape, BinningRatioType in_scale);
    bool Initialize(std::array<int, 2>* in_img_shape);
    /**
     * 获取offline或者online的图像。
     *
     * \param[in] img_sensor // 输出图像, 3-D array with layout HWC, SSNE_Y_8 format。
    */
    void GetImage(ssne_tensor_t* img_sensor);

    // 释放资源
    void Release();

    // 前处理时，模型推理输入的原始待检测图像尺寸，（width，height）
    std::array<int, 2> img_shape;

  private:
    // online setting
    uint8_t format_online;
};


struct DetectionResult {
    std::vector<std::array<float, 4>> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;
    int raw_candidates = 0;
    float top_score = 0.0f;
    int top_class_id = -1;
    bool preprocess_ok = false;
    bool inference_ok = false;
    int input_dtype = -1;
    int decoded_candidates = 0;
    int after_nms_count = 0;
    int score_over_005 = 0;
    int score_over_010 = 0;
    int score_over_025 = 0;
    int score_over_040 = 0;
    std::array<float, 3> head_top_scores = {0.0f, 0.0f, 0.0f};
    std::array<int, 3> head_top_classes = {-1, -1, -1};
    std::string error_stage;
    int error_code = 0;
    bool tensor_dump_printed = false;
    void Clear() {
        boxes.clear();
        scores.clear();
        class_ids.clear();
        raw_candidates = 0;
        top_score = 0.0f;
        top_class_id = -1;
        preprocess_ok = false;
        inference_ok = false;
        input_dtype = -1;
        decoded_candidates = 0;
        after_nms_count = 0;
        score_over_005 = 0;
        score_over_010 = 0;
        score_over_025 = 0;
        score_over_040 = 0;
        head_top_scores = {0.0f, 0.0f, 0.0f};
        head_top_classes = {-1, -1, -1};
        error_stage.clear();
        error_code = 0;
        tensor_dump_printed = false;
    }
};

/**
 * @brief YOLOv8 head6 检测器
 * @description 640×640 RGB 输入，6 输出头 (3 cls + 3 reg)，DFL + per-class NMS
 */
class YOLOV8 {
  public:
    std::string ModelName() const { return "yolov8"; }

    bool Initialize(std::string& model_path, std::array<int, 2>* in_img_shape,
                    std::array<int, 2>* in_det_shape);

    void Predict(ssne_tensor_t* img_in, DetectionResult* result,
                 float conf_threshold = 0.4f, uint64_t frame_index = 0,
                 bool print_tensor_dump = false);

    void Release();

    float nms_threshold = 0.45f;
    int top_k = 150;
    int keep_top_k = 30;

  private:
    void DecodeHeadOutputs(const float* cls_head, const float* reg_head,
                           int height, int width, int stride,
                           float conf_threshold,
                           std::vector<std::array<float, 4>>& boxes,
                           std::vector<float>& scores,
                           std::vector<int>& class_ids);

    void Postprocess(std::vector<std::array<float, 4>>* boxes,
                     std::vector<float>* scores,
                     std::vector<int>* class_ids,
                     DetectionResult* result);

    static float Sigmoid(float x);
    static float IoU(const std::array<float, 4>& a, const std::array<float, 4>& b);

    uint16_t model_id = 0;
    ssne_tensor_t inputs[1];
    ssne_tensor_t outputs[6];
    AiPreprocessPipe pipe_offline = GetAIPreprocessPipe();
    std::array<int, 2> img_shape;
    std::array<int, 2> det_shape;
    float w_scale;
    float h_scale;
    int crop_x0;
};
