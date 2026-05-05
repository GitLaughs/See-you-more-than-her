/**
 * rps_classifier.hpp — 5 分类视觉导航分类器接口
 *
 * 输入：720×1280 Y8 灰度图像（摄像头原始帧）
 * 预处理：中心裁剪至 320×320，模型归一化
 * 输出：5 个类别得分（float32），取 argmax
 * 类别顺序：person / stop / forward / obstacle / NoTarget
 * 阈值：置信度 < 0.6 时归为 NoTarget
 */

#pragma once

#include "common.hpp"

class RPS_CLASSIFIER {
  public:
    std::string ModelName() const { return "rps_classifier"; }

    bool Initialize(std::string& model_path, std::array<int, 2>* in_img_shape,
                    std::array<int, 2>* in_cls_shape);

    void Predict(ssne_tensor_t* img_in, std::string& out_label, float& out_score,
                 float out_scores[5] = nullptr);

    void Release();

  private:
    uint16_t model_id = 0;
    ssne_tensor_t inputs[1]{};
    ssne_tensor_t outputs[1]{};
    AiPreprocessPipe pipe_offline = GetAIPreprocessPipe();
    std::array<int, 2> img_shape;
    std::array<int, 2> cls_shape;
};
