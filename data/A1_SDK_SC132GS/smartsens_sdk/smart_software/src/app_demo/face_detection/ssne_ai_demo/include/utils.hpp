/**
 * utils.hpp — 可视化器（VISUALIZER）
 *
 * 封装 OSD 图层操作的高级接口，提供：
 *   - DrawFixedSquare：绘制固定正方形（Layer 1）
 *   - DrawBitmap：绘制位图纹理（Layer 2）
 *   - ClearLayer：清除指定图层内容
 * 位图路径自动以 /app_demo/app_assets/ 为前缀。
 */

#pragma once

#include "osd-device.hpp"
#include <algorithm>

class VISUALIZER {
  public:
    bool Initialize(std::array<int, 2>& in_img_shape, const std::string& bitmap_lut_path = "");
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
    /**
     * @brief 清空指定OSD图层内容
     * @param layer_id 要清空的图层ID
     * @description 清空指定图层上的所有绘制内容，但保留图层本身
     */
    void ClearLayer(int layer_id = 2);
  private:
    // OSD设备实例
    sst::device::osd::OsdDevice osd_device;
    int m_width;   // 画面宽度
    int m_height;  // 画面高度
    std::string m_bitmap_lut_path_full;  // 保存完整的位图LUT路径，确保生命周期
};
