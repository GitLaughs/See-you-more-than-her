/*
 * @Filename: online_ped_osd.cpp
 * @Author: Twyla Tu
 * @Email: qian.tu@smartsenstech.com
 * @Date: 2024-08-08 15-10-45
 * @Copyright (c) 2024 SmartSens
 */

#include <fstream>
#include <iostream>
#include <cstring>
#include "include/utils.hpp"

using namespace std;

std::chrono::duration<double, std::milli> calculateAvg(
    const std::vector<std::chrono::duration<double, std::milli>>& durations) {
    std::chrono::duration<double, std::milli> total_duration(0);
    for (const auto& duration : durations) {
        total_duration += duration;
        // std::cout << duration.count() << " ";
    }
    // std::cout << endl;
    auto average =  durations.size() > 0 ? (total_duration / durations.size()) : total_duration;
    return average;
}

int main(int argc, char* argv[]) {
    // 视频路径
    string video_ext = ".bin";
    int save_num = 5;
    // 推理的帧数
    int num_frames = 0;
    if (argc > 1) {
        if (argc == 2) {
            num_frames = stoi(argv[1]);
            // 此时输出应为单帧
            cout << "Number of frames: " << num_frames << endl;
        }
        else {
            cerr << "Invalid number of arguments!" << endl;
            return -1;
        }
    }

    // 定义检测模型路径 
    string path_det = "/app_assets/models/ped_split_WHETRON.m1model";
    // 定义分类模型路径 
    string path_cls = "/app_assets/models/cls.m1model";
    // 文件路径
    std::string filename_z = "app_assets/maps/distanceMap.bin";
    std::string filename_x = "app_assets/maps/offsetMap.bin";
    
    // 人脸检测模型初始化
    YOLOV7 detector;
    
    // sensor输出图像大小
    int img_height = 1080;
    int img_width = 1920;
    // online输出图像大小
    uint16_t online_width = img_width / kDownSample4x;
    uint16_t online_height = img_height / kDownSample4x;
    // 模型检测图像大小
    int det_height = 320;
    int det_width = 480;
    // 缩放尺度
    float scale = 4;
    float conf_threshold = 0.55;
    float threshold1_dirty = 0.6;  // 脏污分类的阈值
    float threshold2_dirty = 0.6;  // 脏污分类的阈值
    int threshold_dirty_count = 8;  // 连续多少帧检测到脏污
    int count_dirty = 0;
    // 图像是否镜像
    bool img_flip = true;

    if (ssne_initial())
    {
        fprintf(stderr, "ssne initial fail!\n");
    }

    array<int, 2> img_shape = {img_width, img_height};
    array<int, 2> det_shape = {det_width, det_height};
    detector.Initialize(path_det);
    // 人脸检测结果初始化
    DetectionResult* det_result = new DetectionResult;
    // cout << "[INFO] Detection model initialized!" << endl;
    // 分类模型初始化
    MobileNetV2 classifier;
    classifier.Initialize(path_cls);
    bool flag_dirty = false;

    // 图像输入pipe初始化
    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape, &det_shape, &scale);
    // cout << "[INFO] Onpipe & Offpipe initialized!" << endl;
    // online图像tensor初始化
    ssne_tensor_t input[1];
    ssne_tensor_t output[6];
    ssne_tensor_t output_cls[1];

    Calculator calculator;
    calculator.Initialize(filename_x, filename_z, img_flip, img_shape);

    int num_dets = 0;
    string img_path;
    
    // 计时
    std::vector<std::chrono::duration<double, std::milli>> durations_all;
    
    VISUALIZER visualizer;
    visualizer.Initialize(scale, img_shape);
    
    int k = 0;
    bool out_uart = false;
    
    auto start = std::chrono::high_resolution_clock::now();
    // while(true)
    while((k < num_frames) || (num_frames==0))
    {   
        if(k % 100 == 0){
            // cout << "[INFO] Frame: " << k << " starts..." << endl;
            // print_memory();
        }
        
        processor.GetImage(&input[0]);
        
        // 人脸检测模型推理
        detector.Predict(input, output);
        // 脏污检测
        classifier.Predict(input, output_cls);
        // 检测结果后处理
        visualizer.Run(output, det_result, conf_threshold);

        float *score_cls = (float*)get_data(output_cls[0]);
        if ((score_cls[1] > threshold1_dirty) || (score_cls[2] > threshold2_dirty)) {
            count_dirty += 1;
            flag_dirty = count_dirty >= threshold_dirty_count ? true : false;
        }
        else {
            count_dirty = 0;
            flag_dirty = false;
        }
        // std::cout << "dirty score: " << score_cls[0] << " " << score_cls[1] << " " << score_cls[2] << std::endl;
        
        // cout << "[INFO] Frame: " << k << " - detector inference finished!" << endl;
        out_uart = k % 15 == 0 ? true : false; 
        calculator.Run(det_result, flag_dirty, out_uart);
        const std::vector<DetPed> pedestrians = calculator.getPedestrians();
        visualizer.Draw(det_result, pedestrians);

        k = (k + 1) % 15;
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - start);
        start = std::chrono::high_resolution_clock::now();
        durations_all.push_back(duration);
        
        if (k % 100 == 0){
            std::chrono::duration<double, std::milli> avg;
            avg = calculateAvg(durations_all);
            // fprintf(stderr, "getimage - : %.1fms\n", avg.count());
            durations_all.clear();
            // system("cat /proc/meminfo | grep MemFree");
        }
    }
    
    //std::chrono::duration<double, std::milli> avg;
    //avg = calculateAvg(durations_getimage);
    //fprintf(stderr, "getimage - : %.1fms\n", avg.count());
    //avg = calculateAvg(processor.durations_preprocess);
    //fprintf(stderr, "det - preprocess: %.1fms\n", avg.count());
    //avg = calculateAvg(detector.durations_forward);
    //fprintf(stderr, "det - forward: %.1fms\n", avg.count());
    //avg = calculateAvg(detector.durations_postprocess);
    //fprintf(stderr, "det - postprocess: %.1fms\n", avg.count());
    //avg = calculateAvg(durations_osd);
    //fprintf(stderr, "osd - : %.1fms\n", avg.count());

    // 释放内存
    delete det_result;
    release_tensor(output[0]);
    release_tensor(output[1]);
    release_tensor(output[2]);
    release_tensor(output[3]);
    release_tensor(output[4]);
    release_tensor(output[5]);
    detector.Release();
    release_tensor(output_cls[0]);
    classifier.Release();
    processor.Release();
    visualizer.Release();
    calculator.Release();
    durations_all.clear();

    ssne_release();

    return 0;
}
