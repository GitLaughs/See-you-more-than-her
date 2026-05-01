/*
 * @Filename: demo_rps_game.cpp
 * @Description: Demo-rps video baseline with gesture-to-chassis control
 */
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/utils.hpp"

using namespace std;

bool g_exit_flag = false;
std::mutex g_mtx;

namespace {

constexpr int16_t kForwardVx = 100;
constexpr int16_t kStopVx = 0;
constexpr int16_t kBackwardVx = -100;
constexpr int kConfirmFrames = 3;

struct RuntimeState {
    uint64_t frame_index = 0;
    std::string candidate = "NoTarget";
    std::string locked = "NoTarget";
    int candidate_frames = 0;
    bool chassis_ready = false;
};

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

std::string stabilize_label(RuntimeState* state, const std::string& label) {
    if (label == state->candidate) {
        state->candidate_frames += 1;
    } else {
        state->candidate = label;
        state->candidate_frames = 1;
    }

    if (state->candidate_frames >= kConfirmFrames) {
        state->locked = state->candidate;
    }

    return state->locked;
}

void select_velocity(const std::string& label, int16_t* vx, int16_t* vy, int16_t* vz) {
    *vx = kStopVx;
    *vy = 0;
    *vz = 0;

    if (label == "P") {
        *vx = kForwardVx;
    } else if (label == "R") {
        *vx = kStopVx;
    } else if (label == "S") {
        *vx = kBackwardVx;
    }
}

}  // namespace

int main() {
    array<int, 2> img_shape = {1920, 1080};
    array<int, 2> cls_shape = {320, 320};
    string path_cls = "/app_demo/app_assets/models/model_rps.m1model";

    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
    }

    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape);

    RPS_CLASSIFIER classifier;
    classifier.Initialize(path_cls, &img_shape, &cls_shape);

    VISUALIZER visualizer;
    visualizer.Initialize(img_shape, "shared_colorLUT.sscl");

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();

    cout << "sleep for 0.2 second!" << endl;
    usleep(200000);
    visualizer.DrawBitmap("background.ssbmp", "shared_colorLUT.sscl", 0, 0, 2);

    ssne_tensor_t img_sensor;
    RuntimeState runtime;
    runtime.chassis_ready = chassis_ready;

    std::thread listener_thread(keyboard_listener);
    auto last_status_log = std::chrono::steady_clock::now();

    while (!check_exit_flag()) {
        processor.GetImage(&img_sensor);

        std::string label;
        float score = 0.0f;
        classifier.Predict(&img_sensor, label, score);
        const std::string locked_label = stabilize_label(&runtime, label);

        int16_t vx = 0;
        int16_t vy = 0;
        int16_t vz = 0;
        select_velocity(locked_label, &vx, &vy, &vz);
        if (runtime.chassis_ready) {
            chassis.SendVelocity(vx, vy, vz);
        }

        ChassisState chassis_state;
        chassis.ReadTelemetry(chassis_state);

        runtime.frame_index += 1;
        const auto now = std::chrono::steady_clock::now();
        if (now - last_status_log >= std::chrono::seconds(2)) {
            printf("[A1] frame=%llu label=%s score=%.3f locked=%s vx=%d vz=%d tele_vx=%d volt=%.2f\n",
                   static_cast<unsigned long long>(runtime.frame_index),
                   label.c_str(),
                   score,
                   locked_label.c_str(),
                   vx,
                   vz,
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

    classifier.Release();
    processor.Release();
    visualizer.Release();

    if (ssne_release()) {
        fprintf(stderr, "SSNE release failed!\n");
        return -1;
    }

    return 0;
}
