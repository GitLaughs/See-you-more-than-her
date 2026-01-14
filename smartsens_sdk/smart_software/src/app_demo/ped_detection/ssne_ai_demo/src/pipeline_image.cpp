/*
 * @Filename: pipeline_image.cpp
 * @Author: Twyla Tu
 * @Email: qian.tu@smartsenstech.com
 * @Date: 2024-08-08 15-07-26
 * @Copyright (c) 2024 SmartSens
 */
#include "../include/common.hpp"
#include <iostream>

void IMAGEPROCESSOR::Initialize(std::array<int, 2>* in_img_shape,
    std::array<int, 2>* in_det_shape, float* in_scale) {
    img_shape = *in_img_shape;
    det_shape = *in_det_shape;
    det_scale = *in_scale;

    // online配置
    uint16_t img_width = static_cast<uint16_t>(img_shape[0]);
    uint16_t img_height = static_cast<uint16_t>(img_shape[1]);

    format_online = SSNE_YUV422_16;

    OnlineSetOutputImage(kPipeline0, format_online, img_width, img_height);
    // printf("[INFO] format_online: %d \n", format_online);
    int ret = OpenOnlinePipeline(kPipeline0);
    // printf("[INFO] open online results: %d \n", ret);
    //printf("[INFO] offline padding height: %d\n", pad_height);

    // offline配置
    // padding, resize, normalize
    // SetPadding(0, pad_height, 0, pad_height, 0);
}

void IMAGEPROCESSOR::GetImage(ssne_tensor_t* img_out) {
    //auto start = std::chrono::high_resolution_clock::now();
    int size_online = 0;
    int capture_code = GetImageData(img_out, kPipeline0, kSensor0, 0);
    size_online = get_total_size(*img_out);
    // printf("[INFO] get offline image size_online: %d \n", size_online);
    if (capture_code != 0)
    {
        printf("Get Invalid Image\n");
    }

    //auto end = std::chrono::high_resolution_clock::now();
    //auto duration = std::chrono::duration<double, std::milli>(end - start);
    //durations_preprocess.push_back(duration);
}

void IMAGEPROCESSOR::ProcessDetections(DetectionResult* det_result) {
    std::array<float, 4> det = {0, 0, 0, 0};
    for (unsigned int i = 0; i < det_result->boxes.size(); i++)
    {
        det = det_result->boxes[i];
        det[0] = det[0] * kDownSample4x;
        det[1] = (det[1] - pad_height) * kDownSample4x;
        det[2] = det[2] * kDownSample4x;
        det[3] = (det[3] - pad_height) * kDownSample4x;
        det_result->boxes[i] = det;
    }
}

void IMAGEPROCESSOR::Release()
{
    // ReleaseAIPreprocessPipe(pipe_offline);
    CloseOnlinePipeline(kPipeline1);
    // printf("[INFO] OnlinePipe closed!\n");

    // 计时
    durations_preprocess.clear();
}
