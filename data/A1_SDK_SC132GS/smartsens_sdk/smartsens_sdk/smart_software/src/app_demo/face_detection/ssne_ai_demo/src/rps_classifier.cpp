/*
 * @Filename: rps_classifier.cpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-01-20
 * @Copyright (c) 2025 SmartSens
 * @Description: 石头剪刀布分类器实现文件
 */
#include <assert.h>
#include "../include/utils.hpp"
#include <iostream>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * @brief 初始化石头剪刀布分类器
 */
void RPS_CLASSIFIER::Initialize(std::string& model_path, std::array<int, 2>* in_img_shape,
                                std::array<int, 2>* in_cls_shape) {
    img_shape = *in_img_shape;
    cls_shape = *in_cls_shape;

    // 加载模型
    char* model_path_char = const_cast<char*>(model_path.c_str());
    model_id = ssne_loadmodel(model_path_char, SSNE_STATIC_ALLOC);

    // 创建模型输入tensor (RGB三通道)
    uint32_t cls_width = static_cast<uint32_t>(cls_shape[0]);
    uint32_t cls_height = static_cast<uint32_t>(cls_shape[1]);
    inputs[0] = create_tensor(cls_width, cls_height, SSNE_RGB, SSNE_BUF_AI);

    // 设置预处理管道：按实际online输出取中心正方形，输出尺寸由inputs[0]决定（320×320）
    int crop_size = std::min(img_shape[0], img_shape[1]);
    int crop_x0 = (img_shape[0] - crop_size) / 2;
    int crop_y0 = (img_shape[1] - crop_size) / 2;
    SetCrop(pipe_offline, crop_x0, crop_y0, crop_x0 + crop_size, crop_y0 + crop_size);
    printf("[INFO] RPS classifier crop [%d, %d, %d, %d] from image shape [%d, %d]\n",
           crop_x0, crop_y0, crop_x0 + crop_size, crop_y0 + crop_size, img_shape[0], img_shape[1]);

    // 设置归一化参数（从模型自动获取）
    SetNormalize(pipe_offline, model_id);

    printf("[INFO] RPS classifier initialized with input shape [%d, %d]\n", cls_shape[0], cls_shape[1]);
}

/**
 * @brief 执行石头剪刀布分类预测
 */
void RPS_CLASSIFIER::Predict(ssne_tensor_t* img, std::string& out_label, float& out_score, float out_scores[3]) {
    // 运行AI预处理管道（crop + resize）
    int ret = RunAiPreprocessPipe(pipe_offline, *img, inputs[0]);
    if (ret != 0) {
        printf("[ERROR] Failed to run AI preprocess pipe for classification!\n");
        printf("ret: %d\n", ret);
        out_label = "Error";
        out_score = 0.0f;
        if (out_scores) {
            out_scores[0] = out_scores[1] = out_scores[2] = 0.0f;
        }
        return;
    }

    int dtype = -1;
    ssne_get_model_input_dtype(model_id, &dtype);
    set_data_type(inputs[0], dtype);

    // 前向推理：在NPU上执行模型推理
    if (ssne_inference(model_id, 1, inputs)) {
        fprintf(stderr, "ssne inference fail!\n");
        out_label = "Error";
        out_score = 0.0f;
        if (out_scores) {
            out_scores[0] = out_scores[1] = out_scores[2] = 0.0f;
        }
        return;
    }

    // 获取模型输出（3个类别的得分）
    ssne_getoutput(model_id, 1, outputs);

    float* data = (float*)get_data(outputs[0]);

    // 输出为3个float值
    float scores[3] = {data[0], data[1], data[2]};
    if (out_scores) {
        out_scores[0] = scores[0];
        out_scores[1] = scores[1];
        out_scores[2] = scores[2];
    }
    // 找最大值
    int max_idx = 0;
    float max_score = scores[0];
    for (int i = 1; i < 3; i++) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            max_idx = i;
        }
    }

    const char* labels[] = {"P", "R", "S"};
    if (max_score > 0.6f) {
        out_label = labels[max_idx];
        out_score = max_score;
    } else {
        out_label = "NoTarget";
        out_score = max_score;
    }
}

/**
 * @brief 释放分类器资源
 */
void RPS_CLASSIFIER::Release() {
    // 释放输入tensor
    release_tensor(inputs[0]);
    // 释放输出tensor
    release_tensor(outputs[0]);
    // 释放预处理管道
    ReleaseAIPreprocessPipe(pipe_offline);
}
