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

/*! @brief 人脸检测结果的结构体定义。
 */
struct FaceDetectionResult {
  /** \brief All the detected object boxes for an input image.
   * The size of `boxes` is the number of detected objects, and the element of `boxes` 
   * is a array of 4 float values, means [xmin, ymin, xmax, ymax].
   */
  std::vector<std::array<float, 4>> boxes;
  /** \brief
   * If the model detect face with landmarks, every detected object box correspoing to 
   * a landmark, which is a array of 2 float values, means location [x,y].
  */
  std::vector<std::array<float, 2>> landmarks;
  /** \brief
   * Indicates the confidence of all targets detected from a single image, and the number 
   * of elements is consistent with boxes.size().
   */
  std::vector<float> scores;
  /** \brief
   * `landmarks_per_face` indicates the number of face landmarks for each detected face
   * if the model's output contains face landmarks (such as YOLOv5Face, SCRFD, ...)
  */
  int landmarks_per_face;

  FaceDetectionResult() { landmarks_per_face = 0; }
  FaceDetectionResult(const FaceDetectionResult& res);
  // 清空FaceDetectionResult内的所有变量
  void Clear();

  // 清空FaceDetectionResult，释放内存
  void Free();
  
  // 提前为结构体保留一定的空间 
  void Reserve(int size);
  
  // 修改结构体大小，取前size个元素
  void Resize(int size);
};

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
    
    /*
     * 对检测坐标进行后处理，还原缩放和padding导致的坐标变化。
    */
    // void ProcessDetections(FaceDetectionResult* result);

    // 释放资源
    void Release();

    // 前处理时，模型推理输入的原始待检测图像尺寸，（width，height）
    std::array<int, 2> img_shape;
  
  private:
    // online setting
    uint8_t format_online;
};

class SCRFDGRAY {
  public:
    std::string ModelName() const { return "scrfd_gray"; }

    /** \brief 输入单张图像，预测人脸检测框的位置。
     *
     * \param[in] img_in // 输入图像, 3-D array with layout HWC, BGR format。
     * \param[in] result 模型输出结果, 结构体类型。
     * \param[in] conf_threshold 后处理的置信度阈值，默认是0.25。
     * \return none
     */
    void Predict(ssne_tensor_t* img_in, FaceDetectionResult* result, float conf_threshold = 0.25f);

    /** \brief 人脸检测模型初始化。
      *
      * \param[in] model_path onnx模型路径，字符串类型。
      * \param[in] in_img_shape 输入图像尺寸(w, h)。
      * \param[in] in_det_shape 检测图像尺寸(w, h)。
      * \param[in] in_use_kps 模型是否能否输出人脸关键点。
      * \param[in] in_box_len 模型输出bbox的个数，提前为tensorrt预留，内存初始化所用。
      * \return none
      */
    void Initialize(std::string& model_path, std::array<int, 2>* in_img_shape, 
                    std::array<int, 2>* in_det_shape, bool in_use_kps,
                    int in_box_len);
  
    // 后处理时，nms阈值
    float nms_threshold;
    // 后处理时，做完nms之后最多保存的box个数
    int keep_top_k;
    // 后处理时，做nms之前最多保存的box个数
    int top_k;

    // 前处理时，模型推理输入的原始待检测图像尺寸，（width，height）
    std::array<int, 2> img_shape;
    // 前处理时，模型推理需要的待检测图像尺寸，（width，height）
    std::array<int, 2> det_shape;
    // 模型输出bbox的个数
    int box_len;
    // 宽度缩放尺度
    float w_scale;
    // 高度缩放尺度
    float h_scale;

    // 后处理时，onnx的输出是否包含关键点信息
    bool use_kps;

    // 后处理时，cfg所包含的每个stage的图像尺寸要求
    std::vector<std::array<int, 2>> min_sizes;
    // 后处理时，cfg所包含的下采样的步长（8，16，32）
    std::vector<int> steps;
    // 后处理时，cfg所包含的variance
    std::vector<float> variance;
    // 后处理时，cfg所包含的clip
    bool clip = false;
    // 后处理时，cfg所包含的ratios
    std::vector<float> ratios;
    // 释放资源
    void Release();

    //debug
    void saveImageBin(const void* data, int w, int h, const char* filename);
    void saveFloatBin(const float* data, int length, const char* filename);

  private:
    // 推理用的模型
    uint16_t model_id = 0;
    ssne_tensor_t inputs[1];
    // 输出顺序：bboxes，scores
    ssne_tensor_t outputs[6];
    // offline setting
    AiPreprocessPipe pipe_offline = GetAIPreprocessPipe();

    // 模型的锚点框
    std::vector<std::array<float, 4>> anchors;
    /* 根据锚点框，产生各个尺度下的所有预定义检测框 */
    void GenerateBoxes();
    /* 根据检测结果，对检测框进行坐标换算 */
    void DecodeBoxes(std::vector<std::array<float, 4>>& boxes);
    /* 检测结果后处理 */
    void Postprocess(std::vector<std::array<float, 4>>* boxes, std::vector<float>* scores, 
                     FaceDetectionResult* result, float* conf_threshold);
};
