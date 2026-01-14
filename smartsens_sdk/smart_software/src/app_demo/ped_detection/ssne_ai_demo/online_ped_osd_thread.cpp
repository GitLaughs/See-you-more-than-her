
/*
 * @Filename: online_ped_osd_thread.cpp
 * @Author: Twyla Tu
 * @Email: qian.tu@smartsenstech.com
 * @Date: 2024-08-08 15-10-25
 * @Copyright (c) 2024 SmartSens
 */
#include <fstream>
#include <iostream>
#include <cstring>
#include "include/utils.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <getopt.h>
#include "nlohmann/json.hpp"
using namespace std;
using json = nlohmann::json;
std::mutex mtx1;
std::condition_variable cv_det_for_post;
bool end_det_for_post = false;  // 控制交替的标志
std::condition_variable cv_cls_for_post;
bool end_cls_for_post = false;  // 控制交替的标志
std::atomic<bool> all_close(false);
// 人脸检测模型初始化
YOLOV7 detector;
// 分类模型初始化
MobileNetV2 classifier;
// 人脸检测结果初始化
DetectionResult* det_result = new DetectionResult;
// 图像输入pipe初始化
IMAGEPROCESSOR processor;
// online图像tensor初始化
ssne_tensor_t input[1];
ssne_tensor_t output[6];
ssne_tensor_t output_cls[1];
float conf = 0.55f;
float threshold1_dirty = 0.6;  // 脏污分类的阈值
float threshold2_dirty = 0.6;  // 脏污分类的阈值
int threshold_dirty_count = 8;  // 连续多少帧检测到脏污
int count_dirty = 0;
VISUALIZER visualizer;
Calculator calculator;
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
void detection(int maxNum) {
    int k = 0;
    // 计时
    std::vector<std::chrono::duration<double, std::milli>> durations_all;
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start);
    while ((k < maxNum) || (maxNum == 0)){
        //std::cout << "start capture: " << k  << std::endl;
        processor.GetImage(&input[0]);
        //std::cout << "end capture: " << k  << std::endl;
		if (k == 0)
			save_tensor_buffer(input[0], "/app_demo/app_assets/imgs/save2_yuv16_result.bin");
		visualizer.Draw();
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration<double, std::milli>(end - start);
        start = std::chrono::high_resolution_clock::now();
        durations_all.push_back(duration);
        k++;
        if (k % 100 == 0){
            std::chrono::duration<double, std::milli> avg;
            avg = calculateAvg(durations_all);
			printf("detection getimage - : %.1fms\n", avg.count());
            // fprintf(stdout, "getimage - : %.1fms\n", avg.count());
            durations_all.clear();
            // system("cat /proc/meminfo | grep MemFree");
        }
        if (all_close) break;
    }
    // 释放内存
    // cout << "Start Release Processor" << endl;
    processor.Release();
    cout << "Processor Released" << endl;
    // cout << "Start Release Detector" << endl;
    release_tensor(output[0]);
    release_tensor(output[1]);
    release_tensor(output[2]);
    release_tensor(output[3]);
    release_tensor(output[4]);
    release_tensor(output[5]);
    detector.Release();
    release_tensor(output_cls[0]);
    classifier.Release();
    durations_all.clear();
    cout << "Detector Released" << endl;
}
void post(int maxNum) {
    int k = 0;
    // const OSD osd_ped = visualizer.getOSD();
    bool flag_dirty = false;
    bool out_uart = false;
    while ((k < maxNum) || (maxNum == 0)){
        {
            std::unique_lock<std::mutex> lock(mtx1);
            cv_det_for_post.wait(lock, [] { return (end_det_for_post || all_close);}); // 等待turn为false
            end_det_for_post = false;
            if (all_close) break;
        }
        // std::cout << "start post: " << k << std::endl;
        // 后处理
        visualizer.Run(output, det_result, conf);
        {
            std::unique_lock<std::mutex> lock(mtx1);
            cv_cls_for_post.wait(lock, [] { return (end_cls_for_post || all_close);}); // 等待turn为false
            end_cls_for_post = false;
            if (all_close) break;
        }
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
        out_uart = k % 15 == 0 ? true : false;
        calculator.Run(det_result, flag_dirty, out_uart);
        // 画图
        const std::vector<DetPed> pedestrians = calculator.getPedestrians();
        //visualizer.Draw(det_result, pedestrians);
        // std::cout << "end post: " << k << std::endl;
        k = (k + 1) % 15;
        if (all_close) break;
    }
    // cout << "Start Release Visualizer" << endl;
    delete det_result;
    visualizer.Release();
    calculator.Release();
    cout << "Visualizer Released" << endl;
}
int main(int argc, char* argv[]) {
    // get option
    int next_option;
    const char* const short_options = "f:h:";
    const struct option long_options[] = {
        { "config", 1, NULL, 'f'},
        { "help", 1, NULL, 'h'},
        { NULL, 0, NULL, 0 }
    };
    char * filePath = NULL;
    do {
        next_option = getopt_long(argc, argv, short_options, long_options, NULL);
        switch (next_option)
        {
        case 'f':
            filePath = optarg;
            break;
        case 'h':
            break;
        default:
            break;
        }
    } while (next_option != -1);
    if (filePath == NULL){
        printf("Usage: ./ssne_ai_demo -f  [config.json]. ");
        exit(0);
    }
    json app_config;
	ifstream jfile(filePath);
	jfile >> app_config;		// 以文件流形式读取 json 文件
    // 推理的帧数
    int num_frames = app_config.at("num_frames");
    // 定义检测模型路径
    string path_det = app_config.at("path_det");
    // 定义分类模型路径
    string path_cls = app_config.at("path_cls");
    cout << "[INFO] path_det " << path_det.c_str() <<endl;
    // sensor输出图像大小
    int img_height = app_config.at("img_height");
    int img_width = app_config.at("img_width");
    // online输出图像大小
    uint16_t online_width = img_width / kDownSample4x;
    uint16_t online_height = img_height / kDownSample4x;
    // 模型检测图像大小
    int det_height = app_config.at("det_height");
    int det_width = app_config.at("det_width");
    // 缩放尺度
    float scale = 4;
    // 图像是否镜像
    bool img_flip = true;
    if (ssne_initial()){
        fprintf(stderr, "ssne initial fail!\n");
    }
    array<int, 2> img_shape = {img_width, img_height};
    array<int, 2> det_shape = {det_width, det_height};
    cout << "[INFO] Detection Model initialized!" << endl;
    detector.Initialize(path_det);
    classifier.Initialize(path_cls);
    processor.Initialize(&img_shape, &det_shape, &scale);
    cout << "[INFO] Onpipe & Offpipe initialized!" << endl;
    // 文件路径
    std::string filename_z = app_config.at("map_filename_z");
    std::string filename_x = app_config.at("map_filename_x");
    calculator.Initialize(filename_x, filename_z, img_flip, img_shape);
    int num_dets = 0;
    string img_path;
    visualizer.Initialize(scale, img_shape);
    cout << "[INFO] Visualizer initialized!" << endl;
    std::thread Thread1(detection, num_frames);
    //std::thread Thread2(post, num_frames);
    int out;
    std::cout << "press: " << std::endl;
    std::cin >> out;
    std::cout << "press: " << out << std::endl;
    if (out == 1){
        all_close = true;
        cv_det_for_post.notify_one();
        cv_cls_for_post.notify_one();
    }
    Thread1.join();
    //Thread2.join();
    ssne_release();
    return 0;
}
