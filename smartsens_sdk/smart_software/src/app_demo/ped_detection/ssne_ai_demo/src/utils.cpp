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


/**
 * @brief OSD可视化器初始化函数
 * @param in_img_shape 图像尺寸 [宽度, 高度]
 * @description 初始化OSD设备
 */
void VISUALIZER::Initialize(std::array<int, 2>& in_img_shape) {
    // 初始化OSD设备，配置图像宽度和高度
    osd_device.Initialize(in_img_shape[0], in_img_shape[1]);
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
 * @description 将所有检测到的人脸框绘制到OSD显示层
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
        
        quad_rangle_vec.emplace_back(q);     // 添加到矩形框向量
    }

    // 调用OSD设备绘制所有矩形框到显示层
    osd_device.Draw(quad_rangle_vec);
}

/**
 * @brief 释放OSD可视化器资源
 * @description 清理OSD设备占用的资源
 */
void VISUALIZER::Release() {
    osd_device.Release();  // 释放OSD设备资源
}

