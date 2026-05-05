#include <array>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/rps_classifier.hpp"
#include "include/utils.hpp"

namespace {

constexpr int kCameraWidth = 720;
constexpr int kCameraHeight = 1280;
constexpr int kClassifierInputWidth = 320;
constexpr int kClassifierInputHeight = 320;
constexpr int kClassCount = 5;
constexpr int16_t kForwardVelocity = 200;
constexpr const char* kLabels[kClassCount] = {"person", "stop", "forward", "obstacle", "NoTarget"};

volatile sig_atomic_t g_exit_flag = 0;
ChassisController* g_chassis = nullptr;
bool g_chassis_ready = false;
std::string g_last_cli_action = "stop";

struct LatestRpsSnapshot {
    bool valid = false;
    uint64_t frame_index = 0;
    std::string request_id;
    std::string label = "NoTarget";
    float confidence = 0.0f;
    float scores[kClassCount] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::string action = "stop";
};

std::mutex g_latest_rps_mutex;
std::condition_variable g_latest_rps_cv;
LatestRpsSnapshot g_latest_rps_snapshot;

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    return out.str();
}

std::string action_for_label(const std::string& label) {
    if (label == "forward") return "forward";
    return "stop";
}

int16_t vx_for_action(const std::string& action) {
    return action == "forward" ? kForwardVelocity : 0;
}

void print_debug_response(const std::string& command, const std::string& body, bool success = true) {
    std::cout << "A1_DEBUG {\"command\":\"" << command << "\",\"success\":"
              << (success ? "true" : "false") << "," << body << "}" << std::endl;
}

void update_latest_rps_snapshot(uint64_t frame_index, const std::string& request_id,
                                const std::string& label, float confidence,
                                const float scores[kClassCount], const std::string& action) {
    {
        std::lock_guard<std::mutex> lock(g_latest_rps_mutex);
        g_latest_rps_snapshot.valid = true;
        g_latest_rps_snapshot.frame_index = frame_index;
        g_latest_rps_snapshot.request_id = request_id;
        g_latest_rps_snapshot.label = label;
        g_latest_rps_snapshot.confidence = confidence;
        for (int i = 0; i < kClassCount; ++i) {
            g_latest_rps_snapshot.scores[i] = scores[i];
        }
        g_latest_rps_snapshot.action = action;
    }
    g_latest_rps_cv.notify_all();
}

std::string build_rps_snapshot_json() {
    LatestRpsSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(g_latest_rps_mutex);
        snapshot = g_latest_rps_snapshot;
    }

    if (!snapshot.valid) {
        return "\"message\":\"no classification snapshot yet\"";
    }

    std::ostringstream body;
    body << std::fixed << std::setprecision(3);
    body << "\"request\":\"" << json_escape(snapshot.request_id) << "\""
         << ",\"frame\":" << snapshot.frame_index
         << ",\"camera_w\":" << kCameraWidth
         << ",\"camera_h\":" << kCameraHeight
         << ",\"roi\":{\"x\":200,\"y\":480,\"w\":320,\"h\":320}"
         << ",\"label\":\"" << json_escape(snapshot.label) << "\""
         << ",\"confidence\":" << snapshot.confidence
         << ",\"scores\":[" << snapshot.scores[0] << "," << snapshot.scores[1] << "," << snapshot.scores[2]
         << "," << snapshot.scores[3] << "," << snapshot.scores[4] << "]"
         << ",\"action\":\"" << json_escape(snapshot.action) << "\""
         << ",\"message\":\"latest classification snapshot\"";
    return body.str();
}

void handle_signal(int) {
    g_exit_flag = 1;
}

void send_cli_chassis_action(const std::string& action) {
    const int16_t vx = vx_for_action(action);
    if (g_chassis != nullptr && g_chassis_ready) {
        g_chassis->SendVelocity(vx, 0, 0);
        g_last_cli_action = action;
    }

    std::ostringstream body;
    body << "\"action\":\"" << action
         << "\",\"vx\":" << vx
         << ",\"chassis_ok\":" << (g_chassis_ready ? "true" : "false")
         << ",\"message\":\"forward=move, others=stop\"";
    print_debug_response("chassis_test", body.str(), true);
}

void handle_a1_test_command(const std::string& line) {
    std::istringstream iss(line);
    std::string prefix;
    std::string command;
    iss >> prefix >> command;
    if (prefix != "A1_TEST") return;

    if (command == "ping") {
        print_debug_response("ping", "\"message\":\"pong\",\"chassis_ok\":" + std::string(g_chassis_ready ? "true" : "false"));
        return;
    }
    if (command == "rps_snapshot") {
        std::string request_id;
        iss >> request_id;
        if (request_id.empty()) request_id = "legacy";
        {
            std::lock_guard<std::mutex> lock(g_latest_rps_mutex);
            g_latest_rps_snapshot.request_id = request_id;
        }
        std::unique_lock<std::mutex> lock(g_latest_rps_mutex);
        const bool ready = g_latest_rps_cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return g_latest_rps_snapshot.valid && g_latest_rps_snapshot.request_id == request_id;
        });
        lock.unlock();
        if (!ready) {
            print_debug_response(command, "\"message\":\"classification snapshot timeout\"", false);
            return;
        }
        print_debug_response(command, build_rps_snapshot_json(), true);
        return;
    }
    if (command == "chassis_test") {
        std::string action;
        iss >> action;
        if (action == "forward" || action == "stop") {
            send_cli_chassis_action(action);
            return;
        }
        print_debug_response("chassis_test", "\"error\":\"unsupported action\"", false);
        return;
    }
    if (command == "move") {
        int vx = 0;
        int vy = 0;
        int vz = 0;
        iss >> vx >> vy >> vz;
        if (g_chassis != nullptr && g_chassis_ready) {
            g_chassis->SendVelocity(static_cast<int16_t>(vx), static_cast<int16_t>(vy), static_cast<int16_t>(vz));
        }
        print_debug_response("move", "\"chassis_ok\":" + std::string(g_chassis_ready ? "true" : "false") + ",\"vx\":" + std::to_string(vx) + ",\"vy\":" + std::to_string(vy) + ",\"vz\":" + std::to_string(vz));
        return;
    }
    if (command == "stop") {
        send_cli_chassis_action("stop");
        return;
    }

    print_debug_response(command.empty() ? "unknown" : command, "\"error\":\"unsupported command\"", false);
}

void keyboard_listener() {
    std::string input;
    std::cout << "Input 'q' to exit or A1_TEST ping/rps_snapshot/chassis_test/move commands..." << std::endl;
    while (!g_exit_flag && std::getline(std::cin, input)) {
        if (input == "q" || input == "Q") {
            g_exit_flag = 1;
            break;
        }
        if (input.rfind("A1_TEST", 0) == 0) {
            handle_a1_test_command(input);
        }
    }
}

void send_velocity_if_changed(ChassisController& chassis, bool chassis_ready,
                              const std::string& action, std::string& last_action) {
    if (!chassis_ready || action == last_action) return;
    const int16_t vx = vx_for_action(action);
    chassis.SendVelocity(vx, 0, 0);
    last_action = action;
    std::cout << "[RPS_CHASSIS] action=" << action << " vx=" << vx << std::endl;
}

}  // namespace

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::array<int, 2> camera_shape = {kCameraWidth, kCameraHeight};
    std::array<int, 2> cls_shape = {kClassifierInputWidth, kClassifierInputHeight};
    std::string model_path = "/app_demo/app_assets/models/model_rps.m1model";

    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
        return 1;
    }

    IMAGEPROCESSOR processor;
    if (!processor.Initialize(&camera_shape)) {
        fprintf(stderr, "Image pipeline initialization failed!\n");
        ssne_release();
        return 1;
    }

    RPS_CLASSIFIER classifier;
    if (!classifier.Initialize(model_path, &camera_shape, &cls_shape)) {
        fprintf(stderr, "RPS classifier initialization failed!\n");
        processor.Release();
        ssne_release();
        return 1;
    }

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();
    g_chassis = &chassis;
    g_chassis_ready = chassis_ready;
    if (!chassis_ready) {
        std::cout << "[RPS_CHASSIS] chassis unavailable, classification only" << std::endl;
    }

    std::thread listener_thread(keyboard_listener);

    ssne_tensor_t img_sensor;
    std::string last_action = "stop";
    uint64_t frame_index = 0;
    uint64_t last_summary_frame = 0;
    auto last_summary_log = std::chrono::steady_clock::now() - std::chrono::seconds(10);

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    while (!g_exit_flag) {
        ++frame_index;
        processor.GetImage(&img_sensor);

        std::string label;
        float confidence = 0.0f;
        float scores[kClassCount] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        classifier.Predict(&img_sensor, label, confidence, scores);
        const std::string action = action_for_label(label);

        update_latest_rps_snapshot(frame_index, g_latest_rps_snapshot.request_id, label, confidence, scores, action);
        send_velocity_if_changed(chassis, chassis_ready, action, last_action);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_summary_log >= std::chrono::seconds(2)) {
            const uint64_t frames_since_summary = frame_index - last_summary_frame;
            printf("[RPS] frame=%llu frames_2s=%llu label=%s conf=%.3f scores=[%.3f,%.3f,%.3f,%.3f,%.3f] action=%s\n",
                   static_cast<unsigned long long>(frame_index),
                   static_cast<unsigned long long>(frames_since_summary),
                   label.c_str(), confidence,
                   scores[0], scores[1], scores[2], scores[3], scores[4], action.c_str());
            last_summary_frame = frame_index;
            last_summary_log = now;
        }

        usleep(10000);
    }

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    classifier.Release();
    processor.Release();
    chassis.Release();
    g_chassis = nullptr;
    g_chassis_ready = false;

    if (listener_thread.joinable()) {
        listener_thread.detach();
    }

    ssne_release();
    return 0;
}
