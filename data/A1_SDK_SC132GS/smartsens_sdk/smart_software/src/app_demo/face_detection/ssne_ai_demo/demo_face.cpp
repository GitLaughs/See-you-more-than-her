/*
 * @Filename: demo_face.cpp
 * @Author: Hongying He
 * @Email: hongying.he@smartsenstech.com
 * @Date: 2025-12-30 14-57-47
 * @Copyright (c) 2025 SmartSens
 */
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/utils.hpp"

using namespace std;

bool g_exit_flag = false;
std::mutex g_mtx;

enum class ActionState {
    Idle,
    Forward,
    StopGesture,
    AvoidLeft,
    AvoidRight,
    Blocked,
};

enum class SemanticLabel {
    NoTarget,
    Person,
    Forward,
    Stop,
    Obstacle,
};

struct FrameScores {
    float person = 0.f;
    float forward = 0.f;
    float stop = 0.f;
    float obstacle = 0.f;
};

struct VisionUiState {
    SemanticLabel label = SemanticLabel::NoTarget;
    float confidence = 0.f;
    bool NoTarget = true;
    bool target_locked = false;
    ActionState action_hint = ActionState::Idle;
    bool safe_to_move = false;
};

struct SemanticStabilizer {
    std::deque<FrameScores> history;
    SemanticLabel candidate = SemanticLabel::NoTarget;
    SemanticLabel locked = SemanticLabel::NoTarget;
    int candidate_frames = 0;
    int hold_frames = 0;
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

constexpr int kSemanticAverageFrames = 5;
constexpr int kSemanticLockFrames = 3;
constexpr int kSemanticHoldFrames = 6;
constexpr float kSemanticNoTargetThreshold = 0.35f;
constexpr int kLayerAnimation = 3;
constexpr int kLayerPrompt = 4;

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

const char* semantic_label_name(SemanticLabel label) {
    switch (label) {
        case SemanticLabel::NoTarget: return "NoTarget";
        case SemanticLabel::Person: return "person";
        case SemanticLabel::Forward: return "forward";
        case SemanticLabel::Stop: return "stop";
        case SemanticLabel::Obstacle: return "obstacle";
    }
    return "unknown";
}

FrameScores extract_frame_scores(const FaceDetectionResult& det_result) {
    FrameScores scores;
    for (size_t i = 0; i < det_result.class_ids.size() && i < det_result.scores.size(); ++i) {
        const float score = det_result.scores[i];
        switch (det_result.class_ids[i]) {
            case cfg::TARGET_CLASS_PERSON:
                scores.person = std::max(scores.person, score);
                break;
            case cfg::TARGET_CLASS_FORWARD:
                scores.forward = std::max(scores.forward, score);
                break;
            case cfg::TARGET_CLASS_STOP:
                scores.stop = std::max(scores.stop, score);
                break;
            case cfg::TARGET_CLASS_OBSTACLE_BOX:
                scores.obstacle = std::max(scores.obstacle, score);
                break;
        }
    }
    return scores;
}

FrameScores average_scores(const std::deque<FrameScores>& history) {
    FrameScores avg;
    if (history.empty()) {
        return avg;
    }

    for (const auto& scores : history) {
        avg.person += scores.person;
        avg.forward += scores.forward;
        avg.stop += scores.stop;
        avg.obstacle += scores.obstacle;
    }

    const float inv = 1.0f / static_cast<float>(history.size());
    avg.person *= inv;
    avg.forward *= inv;
    avg.stop *= inv;
    avg.obstacle *= inv;
    return avg;
}

SemanticLabel choose_semantic_label(const FrameScores& scores, float* confidence) {
    *confidence = scores.obstacle;
    if (scores.obstacle >= kSemanticNoTargetThreshold) {
        return SemanticLabel::Obstacle;
    }

    *confidence = scores.stop;
    if (scores.stop >= kSemanticNoTargetThreshold) {
        return SemanticLabel::Stop;
    }

    *confidence = scores.forward;
    if (scores.forward >= kSemanticNoTargetThreshold) {
        return SemanticLabel::Forward;
    }

    *confidence = scores.person;
    if (scores.person >= kSemanticNoTargetThreshold) {
        return SemanticLabel::Person;
    }

    *confidence = 0.f;
    return SemanticLabel::NoTarget;
}

ActionState semantic_action_hint(SemanticLabel label, ActionState obstacle_action) {
    switch (label) {
        case SemanticLabel::Forward:
            return ActionState::Forward;
        case SemanticLabel::Stop:
            return ActionState::StopGesture;
        case SemanticLabel::Obstacle:
            if (obstacle_action == ActionState::AvoidLeft ||
                obstacle_action == ActionState::AvoidRight ||
                obstacle_action == ActionState::Blocked) {
                return obstacle_action;
            }
            return ActionState::Blocked;
        case SemanticLabel::Person:
        case SemanticLabel::NoTarget:
            return ActionState::Idle;
    }
    return ActionState::Idle;
}

VisionUiState update_semantic_state(SemanticStabilizer* stabilizer,
                                    const FaceDetectionResult& det_result,
                                    ActionState obstacle_action) {
    stabilizer->history.push_back(extract_frame_scores(det_result));
    while (static_cast<int>(stabilizer->history.size()) > kSemanticAverageFrames) {
        stabilizer->history.pop_front();
    }

    const FrameScores avg = average_scores(stabilizer->history);
    float confidence = 0.f;
    const SemanticLabel candidate = choose_semantic_label(avg, &confidence);

    if (candidate == stabilizer->candidate) {
        stabilizer->candidate_frames += 1;
    } else {
        stabilizer->candidate = candidate;
        stabilizer->candidate_frames = 1;
    }

    if (candidate != SemanticLabel::NoTarget && stabilizer->candidate_frames >= kSemanticLockFrames) {
        stabilizer->locked = candidate;
        stabilizer->hold_frames = kSemanticHoldFrames;
    } else if (stabilizer->hold_frames > 0) {
        stabilizer->hold_frames -= 1;
    } else if (candidate == SemanticLabel::NoTarget) {
        stabilizer->locked = SemanticLabel::NoTarget;
    }

    VisionUiState state;
    state.label = stabilizer->locked;
    state.confidence = confidence;
    state.NoTarget = state.label == SemanticLabel::NoTarget;
    state.target_locked = !state.NoTarget;
    state.action_hint = semantic_action_hint(state.label, obstacle_action);
    state.safe_to_move = state.action_hint == ActionState::Forward ||
                         state.action_hint == ActionState::AvoidLeft ||
                         state.action_hint == ActionState::AvoidRight;
    return state;
}

void render_semantic_osd(VISUALIZER* visualizer,
                         const VisionUiState& state,
                         uint64_t frame_index,
                         SemanticLabel* last_label) {
    if (state.label != *last_label) {
        visualizer->ClearLayer(kLayerAnimation);
        visualizer->ClearLayer(kLayerPrompt);
        *last_label = state.label;
    }

    switch (state.label) {
        case SemanticLabel::NoTarget:
            visualizer->ClearLayer(kLayerAnimation);
            visualizer->ClearLayer(kLayerPrompt);
            break;
        case SemanticLabel::Person:
            visualizer->DrawBitmap("hello_bubble.ssbmp", "shared_colorLUT.sscl", 260, 40, kLayerPrompt);
            visualizer->DrawBitmap("hello_icon.ssbmp", "shared_colorLUT.sscl", 220, 52, kLayerPrompt);
            break;
        case SemanticLabel::Forward: {
            const int idx = static_cast<int>((frame_index / 5) % 4);
            visualizer->DrawBitmap("car_forward_" + std::to_string(idx) + ".ssbmp", "shared_colorLUT.sscl", 160, 280, kLayerAnimation);
            break;
        }
        case SemanticLabel::Stop: {
            const int idx = static_cast<int>((frame_index / 5) % 3);
            visualizer->DrawBitmap("car_stop_" + std::to_string(idx) + ".ssbmp", "shared_colorLUT.sscl", 160, 280, kLayerAnimation);
            break;
        }
        case SemanticLabel::Obstacle: {
            const int idx = static_cast<int>((frame_index / 5) % 6);
            visualizer->DrawBitmap("obstacle_alert.ssbmp", "shared_colorLUT.sscl", 80, 40, kLayerPrompt);
            visualizer->DrawBitmap("car_detour_" + std::to_string(idx) + ".ssbmp", "shared_colorLUT.sscl", 80, 190, kLayerAnimation);
            break;
        }
    }
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
    visualizer.Initialize(img_shape, "shared_colorLUT.sscl");

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();

    cout << "sleep for 0.2 second!" << endl;
    usleep(200000);
    visualizer.DrawBitmap("background.ssbmp", "shared_colorLUT.sscl", 0, 0, 2);

    ssne_tensor_t img_sensor;
    RuntimeState runtime;
    runtime.chassis_ready = chassis_ready;
    SemanticStabilizer semantic_stabilizer;
    VisionUiState ui_state;
    SemanticLabel last_osd_label = SemanticLabel::NoTarget;

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

        const ActionState raw_action = decide_action(
            *det_result,
            &runtime,
            static_cast<float>(crop_shape[0]),
            static_cast<float>(crop_shape[1]));
        ui_state = update_semantic_state(&semantic_stabilizer, *det_result, raw_action);
        runtime.action = ui_state.action_hint;
        runtime.frame_index += 1;
        render_semantic_osd(&visualizer, ui_state, runtime.frame_index, &last_osd_label);

        int16_t vx = 0;
        int16_t vy = 0;
        int16_t vz = 0;
        select_velocity(runtime.action, &vx, &vy, &vz);
        if (runtime.chassis_ready) {
            chassis.SendVelocity(vx, vy, vz);
        }

        ChassisState chassis_state;
        chassis.ReadTelemetry(chassis_state);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_status_log >= std::chrono::seconds(2)) {
            printf("[A1] frame=%llu label=%s conf=%.3f locked=%d action=%s safe=%d det=%d obstacle=%.3f center=%.2f bottom=%.2f vx=%d vz=%d tele_vx=%d volt=%.2f\n",
                   static_cast<unsigned long long>(runtime.frame_index),
                   semantic_label_name(ui_state.label),
                   ui_state.confidence,
                   ui_state.target_locked ? 1 : 0,
                   action_name(runtime.action),
                   ui_state.safe_to_move ? 1 : 0,
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
