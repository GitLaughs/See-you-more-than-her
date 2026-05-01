/*
 * @Filename: pipeline_image.cpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#include "../include/common.hpp"
#include <iostream>
#include <unistd.h>

void IMAGEPROCESSOR::Initialize(std::array<int, 2>* in_img_shape)
{
    img_shape = *in_img_shape;

    uint16_t img_width = static_cast<uint16_t>(img_shape[0]);
    uint16_t img_height = static_cast<uint16_t>(img_shape[1]);
    format_online = SSNE_YUV422_16;

    OnlineSetOutputImage(kPipeline0, format_online, img_width, img_height);

    int res0 = OpenOnlinePipeline(kPipeline0);
    if (res0 != 0) {
        printf("[ERROR] Failed to open online pipeline!\n");
        printf("ret: %d\n", res0);
        return;
    }
    printf("[INFO] open online pipe0: %d \n", res0);
}

void IMAGEPROCESSOR::GetImage(ssne_tensor_t* img_sensor) {
    int capture_code = GetImageData(img_sensor, kPipeline0, kSensor0, 0);
    if (capture_code != 0)
    {
        printf("[IMAGEPROCESSOR] Get Invalid Image from kPipeline0!\n");
    }
}

void IMAGEPROCESSOR::Release()
{
    CloseOnlinePipeline(kPipeline0);
    printf("[INFO] OnlinePipe closed!\n");
}
