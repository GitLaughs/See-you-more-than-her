/*
 * @Filename: demo_face.cpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/utils.hpp"

using namespace std;

bool g_exit_flag = false;
std::mutex g_mtx;

struct osdInfo {
    std::string filename;
    uint16_t x;
    uint16_t y;
};

enum class ActionState {
    Idle,
    Forward,
    StopGesture,
    AvoidLeft,
    AvoidRight,
    Blocked,
};

struct DetectionSummary {
    bool found = false;
    std::array<float, 4> box = {0.f, 0.f, 0.f, 0.f};
    float score = 0.f;
    float area_ratio = 0.f;
    float center_x_ratio = 0.5f;
    float bottom_ratio = 0.f;
};

struct RuntimeState {
    ActionState action = ActionState::Idle;
    int forward_frames = 0;
    int stop_frames = 0;
    int obstacle_frames = 0;
    int clear_frames = 0;
    uint64_t frame_index = 0;
    bool chassis_ready = false;
    int det_count = 0;
    DetectionSummary obstacle;
};

namespace {

const char* action_name(ActionState action) {
    switch (action) {
        case ActionState::Idle: return "idle";
        case ActionState::Forward: return "forward";
        case ActionState::StopGesture: return "stop_gesture";
        case ActionState::AvoidLeft: return "avoid_left";
        case ActionState::AvoidRight: return "avoid_right";
        case ActionState::Blocked: return "blocked";
    }
    return "unknown";
}

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

std::vector<std::array<float, 4>> to_osd_boxes(const FaceDetectionResult& det_result,
                                               float crop_offset_y)
{
    std::vector<std::array<float, 4>> boxes_original_coord;
    boxes_original_coord.reserve(det_result.boxes.size());
    for (const auto& box : det_result.boxes) {
        boxes_original_coord.push_back({
            box[0],
            box[1] + crop_offset_y,
            box[2],
            box[3] + crop_offset_y,
        });
    }
    return boxes_original_coord;
}

DetectionSummary summarize_best_detection(const FaceDetectionResult& det_result,
                                          int class_id,
                                          float frame_w,
                                          float frame_h)
{
    DetectionSummary summary;
    float best_rank = -1.0f;

    for (size_t i = 0; i < det_result.boxes.size() && i < det_result.class_ids.size(); ++i) {
        if (det_result.class_ids[i] != class_id) {
            continue;
        }

        const auto& box = det_result.boxes[i];
        const float width = std::max(0.0f, box[2] - box[0]);
        const float height = std::max(0.0f, box[3] - box[1]);
        const float area_ratio = (width * height) / std::max(1.0f, frame_w * frame_h);
        const float score = i < det_result.scores.size() ? det_result.scores[i] : 0.0f;
        const float rank = area_ratio * 2.0f + score;
        if (rank <= best_rank) {
            continue;
        }

        best_rank = rank;
        summary.found = true;
        summary.box = box;
        summary.score = score;
        summary.area_ratio = area_ratio;
        summary.center_x_ratio = ((box[0] + box[2]) * 0.5f) / std::max(1.0f, frame_w);
        summary.bottom_ratio = box[3] / std::max(1.0f, frame_h);
    }

    return summary;
}

ActionState decide_action(const FaceDetectionResult& det_result,
                          RuntimeState* runtime,
                          float frame_w,
                          float frame_h)
{
    const DetectionSummary forward = summarize_best_detection(
        det_result, cfg::TARGET_CLASS_FORWARD, frame_w, frame_h);
    const DetectionSummary stop = summarize_best_detection(
        det_result, cfg::TARGET_CLASS_STOP, frame_w, frame_h);
    const DetectionSummary obstacle = summarize_best_detection(
        det_result, cfg::TARGET_CLASS_OBSTACLE_BOX, frame_w, frame_h);

    runtime->det_count = static_cast<int>(det_result.boxes.size());
    runtime->obstacle = obstacle;

    runtime->forward_frames = forward.found ? runtime->forward_frames + 1 : 0;
    runtime->stop_frames = stop.found ? runtime->stop_frames + 1 : 0;

    const bool obstacle_confirmed = obstacle.found && (
        obstacle.area_ratio >= cfg::OBSTACLE_WARN_AREA_RATIO ||
        obstacle.bottom_ratio >= cfg::OBSTACLE_BOTTOM_RATIO);
    runtime->obstacle_frames = obstacle_confirmed ? runtime->obstacle_frames + 1 : 0;
    runtime->clear_frames = obstacle.found ? 0 : runtime->clear_frames + 1;

    if (runtime->stop_frames >= cfg::GESTURE_CONFIRM_FRAMES) {
        return ActionState::StopGesture;
    }

    if (runtime->obstacle_frames >= cfg::OBSTACLE_CONFIRM_FRAMES) {
        const bool near_center = obstacle.center_x_ratio >= cfg::OBSTACLE_CENTER_LEFT &&
                                 obstacle.center_x_ratio <= cfg::OBSTACLE_CENTER_RIGHT;
        const bool very_near = obstacle.area_ratio >= cfg::OBSTACLE_NEAR_AREA_RATIO ||
                               obstacle.bottom_ratio >= cfg::OBSTACLE_BOTTOM_RATIO;
        if (very_near && near_center) {
            return ActionState::Blocked;
        }
        if (obstacle.center_x_ratio < cfg::OBSTACLE_CENTER_LEFT) {
            return ActionState::AvoidRight;
        }
        if (obstacle.center_x_ratio > cfg::OBSTACLE_CENTER_RIGHT) {
            return ActionState::AvoidLeft;
        }
        return ActionState::Blocked;
    }

    if (runtime->forward_frames >= cfg::GESTURE_CONFIRM_FRAMES) {
        return ActionState::Forward;
    }

    if ((runtime->action == ActionState::AvoidLeft ||
         runtime->action == ActionState::AvoidRight ||
         runtime->action == ActionState::Blocked) &&
        runtime->clear_frames < cfg::CLEAR_CONFIRM_FRAMES) {
        return runtime->action;
    }

    return ActionState::Idle;
}

void select_velocity(ActionState action, int16_t* vx, int16_t* vy, int16_t* vz) {
    *vx = cfg::VX_STOP;
    *vy = 0;
    *vz = 0;

    switch (action) {
        case ActionState::Forward:
            *vx = cfg::VX_FORWARD;
            break;
        case ActionState::AvoidLeft:
            *vx = cfg::VX_FORWARD;
            *vz = cfg::VZ_TURN;
            break;
        case ActionState::AvoidRight:
            *vx = cfg::VX_FORWARD;
            *vz = -cfg::VZ_TURN;
            break;
        case ActionState::StopGesture:
        case ActionState::Blocked:
        case ActionState::Idle:
            break;
    }
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    int img_width = cfg::SENSOR_WIDTH;
    int img_height = cfg::SENSOR_HEIGHT;
    array<int, 2> det_shape = {cfg::DET_WIDTH, cfg::DET_HEIGHT};
    string path_det = cfg::MODEL_PATH;

    static osdInfo osds[3] = {
        {"si.ssbmp", 10, 10},
        {"te.ssbmp", 90, 10},
        {"wei.ssbmp", 170, 10}
    };

    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
    }

    array<int, 2> img_shape = {img_width, img_height};
    array<int, 2> crop_shape = {cfg::PIPE_CROP_WIDTH, cfg::PIPE_CROP_HEIGHT};
    const float crop_offset_y = static_cast<float>(cfg::PIPE_CROP_Y1);

    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape);

    YOLOV8 detector;
    detector.Initialize(path_det, &crop_shape, &det_shape);

    FaceDetectionResult* det_result = new FaceDetectionResult;

    VISUALIZER visualizer;
    visualizer.Initialize(img_shape);

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();

    cout << "sleep for 0.2 second!" << endl;
    usleep(200000);

    visualizer.DrawBitmap(osds[0].filename, "shared_colorLUT.sscl", osds[0].x, osds[0].y, 2);

    uint16_t num_frames = 0;
    uint8_t osd_index = 0;
    ssne_tensor_t img_sensor;
    RuntimeState runtime;
    runtime.chassis_ready = chassis_ready;

    std::thread listener_thread(keyboard_listener);
    auto last_status_log = std::chrono::steady_clock::now();

    while (!check_exit_flag()) {
        processor.GetImage(&img_sensor);
        detector.Predict(&img_sensor, det_result, cfg::DET_CONF_THRESH);

        if (!det_result->boxes.empty()) {
            visualizer.Draw(to_osd_boxes(*det_result, crop_offset_y));
        } else {
            std::vector<std::array<float, 4>> empty_boxes;
            visualizer.Draw(empty_boxes);
        }

        const ActionState next_action = decide_action(
            *det_result,
            &runtime,
            static_cast<float>(crop_shape[0]),
            static_cast<float>(crop_shape[1]));
        runtime.action = next_action;
        runtime.frame_index += 1;

        int16_t vx = 0;
        int16_t vy = 0;
        int16_t vz = 0;
        select_velocity(runtime.action, &vx, &vy, &vz);
        if (runtime.chassis_ready) {
            chassis.SendVelocity(vx, vy, vz);
        }

        ChassisState chassis_state;
        chassis.ReadTelemetry(chassis_state);

        num_frames += 1;
        osd_index = (num_frames / 10) % 3;
        visualizer.DrawBitmap(osds[osd_index].filename, "shared_colorLUT.sscl", osds[osd_index].x, osds[osd_index].y, 2);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_status_log >= std::chrono::seconds(2)) {
            printf("[A1] frame=%llu action=%s det=%d obstacle=%.3f center=%.2f bottom=%.2f vx=%d vz=%d tele_vx=%d volt=%.2f\n",
                   static_cast<unsigned long long>(runtime.frame_index),
                   action_name(runtime.action),
                   runtime.det_count,
                   runtime.obstacle.area_ratio,
                   runtime.obstacle.center_x_ratio,
                   runtime.obstacle.bottom_ratio,
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

    delete det_result;
    detector.Release();
    processor.Release();
    visualizer.Release();

    if (ssne_release()) {
        fprintf(stderr, "SSNE release failed!\n");
        return -1;
    }

    return 0;
}
