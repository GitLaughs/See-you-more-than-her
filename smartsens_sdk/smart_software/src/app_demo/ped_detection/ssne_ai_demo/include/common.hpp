/*
 * @Filename: common.hpp
 * @Author: Twyla Tu
 * @Email: qian.tu@smartsenstech.com
 * @Date: 2024-08-08 15-05-38
 * @Copyright (c) 2024 SmartSens
 */
#pragma once

#include <stdio.h>
#include <vector>
#include <array>
#include <string>
#include <math.h>
#include "smartsoc/ssne_api.h"
#include <chrono>


/*! @brief 点的结构体定义。
 */
struct Point {
  float x;
  float y;

  // 构造函数
  Point(float x, float y) : x(x), y(y) {}
};

/*! @brief 人脸检测结果的结构体定义。
 */
struct DetectionResult {
  /** \brief All the detected object boxes for an input image.
   * The size of `boxes` is the number of detected objects, and the element of `boxes` 
   * is a array of 4 float values, means [xmin, ymin, xmax, ymax].
   */
  std::vector<std::array<float, 4>> boxes;
  /** \brief
   * If the model detect obj with landmarks, every detected object box correspoing to 
   * a landmark, which is a array of 2 float values, means location [x,y].
  */
  std::vector<std::array<float, 2>> landmarks;
  /** \brief
   * Indicates the confidence of all targets detected from a single image, and the number 
   * of elements is consistent with boxes.size().
   */
  std::vector<float> scores;
  /** \brief
   * `landmarks_per_obj` indicates the number of obj landmarks for each detected obj
   * if the model's output contains obj landmarks (such as YOLOv5, SCRFD, ...)
  */
  int landmarks_per_obj;

  DetectionResult() { landmarks_per_obj = 0; }
  DetectionResult(const DetectionResult& res);
  // 清空DetectionResult内的所有变量
  void Clear();

  // 清空DetectionResult，释放内存
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
      * \param[in] in_det_shape 检测图像尺寸(w, h)。
      * \param[in] in_scale 输入图像和检测图像之间的尺度倍数。
      * \return none
      */
    void Initialize(std::array<int, 2>* in_img_shape, std::array<int, 2>* in_det_shape, 
                    float* in_scale);
    /**
     * 获取offline或者online的图像。
     * 
     * \param[in] img_in // 输入图像, 3-D array with layout HWC, YUV422_20bit format。
     * \param[in] img_out // 输出图像, 3-D array with layout HWC, SSNE_Y_8 format。
    */
    // void GetImage(SSAIRT::SSNE_Tensor* img_in, SSAIRT::SSNE_Tensor* img_out);
    void GetImage(ssne_tensor_t* img_out);
    
    /*
     * 对检测坐标进行后处理，还原缩放和padding导致的坐标变化。
    */
    void ProcessDetections(DetectionResult* result);

    // 释放资源
    void Release();

    // 前处理时，模型推理输入的原始待检测图像尺寸，（width，height）
    std::array<int, 2> img_shape;
    // 前处理时，模型推理需要的待检测图像尺寸，（width，height）
    std::array<int, 2> det_shape;

    // 计时
    std::vector<std::chrono::duration<double, std::milli>> durations_preprocess;
  
  private:
    // offline setting
    // AiPreprocessPipe pipe_offline = GetAIPreprocessPipe();
    // online setting
    uint16_t online_width;
    uint16_t online_height;
    uint8_t format_online;
    // 后处理时，检测的缩放尺度
    float det_scale;
    // 在y方向padding的尺寸
    uint16_t pad_height;
};

class MobileNetV2 {
  public:
    std::string ModelName() const { return "MobileNetV2"; }
    void Predict(ssne_tensor_t input[], ssne_tensor_t output[]);
    void Initialize(std::string& model_path);
    void Release();
    
  private:
    // 推理用的模型
    uint16_t model_id = 0;
   
};

class YOLOV7 {
  public:
    std::string ModelName() const { return "yolov7"; }

    /** \brief 输入单张图像，预测人脸检测框的位置。
     *
     * \param[in] img // 输入图像, 3-D array with layout HWC, BGR format。
     * \param[in] result 模型输出结果, 结构体类型。
     * \param[in] conf_threshold 后处理的置信度阈值，默认是0.25。
     * \return none
     */
    void Predict(ssne_tensor_t input[], ssne_tensor_t output[]);
    // DetectionResult* result, float conf_threshold = 0.25f);

    /** \brief 人脸检测模型初始化。
      *
      * \param[in] model_path onnx模型路径，字符串类型。
      * \param[in] in_img_shape 输入图像尺寸(w, h)。
      * \param[in] in_det_shape 检测图像尺寸(w, h)。
      * \param[in] in_use_kps 模型是否能否输出人脸关键点。
      * \param[in] in_scale 输入图像和检测图像之间的尺度倍数。
      * \param[in] in_box_len 模型输出bbox的个数，提前为tensorrt预留，内存初始化所用。
      * \return none
      */
    // void Initialize(std::string& model_path, std::array<int, 2>* in_img_shape, 
    //                     std::array<int, 2>* in_det_shape, float* in_scale);
    void Initialize(std::string& model_path);
  
    // 后处理时，nms阈值
    // float nms_threshold;
    // 后处理时，做完nms之后最多保存的box个数
    // int keep_top_k;
    // 后处理时，做nms之前最多保存的box个数
    // int top_k;
    // 后处理时，检测的缩放尺度
    // float det_scale;

    // 前处理时，模型推理输入的原始待检测图像尺寸，（width，height）
    // std::array<int, 2> img_shape;
    // 前处理时，模型推理需要的待检测图像尺寸，（width，height）
    // std::array<int, 2> det_shape;
    // 模型输出bbox的个数
    // int box_len;
    // 宽度缩放尺度
    // float w_scale;
    // 高度缩放尺度
    // float h_scale;

    // 后处理时，onnx的输出是否包含关键点信息
    // bool use_kps;

    // 后处理时，cfg所包含的每个stage的图像尺寸要求
    // std::vector<std::vector<std::array<int, 2>>> min_sizes_yolo;
    // std::vector<std::array<int, 2>> min_sizes;
    // 后处理时，cfg所包含的下采样的步长（8，16，32）
    // std::vector<int> steps;
    // 后处理时，cfg所包含的variance
    // std::vector<float> variance;
    // 后处理时，cfg所包含的clip
    // bool clip = false;
    // 后处理时，cfg所包含的ratios
    // std::vector<float> ratios;
    // 释放资源
    void Release();
    
    // 计时
    std::vector<std::chrono::duration<double, std::milli>> durations_forward;
    // std::vector<std::chrono::duration<double, std::milli>> durations_postprocess;

  private:
    // 推理用的模型
    uint16_t model_id = 0;
    // ssne_input_t inputs[1];
    // 输出顺序：bboxes，scores
    // ssne_tensor_t output[3];

    // 模型的锚点框
    // std::vector<std::array<float, 4>> anchors;
    /* 根据锚点框，产生各个尺度下的所有预定义检测框 */
    // void GenerateBoxes();
    /* 根据检测结果，对检测框进行坐标换算 */
    // void DecodeBoxes(std::vector<std::array<float, 4>>& boxes);
    /* 检测结果后处理 */
    // void Postprocess(DetectionResult* result, float* conf_threshold);
};
