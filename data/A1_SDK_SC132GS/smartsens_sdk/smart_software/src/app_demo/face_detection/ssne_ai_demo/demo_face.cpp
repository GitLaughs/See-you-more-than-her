/*
 * @Filename: demo_face.cpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#include <fstream>
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <fcntl.h>
#include <regex>
#include <dirent.h>
#include <unistd.h>
#include "include/utils.hpp"
#include "include/chassis_controller.hpp"

using namespace std;

// 全局退出标志（线程安全）
bool g_exit_flag = false;
// 保护退出标志的互斥锁
std::mutex g_mtx;

/**
 * @brief 键盘监听程序，用于结束demo
 */
void keyboard_listener() {
    std::string input;
    std::cout << "键盘监听线程已启动，输入 'q' 退出程序..." << std::endl;

    while (true) {
        // 读取键盘输入（会阻塞直到有输入）
        std::cin >> input;

        // 加锁修改退出标志
        std::lock_guard<std::mutex> lock(g_mtx);
        if (input == "q" || input == "Q") {
            g_exit_flag = true;
            std::cout << "检测到退出指令，通知主线程退出..." << std::endl;
            break;
        } else {
            std::cout << "输入无效（仅 'q' 有效），请重新输入：" << std::endl;
        }
    }
}

/**
 * @brief 检查退出标志的辅助函数（线程安全）
 * @return 是否需要退出
 */
bool check_exit_flag() {
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_exit_flag;
}

/**
 * @brief 人脸检测演示程序主函数
 * @return 执行结果，0表示成功
 */
int main() {
    /******************************************************************************************
     * 1. 参数配置
     ******************************************************************************************/
    
    // 图像尺寸配置
    // 传感器采集: 1280×720 (16:9 Y8 灰度)
    int img_width = 1280;   // 输入图像宽度
    int img_height = 720;   // 输入图像高度
    
    // 模型配置参数
    // 推理输入: 640×360（由 RunAiPreprocessPipe 从 1280×720 缩放，废弃旧裁剪方案）
    array<int, 2> det_shape = {640, 360};  // 检测模型输入尺寸 (16:9)
    string path_det = "/app_demo/app_assets/models/face_640x360.m1model";  // 人脸检测模型路径
    
    /******************************************************************************************
     * 2. 系统初始化
     ******************************************************************************************/
    
    // SSNE初始化
    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
    }
    
    // 图像处理器初始化
    array<int, 2> img_shape = {img_width, img_height};  // 原始图像尺寸 1280×720
    // 废弃旧裁剪参数 crop_shape / crop_offset_y
    // 新方案: sensor 1280×720 → RunAiPreprocessPipe 缩放 → 640×360 推理
    // img_shape 同时作为检测器坐标系参考（使 w_scale=2.0, h_scale=2.0）
    
    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape);  // 初始化图像处理器（全分辨率 1280×720）
    
    // 人脸检测模型初始化
    SCRFDGRAY detector;
    int box_len = det_shape[0] * det_shape[1] / 512 * 21;  // 计算最大检测框数量
    // in_img_shape 传入原图 1280×720，使检测器坐标系与显示分辨率一致（Postprocess 中 scale=2.0）
    detector.Initialize(path_det, &img_shape, &det_shape, false, box_len);  // 初始化检测器
    
    // 人脸检测结果初始化
    FaceDetectionResult* det_result = new FaceDetectionResult;
    
    // OSD可视化器初始化（用于绘制检测框）
    VISUALIZER visualizer;
    visualizer.Initialize(img_shape);  // 初始化可视化器（配置图像尺寸）

    // 底盘控制器初始化（A1 UART TX0/RX0 ↔ STM32 UART3，115200 baud）
    ChassisController chassis;
    bool chassis_ok = chassis.Init();
    if (chassis_ok) {
        printf("[INFO] 底盘控制器初始化成功 (UART 115200)\n");
    } else {
        fprintf(stderr, "[WARN] 底盘控制器初始化失败，底盘控制不可用\n");
    }

    // 系统稳定等待
    cout << "sleep for 0.2 second!" << endl;
    sleep(0.2);  // 等待系统稳定
    
    uint16_t num_frames = 0;  // 帧计数器
    ssne_tensor_t img_sensor;  // 图像tensor定义

    // 创建键盘监听线程
    std::thread listener_thread(keyboard_listener);
    
    /******************************************************************************************
     * 3. 主处理循环
     ******************************************************************************************/
    //循环50000帧后推出，循环次数可以修改，也可以改成while(true)
    while (!check_exit_flag()) {
        
        // 从sensor获取图像（裁剪图）
        processor.GetImage(&img_sensor);
        
        // 人脸检测模型推理（置信度阈值0.4）
        detector.Predict(&img_sensor, det_result, 0.4f);
        
        /**********************************************************************************
         * 3.1 判断是否有检测到人脸
         **********************************************************************************/
        if (det_result->boxes.size() > 0) {
            /**********************************************************************************
             * 3.2 坐标转换：将crop图坐标转换为原图坐标
             **********************************************************************************/
            std::vector<std::array<float, 4>> boxes_original_coord;  // 存储转换后的原图坐标
            
            // 遍历所有检测框进行坐标转换
            for (size_t i = 0; i < det_result->boxes.size(); i++) {
                // 检测器已经通过 w_scale=2.0/h_scale=2.0 将坐标映射到 1280×720 空间
                // 不再需要 crop_offset_y 偏移（废弃旧裁剪方案）
                float x1_orig = det_result->boxes[i][0];
                float y1_orig = det_result->boxes[i][1];
                float x2_orig = det_result->boxes[i][2];
                float y2_orig = det_result->boxes[i][3];
                
                // 保存原图坐标用于OSD绘制
                boxes_original_coord.push_back({x1_orig, y1_orig, x2_orig, y2_orig});
            }
            
            /**********************************************************************************
             * 3.3 OSD绘图：使用原图坐标在OSD上绘制人脸检测框
             **********************************************************************************/
            visualizer.Draw(boxes_original_coord);

            /**********************************************************************************
             * 3.4 底盘控制：检测到人脸 → 以 100 mm/s 前进
             **********************************************************************************/
            printf("[DRIVE] 检测到人脸 %zu 个，直行 100 mm/s\n", det_result->boxes.size());
            if (chassis_ok) chassis.SendVelocity(100, 0, 0);
        }
        else {
            // 未检测到人脸，清除OSD上的检测框并停车
            cout << "[STOP] 未检测到人脸，停车" << endl;
            std::vector<std::array<float, 4>> empty_boxes;
            visualizer.Draw(empty_boxes);  // 传入空向量清除显示
            if (chassis_ok) chassis.SendVelocity(0, 0, 0);
        }
        
        //num_frames += 1;  // 帧计数器递增
    }

    // 等待监听线程退出，释放资源
    if (listener_thread.joinable()) {
        listener_thread.join();
    }
    
    /******************************************************************************************
     * 4. 资源释放
     ******************************************************************************************/
    
    delete det_result;  // 释放检测结果
    detector.Release();  // 释放检测器资源
    processor.Release();  // 释放图像处理器资源
    visualizer.Release();  // 释放可视化器资源
    
    if (ssne_release()) {
        fprintf(stderr, "SSNE release failed!\n");
        return -1;
    }
    
    return 0;
}
 
