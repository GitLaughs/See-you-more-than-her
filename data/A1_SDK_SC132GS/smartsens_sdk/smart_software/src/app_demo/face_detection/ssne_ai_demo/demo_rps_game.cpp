#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/utils.hpp"

namespace {

constexpr int kImageWidth = 1920;
constexpr int kImageHeight = 1280;
constexpr int16_t kForwardVelocity = 200;

constexpr int kClassPerson   = 0;
constexpr int kClassStop     = 1;
constexpr int kClassForward  = 2;
constexpr int kClassObstacle = 3;

const std::map<int, std::string> kClassNames = {
    {kClassPerson,   "person"},
    {kClassStop,     "stop"},
    {kClassForward,  "forward"},
    {kClassObstacle, "obstacle"},
};

volatile sig_atomic_t g_exit_flag = 0;
ChassisController* g_chassis = nullptr;
bool g_chassis_ready = false;
std::string g_last_cli_action = "stop";

std::string base64_encode(const uint8_t* data, size_t len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t value = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) {
            value |= static_cast<uint32_t>(data[i + 1]) << 8;
        }
        if (i + 2 < len) {
            value |= static_cast<uint32_t>(data[i + 2]);
        }
        out.push_back(table[(value >> 18) & 0x3f]);
        out.push_back(table[(value >> 12) & 0x3f]);
        out.push_back(i + 1 < len ? table[(value >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < len ? table[value & 0x3f] : '=');
    }
    return out;
}

const char* depth_bucket_name(float depth_score) {
    if (depth_score >= 0.55f) return "near";
    if (depth_score >= 0.30f) return "mid";
    return "far";
}

uint8_t depth_value_for_score(float depth_score) {
    if (depth_score >= 0.55f) return 240;
    if (depth_score >= 0.30f) return 160;
    return 80;
}

bool is_valid_box(const std::array<float, 4>& box) {
    return std::isfinite(box[0]) && std::isfinite(box[1]) &&
           std::isfinite(box[2]) && std::isfinite(box[3]);
}

float compute_box_depth_score(const std::array<float, 4>& box) {
    const float x1 = std::max(0.0f, std::min(static_cast<float>(kImageWidth), box[0]));
    const float y1 = std::max(0.0f, std::min(static_cast<float>(kImageHeight), box[1]));
    const float x2 = std::max(0.0f, std::min(static_cast<float>(kImageWidth), box[2]));
    const float y2 = std::max(0.0f, std::min(static_cast<float>(kImageHeight), box[3]));
    const float width = std::max(0.0f, x2 - x1);
    const float height = std::max(0.0f, y2 - y1);
    const float area_ratio = (width * height) / static_cast<float>(kImageWidth * kImageHeight);
    const float bottom_ratio = y2 / static_cast<float>(kImageHeight);
    return 0.65f * std::sqrt(area_ratio) + 0.35f * bottom_ratio;
}

void emit_heuristic_depth_frame(uint64_t frame_index, const DetectionResult& det_result) {
    constexpr int kDepthWidth = 80;
    constexpr int kDepthHeight = 60;
    constexpr size_t kDepthBytes = kDepthWidth * kDepthHeight;
    constexpr size_t kMaxChunkChars = 1600;

    std::vector<uint8_t> depth(kDepthBytes, 20);
    for (size_t i = 0; i < det_result.boxes.size(); ++i) {
        const auto& box = det_result.boxes[i];
        if (!is_valid_box(box)) continue;
        const float depth_score = compute_box_depth_score(box);
        const uint8_t value = depth_value_for_score(depth_score);
        const int x1 = std::max(0, std::min(kDepthWidth - 1, static_cast<int>(box[0] * kDepthWidth / kImageWidth)));
        const int y1 = std::max(0, std::min(kDepthHeight - 1, static_cast<int>(box[1] * kDepthHeight / kImageHeight)));
        const int x2 = std::max(0, std::min(kDepthWidth - 1, static_cast<int>(box[2] * kDepthWidth / kImageWidth)));
        const int y2 = std::max(0, std::min(kDepthHeight - 1, static_cast<int>(box[3] * kDepthHeight / kImageHeight)));
        for (int y = y1; y <= y2; ++y) {
            for (int x = x1; x <= x2; ++x) {
                uint8_t& pixel = depth[y * kDepthWidth + x];
                if (value > pixel) pixel = value;
            }
        }
    }

    const std::string encoded = base64_encode(depth.data(), depth.size());
    const size_t chunks = (encoded.size() + kMaxChunkChars - 1) / kMaxChunkChars;
    printf("A1_DEPTH_BEGIN frame=%llu w=%d h=%d fmt=u8 encoding=base64 chunks=%zu bytes=%zu\n",
           static_cast<unsigned long long>(frame_index), kDepthWidth, kDepthHeight, chunks, kDepthBytes);
    for (size_t i = 0; i < chunks; ++i) {
        const size_t offset = i * kMaxChunkChars;
        printf("A1_DEPTH_CHUNK frame=%llu index=%zu data=%s\n",
               static_cast<unsigned long long>(frame_index), i, encoded.substr(offset, kMaxChunkChars).c_str());
    }
    printf("A1_DEPTH_END frame=%llu\n", static_cast<unsigned long long>(frame_index));

    for (size_t i = 0; i < det_result.boxes.size(); ++i) {
        const int cls = i < det_result.class_ids.size() ? det_result.class_ids[i] : -1;
        if (!is_valid_box(det_result.boxes[i])) continue;
        const float score = i < det_result.scores.size() ? det_result.scores[i] : 0.0f;
        const float depth_score = compute_box_depth_score(det_result.boxes[i]);
        const char* bucket = depth_bucket_name(depth_score);
        auto it = kClassNames.find(cls);
        const char* name = (it != kClassNames.end()) ? it->second.c_str() : "unknown";
        printf("A1_DEPTH_OBJECT frame=%llu cls=%s score=%.3f bucket=%s depth=%.3f box=%.1f,%.1f,%.1f,%.1f\n",
               static_cast<unsigned long long>(frame_index), name, score, bucket, depth_score,
               det_result.boxes[i][0], det_result.boxes[i][1], det_result.boxes[i][2], det_result.boxes[i][3]);
    }
    fflush(stdout);
}

void print_debug_response(const std::string& command, const std::string& body, bool success = true) {
    std::cout << "A1_DEBUG {\"command\":\"" << command << "\",\"success\":"
              << (success ? "true" : "false") << "," << body << "}" << std::endl;
}

void handle_signal(int) {
    g_exit_flag = 1;
}

void send_cli_chassis_action(const std::string& action) {
    int16_t vx = 0;
    if (action == "forward") {
        vx = kForwardVelocity;
    }

    if (g_chassis != nullptr && g_chassis_ready) {
        g_chassis->SendVelocity(vx, 0, 0);
        g_last_cli_action = action;
    }

    std::ostringstream body;
    body << "\"action\":\"" << action
         << "\",\"vx\":" << vx << ",\"chassis_ok\":" << (g_chassis_ready ? "true" : "false")
         << ",\"message\":\"forward=forward, stop=stop\"";
    print_debug_response("chassis_test", body.str(), true);
}

void handle_a1_test_command(const std::string& line) {
    std::istringstream iss(line);
    std::string prefix;
    std::string command;
    iss >> prefix >> command;
    if (prefix != "A1_TEST") {
        return;
    }

    if (command == "ping") {
        print_debug_response("ping", "\"message\":\"pong\",\"chassis_ok\":" + std::string(g_chassis_ready ? "true" : "false"));
        return;
    }
    if (command == "debug_status" || command == "uart_status") {
        print_debug_response(command, "\"chassis_ok\":" + std::string(g_chassis_ready ? "true" : "false") + ",\"last_action\":\"" + g_last_cli_action + "\"");
        return;
    }
    if (command == "osd_status") {
        print_debug_response("osd_status", "\"message\":\"background OSD active\",\"layers\":[2]");
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
    std::cout << "Input 'q' to exit or A1_TEST commands..." << std::endl;
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
                              int16_t vx, std::string& last_action) {
    if (!chassis_ready) return;
    std::string action = (vx > 0) ? "forward" : "stop";
    if (action == last_action) return;
    chassis.SendVelocity(vx, 0, 0);
    last_action = action;
    std::cout << "[YOLOV8_CHASSIS] action=" << action << " vx=" << vx << std::endl;
}

}  // namespace

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::array<int, 2> img_shape = {kImageWidth, kImageHeight};
    std::array<int, 2> det_shape = {640, 640};
    std::string model_path = "/app_demo/app_assets/models/25d59a3b-fb19-4da2-8eb8-912bf18f05e6_best_head6.m1model";

    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
        return 1;
    }

    IMAGEPROCESSOR processor;
    if (!processor.Initialize(&img_shape)) {
        fprintf(stderr, "Image pipeline initialization failed!\n");
        ssne_release();
        return 1;
    }

    YOLOV8 detector;
    if (!detector.Initialize(model_path, &img_shape, &det_shape)) {
        fprintf(stderr, "YOLOv8 initialization failed!\n");
        processor.Release();
        ssne_release();
        return 1;
    }

    VISUALIZER visualizer;
    if (!visualizer.Initialize(img_shape, "background_colorLUT.sscl")) {
        fprintf(stderr, "OSD initialization failed!\n");
        detector.Release();
        processor.Release();
        ssne_release();
        return 1;
    }
    visualizer.DrawBitmap("background.ssbmp", "background_colorLUT.sscl", 0, 0, 2);

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();
    g_chassis = &chassis;
    g_chassis_ready = chassis_ready;
    if (!chassis_ready) {
        std::cout << "[YOLOV8_CHASSIS] chassis unavailable, detection only" << std::endl;
    }

    std::thread listener_thread(keyboard_listener);

    ssne_tensor_t img_sensor;
    DetectionResult det_result;
    std::string last_action = "stop";
    uint64_t frame_index = 0;
    auto last_depth_log = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    while (!g_exit_flag) {
        ++frame_index;
        processor.GetImage(&img_sensor);

        detector.Predict(&img_sensor, &det_result, 0.4f);

        bool has_obstacle = false, has_person = false, has_stop = false, has_forward = false;
        for (size_t i = 0; i < det_result.class_ids.size(); ++i) {
            switch (det_result.class_ids[i]) {
                case kClassObstacle: has_obstacle = true; break;
                case kClassPerson:   has_person   = true; break;
                case kClassStop:     has_stop     = true; break;
                case kClassForward:  has_forward  = true; break;
            }
        }

        int16_t vx = 0;
        if (has_obstacle || has_person || has_stop) {
            vx = 0;
        } else if (has_forward) {
            vx = kForwardVelocity;
        }
        send_velocity_if_changed(chassis, chassis_ready, vx, last_action);

        for (size_t i = 0; i < det_result.class_ids.size(); ++i) {
            int cls = det_result.class_ids[i];
            auto it = kClassNames.find(cls);
            const char* name = (it != kClassNames.end()) ? it->second.c_str() : "?";
            printf("[YOLOV8] class=%s score=%.3f box=[%.1f,%.1f,%.1f,%.1f]\n",
                   name, det_result.scores[i],
                   det_result.boxes[i][0], det_result.boxes[i][1],
                   det_result.boxes[i][2], det_result.boxes[i][3]);
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_depth_log >= std::chrono::seconds(1)) {
            emit_heuristic_depth_frame(frame_index, det_result);
            last_depth_log = now;
        }

        visualizer.Draw(det_result.boxes);

        usleep(100000);
    }

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    detector.Release();
    processor.Release();
    visualizer.Release();
    chassis.Release();
    g_chassis = nullptr;
    g_chassis_ready = false;

    if (listener_thread.joinable()) {
        listener_thread.detach();
    }

    ssne_release();
    return 0;
}
