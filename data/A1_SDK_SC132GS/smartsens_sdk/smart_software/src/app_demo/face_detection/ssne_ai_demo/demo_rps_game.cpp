#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/utils.hpp"

namespace {

constexpr int kCameraWidth = 720;
constexpr int kCameraHeight = 1280;
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

struct LatestYoloSnapshot {
    bool valid = false;
    uint64_t frame_index = 0;
    DetectionResult detections;
};

std::mutex g_latest_yolo_mutex;
std::condition_variable g_latest_yolo_cv;
LatestYoloSnapshot g_latest_yolo_snapshot;
volatile sig_atomic_t g_yolo_tensor_dump_requested = 0;
uint64_t g_yolo_tensor_dump_completed_frame = 0;

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
    const float x1 = std::max(0.0f, std::min(static_cast<float>(kCameraWidth), box[0]));
    const float y1 = std::max(0.0f, std::min(static_cast<float>(kCameraHeight), box[1]));
    const float x2 = std::max(0.0f, std::min(static_cast<float>(kCameraWidth), box[2]));
    const float y2 = std::max(0.0f, std::min(static_cast<float>(kCameraHeight), box[3]));
    const float width = std::max(0.0f, x2 - x1);
    const float height = std::max(0.0f, y2 - y1);
    const float area_ratio = (width * height) / static_cast<float>(kCameraWidth * kCameraHeight);
    const float bottom_ratio = y2 / static_cast<float>(kCameraHeight);
    return 0.65f * std::sqrt(area_ratio) + 0.35f * bottom_ratio;
}

void print_detection_summary(uint64_t frame_index, const DetectionResult& det_result) {
    printf("[YOLOV8] frame=%llu det_count=%zu\n",
           static_cast<unsigned long long>(frame_index), det_result.boxes.size());
    if (det_result.boxes.empty()) return;

    const int cls = det_result.class_ids.empty() ? -1 : det_result.class_ids[0];
    auto it = kClassNames.find(cls);
    const char* name = (it != kClassNames.end()) ? it->second.c_str() : "unknown";
    const float score = det_result.scores.empty() ? 0.0f : det_result.scores[0];
    const auto& box = det_result.boxes[0];
    printf("[YOLOV8] frame=%llu first_cls=%s score=%.3f box=[%.1f,%.1f,%.1f,%.1f]\n",
           static_cast<unsigned long long>(frame_index), name, score,
           box[0], box[1], box[2], box[3]);
}

void update_latest_yolo_snapshot(uint64_t frame_index, const DetectionResult& det_result) {
    {
        std::lock_guard<std::mutex> lock(g_latest_yolo_mutex);
        g_latest_yolo_snapshot.valid = true;
        g_latest_yolo_snapshot.frame_index = frame_index;
        g_latest_yolo_snapshot.detections = det_result;
        if (det_result.tensor_dump_printed) {
            g_yolo_tensor_dump_completed_frame = frame_index;
        }
    }
    g_latest_yolo_cv.notify_all();
}

std::string build_yolo_snapshot_json() {
    LatestYoloSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(g_latest_yolo_mutex);
        snapshot = g_latest_yolo_snapshot;
    }

    if (!snapshot.valid) {
        return "\"message\":\"no detection snapshot yet\"";
    }

    const DetectionResult& det = snapshot.detections;
    std::ostringstream body;
    body << std::fixed << std::setprecision(3);
    body << "\"frame\":" << snapshot.frame_index
         << ",\"count\":" << det.boxes.size()
         << ",\"camera_w\":" << kCameraWidth
         << ",\"camera_h\":" << kCameraHeight
         << ",\"threshold\":0.400"
         << ",\"raw_candidates\":" << det.raw_candidates
         << ",\"top_score\":" << det.top_score
         << ",\"top_class_id\":" << det.top_class_id
         << ",\"top_class\":\"" << json_escape(kClassNames.count(det.top_class_id) ? kClassNames.at(det.top_class_id) : "unknown") << "\""
         << ",\"preprocess_ok\":" << (det.preprocess_ok ? "true" : "false")
         << ",\"inference_ok\":" << (det.inference_ok ? "true" : "false")
         << ",\"input_dtype\":" << det.input_dtype
         << ",\"decoded_candidates\":" << det.decoded_candidates
         << ",\"after_nms_count\":" << det.after_nms_count
         << ",\"score_over_005\":" << det.score_over_005
         << ",\"score_over_010\":" << det.score_over_010
         << ",\"score_over_025\":" << det.score_over_025
         << ",\"score_over_040\":" << det.score_over_040
         << ",\"head_top_scores\":[" << det.head_top_scores[0] << "," << det.head_top_scores[1] << "," << det.head_top_scores[2] << "]"
         << ",\"head_top_classes\":[" << det.head_top_classes[0] << "," << det.head_top_classes[1] << "," << det.head_top_classes[2] << "]"
         << ",\"error_stage\":\"" << json_escape(det.error_stage) << "\""
         << ",\"error_code\":" << det.error_code
         << ",\"objects\":[";

    for (size_t i = 0; i < det.boxes.size(); ++i) {
        const int cls = i < det.class_ids.size() ? det.class_ids[i] : -1;
        auto it = kClassNames.find(cls);
        const std::string name = (it != kClassNames.end()) ? it->second : "unknown";
        const float score = i < det.scores.size() ? det.scores[i] : 0.0f;
        const auto& box = det.boxes[i];
        if (i > 0) body << ",";
        body << "{\"class_id\":" << cls
             << ",\"class\":\"" << json_escape(name) << "\""
             << ",\"score\":" << score
             << ",\"box\":[" << box[0] << "," << box[1] << "," << box[2] << "," << box[3] << "]}";
    }

    body << "],\"message\":\"latest detection snapshot\"";
    return body.str();
}

void emit_heuristic_depth_frame(uint64_t frame_index, const DetectionResult& det_result) {
    constexpr int kDepthWidth = 80;
    constexpr int kDepthHeight = 60;
    constexpr size_t kDepthBytes = kDepthWidth * kDepthHeight;
    constexpr size_t kMaxChunkChars = 1600;

    std::vector<uint8_t> depth(kDepthBytes, 0);
    uint32_t seed = static_cast<uint32_t>(frame_index * 2654435761u);
    for (size_t i = 0; i < depth.size(); ++i) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        depth[i] = static_cast<uint8_t>(20 + (seed % 200));
    }
    for (size_t i = 0; i < det_result.boxes.size(); ++i) {
        const auto& box = det_result.boxes[i];
        if (!is_valid_box(box)) continue;
        const float depth_score = compute_box_depth_score(box);
        const uint8_t value = depth_value_for_score(depth_score);
        const int x1 = std::max(0, std::min(kDepthWidth - 1, static_cast<int>(box[0] * kDepthWidth / kCameraWidth)));
        const int y1 = std::max(0, std::min(kDepthHeight - 1, static_cast<int>(box[1] * kDepthHeight / kCameraHeight)));
        const int x2 = std::max(0, std::min(kDepthWidth - 1, static_cast<int>(box[2] * kDepthWidth / kCameraWidth)));
        const int y2 = std::max(0, std::min(kDepthHeight - 1, static_cast<int>(box[3] * kDepthHeight / kCameraHeight)));
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
    if (command == "yolo_snapshot") {
        uint64_t request_after_frame = 0;
        {
            std::lock_guard<std::mutex> lock(g_latest_yolo_mutex);
            request_after_frame = g_latest_yolo_snapshot.frame_index;
            g_yolo_tensor_dump_requested = 1;
        }
        std::unique_lock<std::mutex> lock(g_latest_yolo_mutex);
        const bool ready = g_latest_yolo_cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return g_latest_yolo_snapshot.valid &&
                   g_yolo_tensor_dump_completed_frame > request_after_frame;
        });
        lock.unlock();
        if (!ready) {
            print_debug_response("yolo_snapshot", "\"message\":\"tensor dump timeout\"", false);
            return;
        }
        print_debug_response("yolo_snapshot", build_yolo_snapshot_json(), true);
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
    std::cout << "Input 'q' to exit or A1_TEST ping/yolo_snapshot/chassis_test/move commands..." << std::endl;
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

    std::array<int, 2> camera_shape = {kCameraWidth, kCameraHeight};
    std::array<int, 2> det_shape = {640, 640};
    std::string model_path = "/app_demo/app_assets/models/25d59a3b-fb19-4da2-8eb8-912bf18f05e6_best_head6.m1model";

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

    YOLOV8 detector;
    if (!detector.Initialize(model_path, &camera_shape, &det_shape)) {
        fprintf(stderr, "YOLOv8 initialization failed!\n");
        processor.Release();
        ssne_release();
        return 1;
    }

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
    uint64_t last_summary_frame = 0;
    auto last_depth_log = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    auto last_summary_log = std::chrono::steady_clock::now() - std::chrono::seconds(10);

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    while (!g_exit_flag) {
        ++frame_index;
        processor.GetImage(&img_sensor);

        const bool print_tensor_dump = g_yolo_tensor_dump_requested != 0;
        g_yolo_tensor_dump_requested = 0;
        detector.Predict(&img_sensor, &det_result, 0.4f, frame_index, print_tensor_dump);
        update_latest_yolo_snapshot(frame_index, det_result);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_summary_log >= std::chrono::seconds(10)) {
            const uint64_t frames_since_summary = frame_index - last_summary_frame;
            printf("[YOLOV8] summary frame=%llu frames_10s=%llu det_count=%zu raw=%d decoded=%d nms=%d top=%.3f cls=%d\n",
                   static_cast<unsigned long long>(frame_index),
                   static_cast<unsigned long long>(frames_since_summary),
                   det_result.boxes.size(), det_result.raw_candidates,
                   det_result.decoded_candidates, det_result.after_nms_count,
                   det_result.top_score, det_result.top_class_id);
            if (!det_result.boxes.empty()) {
                const int cls = det_result.class_ids.empty() ? -1 : det_result.class_ids[0];
                auto it = kClassNames.find(cls);
                const char* name = (it != kClassNames.end()) ? it->second.c_str() : "?";
                printf("[YOLOV8] summary first_cls=%s score=%.3f box=[%.1f,%.1f,%.1f,%.1f]\n",
                       name, det_result.scores[0],
                       det_result.boxes[0][0], det_result.boxes[0][1],
                       det_result.boxes[0][2], det_result.boxes[0][3]);
            }
            last_summary_frame = frame_index;
            last_summary_log = now;
        }

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
        if (now - last_depth_log >= std::chrono::seconds(1)) {
            emit_heuristic_depth_frame(frame_index, det_result);
            last_depth_log = now;
        }

        usleep(10000);
    }

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    detector.Release();
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
