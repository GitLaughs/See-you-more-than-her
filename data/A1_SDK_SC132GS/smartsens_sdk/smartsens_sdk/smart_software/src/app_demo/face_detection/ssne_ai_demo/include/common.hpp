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
    void Initialize(std::array<int, 2>* in_img_shape);
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


/**
 * @brief 石头剪刀布分类器类
 * @description 基于model_rps.m1model的分类器，支持RGB三通道输入
 */
class RPS_CLASSIFIER {
  public:
    std::string ModelName() const { return "rps_classifier"; }

    /** \brief 输入单张图像，预测石头剪刀布分类结果。
     *
     * \param[in] img_in // 输入图像, RGB format。
     * \param[out] out_label 分类结果标签 (P/R/S/NoTarget)。
     * \param[out] out_score 分类置信度得分。
     * \param[out] out_scores 3个类别的原始得分 [P_score, R_score, S_score]。
     * \return none
     */
    void Predict(ssne_tensor_t* img_in, std::string& out_label, float& out_score, float out_scores[3] = nullptr);

    /** \brief 分类模型初始化。
      *
      * \param[in] model_path 模型路径，字符串类型。
      * \param[in] in_img_shape 输入图像尺寸(w, h)。
      * \param[in] in_cls_shape 分类模型输入尺寸(w, h)。
      * \return none
      */
    void Initialize(std::string& model_path, std::array<int, 2>* in_img_shape,
                    std::array<int, 2>* in_cls_shape);

    // 释放资源
    void Release();

  private:
    // 推理用的模型
    uint16_t model_id = 0;
    ssne_tensor_t inputs[1];
    ssne_tensor_t outputs[1];
    // offline setting
    AiPreprocessPipe pipe_offline = GetAIPreprocessPipe();

    // 前处理时，原始图像尺寸，（width，height）
    std::array<int, 2> img_shape;
    // 前处理时，模型推理需要的分类图像尺寸，（width，height）
    std::array<int, 2> cls_shape;
};
