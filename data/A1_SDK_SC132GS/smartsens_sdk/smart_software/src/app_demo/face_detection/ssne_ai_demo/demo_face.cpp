/*
 * @Filename: demo_face.cpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/common.hpp"
#include "include/utils.hpp"

using namespace std;

bool g_exit_flag = false;
std::mutex g_mtx;

struct RuntimeState {
    uint64_t frame_index = 0;
    bool chassis_ready = false;
};

namespace {

void keyboard_listener() {
    std::string input;
    std::cout << "键盘监听线程已启动，输入 'q' 退出程序..." << std::endl;

    while (true) {
        std::cin >> input;
        std::lock_guard<std::mutex> lock(g_mtx);
        if (input == "q" || input == "Q") {
            g_exit_flag = true;
            std::cout << "检测到退出指令，通知主线程退出..." << std::endl;
            break;
        }
        std::cout << "输入无效（仅 'q' 有效），请重新输入：" << std::endl;
    }
}

bool check_exit_flag() {
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_exit_flag;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    array<int, 2> img_shape = {cfg::SENSOR_WIDTH, cfg::SENSOR_HEIGHT};

    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
    }

    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape);

    VISUALIZER visualizer;
    visualizer.Initialize(img_shape, "shared_colorLUT.sscl");

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();

    cout << "sleep for 0.2 second!" << endl;
    usleep(200000);
    cout << "[A1] video baseline canvas=" << cfg::SENSOR_WIDTH << "x" << cfg::SENSOR_HEIGHT << endl;
    cout << "[A1] draw demo-rps background" << endl;
    visualizer.DrawBitmap("background.ssbmp", "shared_colorLUT.sscl", 0, 0, 2);
    cout << "[A1] demo-rps background draw call returned" << endl;

    ssne_tensor_t img_sensor;
    RuntimeState runtime;
    runtime.chassis_ready = chassis_ready;

    std::thread listener_thread(keyboard_listener);
    auto last_status_log = std::chrono::steady_clock::now();

    while (!check_exit_flag()) {
        processor.GetImage(&img_sensor);
        runtime.frame_index += 1;

        if (runtime.chassis_ready) {
            chassis.SendVelocity(0, 0, 0);
        }

        ChassisState chassis_state;
        chassis.ReadTelemetry(chassis_state);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_status_log >= std::chrono::seconds(2)) {
            printf("[A1] frame=%llu video=1 inference=0 vx=0 vz=0 tele_vx=%d volt=%.2f\n",
                   static_cast<unsigned long long>(runtime.frame_index),
                   chassis_state.vx,
                   chassis_state.volt);
            last_status_log = now;
        }
    }

    if (runtime.chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
        chassis.Release();
    }

    if (listener_thread.joinable()) {
        listener_thread.join();
    }

    processor.Release();
    visualizer.Release();

    if (ssne_release()) {
        fprintf(stderr, "SSNE release failed!\n");
        return -1;
    }

    return 0;
}
