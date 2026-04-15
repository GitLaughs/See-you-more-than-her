/*
 * @Filename: utils.hpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#pragma once

#include "osd-device.hpp"
#include <algorithm>

namespace utils {
  // 人脸检测模型所需的函数
  /* 合并两段结果 */
  void Merge(FaceDetectionResult* result, size_t low, size_t mid, size_t high);
  /* 归并排序算法 */
  void MergeSort(FaceDetectionResult* result, size_t low, size_t high);
  /* 对检测结果进行排序 */
  void SortDetectionResult(FaceDetectionResult* result);
  /* 非极大值抑制 */
  void NMS(FaceDetectionResult* result, float iou_threshold, int top_k);
} // namespace utils

class VISUALIZER {
  public:
    void Initialize(std::array<int, 2>& in_img_shape, const std::string& bitmap_lut_path = "");
    void Release();
    void Draw();
    void Draw(const std::vector<std::array<float, 4>>& boxes);
    // 检测框使用的图层ID（默认layer 0）
    static const int DETECTION_LAYER_ID = 0;
    /**
     * @brief 绘制固定实心正方形
     * @param x_min 左上角X坐标（绝对坐标，左上角为原点）
     * @param y_min 左上角Y坐标（绝对坐标，左上角为原点）
     * @param x_max 右下角X坐标（绝对坐标，左上角为原点）
     * @param y_max 右下角Y坐标（绝对坐标，左上角为原点）
     * @param layer_id 使用的OSD layer ID（建议使用1，避免与检测框冲突）
     * @description 绘制一次后，正方形会一直显示，不随帧数消失。坐标系统以画面左上角为原点(0,0)，X向右为正，Y向下为正。
     */
    void DrawFixedSquare(int x_min, int y_min, int x_max, int y_max, int layer_id = 1);
    /**
     * @brief 绘制位图
     * @param bitmap_path 位图文件路径（相对于app_assets目录）
     * @param lut_path LUT文件路径（相对于app_assets目录，如果为空则使用默认LUT）
     * @param pos_x 位图左上角X坐标（绝对坐标，左上角为原点）
     * @param pos_y 位图左上角Y坐标（绝对坐标，左上角为原点）
     * @param layer_id 使用的OSD layer ID（建议使用2，避免与其他图层冲突）
     * @description 绘制一次后，位图会一直显示，不随帧数消失。坐标系统以画面左上角为原点(0,0)，X向右为正，Y向下为正。
     */
    void DrawBitmap(const std::string& bitmap_path, const std::string& lut_path = "",
                    int pos_x = 0, int pos_y = 0, int layer_id = 2);

  private:
    // OSD设备实例
    sst::device::osd::OsdDevice osd_device;
    int m_width;   // 画面宽度
    int m_height;  // 画面高度
    std::string m_bitmap_lut_path_full;  // 保存完整的位图LUT路径，确保生命周期
};
