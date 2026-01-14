/*
 * @Filename: yolov7.cpp
 * @Author: Twyla Tu
 * @Email: qian.tu@smartsenstech.com
 * @Date: 2024-08-08 15-08-02
 * @Copyright (c) 2024 SmartSens
 */
#include <assert.h>
#include "../include/common.hpp"
#include <iostream>


void DetectionResult::Free() {
  std::vector<std::array<float, 4>>().swap(boxes);
  std::vector<float>().swap(scores);
  std::vector<std::array<float, 2>>().swap(landmarks);
  landmarks_per_obj = 0;
}

void DetectionResult::Clear() {
  boxes.clear();
  scores.clear();
  landmarks.clear();
  landmarks_per_obj = 0;
}

void DetectionResult::Reserve(int size) {
  boxes.reserve(size);
  scores.reserve(size);
  if (landmarks_per_obj > 0) {
    landmarks.reserve(size * landmarks_per_obj);
  }
}

void DetectionResult::Resize(int size) {
  boxes.resize(size);
  scores.resize(size);
  if (landmarks_per_obj > 0) {
    landmarks.resize(size * landmarks_per_obj);
  }
}

DetectionResult::DetectionResult(const DetectionResult& res) {
  boxes.assign(res.boxes.begin(), res.boxes.end());
  landmarks.assign(res.landmarks.begin(), res.landmarks.end());
  scores.assign(res.scores.begin(), res.scores.end());
  landmarks_per_obj = res.landmarks_per_obj;
}

// void YOLOV7::Postprocess(DetectionResult* result, float* conf_threshold) {
//     // 对推理结果进行解码
//     //DecodeBoxes(*boxes);

//     // 过滤低分结果
//     //size_t num_res = boxes->size();
//     //result->Clear();
//     //result->Reserve(num_res);
//     //int res_count = 0;
//     //for (unsigned int i = 0; i < num_res; i++) {
//     //    float score = scores->at(i);
//     //    if (score <= *conf_threshold) {
//     //        continue;
//     //    }
//     //    result->boxes.emplace_back(boxes->at(i));
//     //    result->scores.push_back(scores->at(i));
//     //    res_count += 1;
//     //}
//     //result->Resize(res_count);
    
//     // 执行NMS
//     utils::NMS(result, nms_threshold, top_k);
    
//     // 恢复尺度
//     int res_count = static_cast<int>(result->boxes.size());
//     result->Resize(std::min(res_count, keep_top_k));

//     for (unsigned int i = 0; i < result->boxes.size(); i++) {
//         result->boxes[i][0] = result->boxes[i][0] * det_scale;
//         result->boxes[i][1] = result->boxes[i][1] * det_scale;
//         result->boxes[i][2] = result->boxes[i][2] * det_scale;
//         result->boxes[i][3] = result->boxes[i][3] * det_scale;
//     }
// }

void YOLOV7::Initialize(std::string& model_path) {
    // nms_threshold = 0.5;
    // keep_top_k = 9;
    // top_k = 150;
    // det_scale = *in_scale;
    // img_shape = *in_img_shape;
    // det_shape = *in_det_shape;
    // use_kps = in_use_kps;
    // box_len = in_box_len;

    // min_sizes_yolo = {{{ 12,  16}, { 19,  36}, { 40,  28}},
    //              {{ 36,  75}, { 76,  55}, { 72, 146}},
    //              {{142, 110}, {192, 243}, {459, 401}}};
    // steps = {8, 16, 32};
    // variance = {0.1, 0.2};
    // clip = false;
    // ratios = {1.0};
    // 生成anchor box
    // GenerateBoxes();
    
    char* model_path_char = const_cast<char*>(model_path.c_str());
    std::cout << "model_file name " << model_path_char << std::endl;
    model_id = ssne_loadmodel(model_path_char, SSNE_STATIC_ALLOC);
}

void YOLOV7::Predict(ssne_tensor_t input[], ssne_tensor_t output[]) {
    //auto start = std::chrono::high_resolution_clock::now();
    // 前向推理
    if (ssne_inference(model_id, 1, input))
    {
        fprintf(stderr, "ssne inference fail!\n");
    }
    ssne_getoutput(model_id, 6, output);

    //auto end = std::chrono::high_resolution_clock::now();
    //auto duration = std::chrono::duration<double, std::milli>(end - start);
    //durations_forward.push_back(duration);

    /*
    ssne_tensor_t gt_output;
    int cmp_res = -1;
    gt_output = create_tensor_from_file("/ai/imgs/output0.bin", SSNE_BUF_AI);
    cmp_res = compare_tensor(output[0],gt_output, 1);
    fprintf(stderr, "cmp_res %d!\n",cmp_res);
    cmp_res = -1;
    gt_output = create_tensor_from_file("/ai/imgs/output1.bin", SSNE_BUF_AI);
    cmp_res = compare_tensor(output[1],gt_output, 1);
    fprintf(stderr, "cmp_res %d!\n",cmp_res);
    cmp_res = -1;
    gt_output = create_tensor_from_file("/ai/imgs/output2.bin", SSNE_BUF_AI);
    cmp_res = compare_tensor(output[2],gt_output, 1);
    fprintf(stderr, "cmp_res %d!\n",cmp_res);
    */
}

void YOLOV7::Release()
{
    // release_tensor(output[0]);
    // release_tensor(output[1]);
    // release_tensor(output[2]);
    // release_tensor(output[3]);
    // release_tensor(output[4]);
    // release_tensor(output[5]);

    // 计时
    durations_forward.clear();
    // durations_postprocess.clear();
}
