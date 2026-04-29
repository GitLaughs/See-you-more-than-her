/*
 * @Filename: utils.cpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#include "../include/utils.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include "log.hpp"


/**
 * @brief OSD可视化器初始化函数
 * @param in_img_shape 图像尺寸 [宽度, 高度]
 * @description 初始化OSD设备
 */
void VISUALIZER::Initialize(std::array<int, 2>& in_img_shape, const std::string& bitmap_lut_path) {
    // 初始化OSD设备，配置图像宽度和高度
    m_width = in_img_shape[0];
    m_height = in_img_shape[1];

    // 如果提供了位图LUT路径，在初始化时加载
    // 位图LUT和默认LUT都在app_assets目录下，使用相同的路径格式
    // 注意：需要将完整路径保存为成员变量，确保生命周期
    const char* lut_path = nullptr;
    if (!bitmap_lut_path.empty()) {
        m_bitmap_lut_path_full = "/app_demo/app_assets/" + bitmap_lut_path;
        lut_path = m_bitmap_lut_path_full.c_str();
    }
    osd_device.Initialize(m_width, m_height, lut_path);
}


/**
 * @brief 绘制测试矩形框（用于测试OSD功能）
 * @description 在OSD上绘制一个固定位置的测试矩形框
 */
void VISUALIZER::Draw() {
    std::vector<sst::device::osd::OsdQuadRangle> quad_rangle_vec;

	sst::device::osd::OsdQuadRangle q;

	// 配置测试矩形框参数
	q.color = 0;                         // 颜色索引0
	q.box = {100, 100, 200, 200};        // 矩形框坐标 [xmin, ymin, xmax, ymax]
	q.border = 3;                        // 边框宽度3像素
	q.alpha = fdevice::TYPE_ALPHA75;     // 透明度75%
	q.type = fdevice::TYPE_HOLLOW;       // 空心矩形
	quad_rangle_vec.emplace_back(q);


    // 调用OSD设备绘制测试矩形框
    osd_device.Draw(quad_rangle_vec);
}

/**
 * @brief 根据检测框绘制OSD矩形
 * @param boxes 检测框向量，每个元素为[xmin, ymin, xmax, ymax]
 * @description 将所有检测到的人脸框绘制到OSD显示层（使用layer 0，不影响固定正方形所在的layer 1）
 */
void VISUALIZER::Draw(const std::vector<std::array<float, 4>>& boxes) {
    printf("Drawing %zu detection boxes\n", boxes.size());

    std::vector<sst::device::osd::OsdQuadRangle> quad_rangle_vec;  // OSD矩形框向量

    // 遍历所有检测框，转换为OSD矩形格式
    for (size_t i = 0; i < boxes.size(); i++) {
        sst::device::osd::OsdQuadRangle q;

        // 将检测框坐标从float转换为int [xmin, ymin, xmax, ymax]
        int xmin = static_cast<int>(boxes[i][0]);  // 左上角x坐标
        int ymin = static_cast<int>(boxes[i][1]);  // 左上角y坐标
        int xmax = static_cast<int>(boxes[i][2]);  // 右下角x坐标
        int ymax = static_cast<int>(boxes[i][3]);  // 右下角y坐标

        q.box = {xmin, ymin, xmax, ymax};  // 设置矩形框坐标

        // 设置矩形框样式参数
        q.color = 1;                         // 颜色索引1（不同于测试框）
        q.border = 3;                        // 边框宽度3像素
        q.alpha = fdevice::TYPE_ALPHA75;     // 透明度75%
        q.type = fdevice::TYPE_HOLLOW;       // 空心矩形
        q.layer_id = DETECTION_LAYER_ID;     // 使用layer 0，避免影响固定正方形的layer 1

        quad_rangle_vec.emplace_back(q);     // 添加到矩形框向量
    }

    // 调用OSD设备绘制所有矩形框到指定图层（layer 0）
    // 使用指定图层版本，避免清除所有图层（包括layer 1的固定正方形）
    osd_device.Draw(quad_rangle_vec, DETECTION_LAYER_ID);
}

/**
 * @brief 绘制固定实心正方形
 * @param x_min 左上角X坐标（绝对坐标，左上角为原点）
 * @param y_min 左上角Y坐标（绝对坐标，左上角为原点）
 * @param x_max 右下角X坐标（绝对坐标，左上角为原点）
 * @param y_max 右下角Y坐标（绝对坐标，左上角为原点）
 * @param layer_id 使用的OSD layer ID（建议使用1，避免与检测框冲突）
 * @description 绘制一次后，正方形会一直显示，不随帧数消失。坐标系统以画面左上角为原点(0,0)，X向右为正，Y向下为正。
 */
void VISUALIZER::DrawFixedSquare(int x_min, int y_min, int x_max, int y_max, int layer_id) {
    // 确保坐标顺序正确（xmin < xmax, ymin < ymax）
    int abs_x_min = x_min;
    int abs_y_min = y_min;
    int abs_x_max = x_max;
    int abs_y_max = y_max;

    if (abs_x_min > abs_x_max) std::swap(abs_x_min, abs_x_max);
    if (abs_y_min > abs_y_max) std::swap(abs_y_min, abs_y_max);

    // 确保坐标在画面范围内
    abs_x_min = std::max(0, std::min(abs_x_min, m_width - 1));
    abs_y_min = std::max(0, std::min(abs_y_min, m_height - 1));
    abs_x_max = std::max(0, std::min(abs_x_max, m_width - 1));
    abs_y_max = std::max(0, std::min(abs_y_max, m_height - 1));

    // 创建正方形框
    std::vector<std::array<float, 4>> square_box;
    square_box.push_back({static_cast<float>(abs_x_min),
                         static_cast<float>(abs_y_min),
                         static_cast<float>(abs_x_max),
                         static_cast<float>(abs_y_max)});

    // 使用指定的layer_id绘制正方形
    osd_device.Draw(square_box,
                    0,                              // 边框宽度0（实心矩形不需要边框）
                    layer_id,                       // 使用指定的layer_id
                    fdevice::TYPE_SOLID,            // 实心矩形
                    fdevice::TYPE_ALPHA100,        // 完全不透明
                    2);                             // 颜色索引2（可根据需要修改）

    std::cout << "[VISUALIZER] Fixed square drawn: (" << abs_x_min << ", " << abs_y_min
              << ") to (" << abs_x_max << ", " << abs_y_max << "), layer_id=" << layer_id << std::endl;
}

/**
 * @brief 绘制位图
 * @param bitmap_path 位图文件路径（相对于app_assets目录）
 * @param lut_path LUT文件路径（相对于app_assets目录，如果为空则使用默认LUT）
 * @param pos_x 位图左上角X坐标（绝对坐标，左上角为原点）
 * @param pos_y 位图左上角Y坐标（绝对坐标，左上角为原点）
 * @param layer_id 使用的OSD layer ID（建议使用2，避免与其他图层冲突）
 * @description 绘制一次后，位图会一直显示，不随帧数消失。坐标系统以画面左上角为原点(0,0)，X向右为正，Y向下为正。
 */
void VISUALIZER::DrawBitmap(const std::string& bitmap_path, const std::string& lut_path,
                            int pos_x, int pos_y, int layer_id) {
    // 构建完整路径
    std::string full_bitmap_path = "/app_demo/app_assets/" + bitmap_path;

    // 构建LUT完整路径（如果提供了）
    // 注意：LUT在初始化时已加载，这里只是用于日志记录
    const char* full_lut_path = nullptr;
    std::string lut_full_path;
    if (!lut_path.empty()) {
        lut_full_path = "/app_demo/app_assets/" + lut_path;
        full_lut_path = lut_full_path.c_str();
    }

    LOG_DEBUG("[VISUALIZER] Drawing bitmap: %s", full_bitmap_path);
    LOG_DEBUG(" at position (%d, %d), layer_id=%d\n", pos_x, pos_y, layer_id);

    // 调用OSD设备绘制位图（传入绝对坐标）
    osd_device.DrawTexture(full_bitmap_path.c_str(), full_lut_path, layer_id, pos_x, pos_y);
}

/**
 * @brief 释放OSD可视化器资源
 * @description 清理OSD设备占用的资源
 */
void VISUALIZER::Release() {
    osd_device.Release();  // 释放OSD设备资源
}

