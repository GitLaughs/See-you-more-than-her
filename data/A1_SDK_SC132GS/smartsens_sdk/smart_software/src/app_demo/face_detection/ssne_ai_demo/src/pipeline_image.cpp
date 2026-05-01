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

/**
 * @brief 图像处理器初始化函数
 * @param in_img_shape 输入图像尺寸 [宽度, 高度]
 * @param in_scale Binning降采样倍数（保留参数以兼容接口，但不使用）
 */
// void IMAGEPROCESSOR::Initialize(std::array<int, 2>* in_img_shape, 
//   BinningRatioType in_scale) {
void IMAGEPROCESSOR::Initialize(std::array<int, 2>* in_img_shape) 
{
    img_shape = *in_img_shape;      // 保存原始图像尺寸
    
    // 在线图像配置参数
    uint16_t img_width = static_cast<uint16_t>(img_shape[0]);
    uint16_t img_height = static_cast<uint16_t>(img_shape[1]);
    format_online = SSNE_YUV422_16;

    OnlineSetOutputImage(kPipeline0, format_online, img_width, img_height);
    
    // 打开pipe0（裁剪图像通道）
    int res0 = OpenOnlinePipeline(kPipeline0);
    if (res0 != 0) {
        printf("[ERROR] Failed to open online pipeline!\n");
        printf("ret: %d\n", res0);
        return;
    }
    printf("[INFO] open online pipe0: %d \n", res0);
}

/**
 * @brief 从pipeline获取 1920×1080 图像
 * @param img_sensor 输出参数：存储从pipe0获取的显示图像
 */
void IMAGEPROCESSOR::GetImage(ssne_tensor_t* img_sensor) {
    int capture_code = -1;  // pipe0采集返回码

    // 从pipe0获取 1920×1080 在线图像数据
    capture_code = GetImageData(img_sensor, kPipeline0, kSensor0, 0);
    
    // 检查pipe0采集是否成功
    if (capture_code != 0)
    {   
        printf("[IMAGEPROCESSOR] Get Invalid Image from kPipeline0!\n");
    }
}

/**
 * @brief 释放图像处理器资源，关闭pipeline
 */
void IMAGEPROCESSOR::Release()
{
    CloseOnlinePipeline(kPipeline0);  // 关闭pipe0（裁剪图像通道）
    printf("[INFO] OnlinePipe closed!\n");
}
