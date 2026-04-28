/*
 * @Filename: demo_face.cpp
 * @Description: A1 SC132GS demo runtime with detector, OSD, chassis UART and A1_TEST CLI.
 */

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "include/chassis_controller.hpp"
#include "include/gpio_test_runner.hpp"
#include "include/utils.hpp"
#include "project_paths.hpp"

namespace {

struct RuntimeState {
    bool exit_requested = false;
    bool link_test_enabled = cfg::LINK_TEST_ENABLED;
    bool manual_active = false;
    int16_t manual_vx = 0;
    int16_t manual_vy = 0;
    int16_t manual_vz = 0;
    uint64_t manual_until_us = 0;
    uint64_t frame_count = 0;
    uint64_t detection_count = 0;
    std::string backend = cfg::USE_SCRFD_BACKEND ? "scrfd_gray" : "yolov8_gray";
    std::string last_command = "boot";
    bool chassis_ready = false;
};

RuntimeState g_state;
std::mutex g_state_mtx;

uint64_t monotonic_time_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000ULL +
           static_cast<uint64_t>(tv.tv_usec);
}

std::vector<std::string> split_words(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> words;
    std::string word;
    while (iss >> word) {
        words.push_back(word);
    }
    return words;
}

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            default: out += ch; break;
        }
    }
    return out;
}

void print_json(const std::string& body) {
    std::cout << "{" << body << "}" << std::endl;
}

void print_status_locked() {
    std::ostringstream oss;
    oss << "\"success\":true"
        << ",\"type\":\"status\""
        << ",\"command\":\"debug_status\""
        << ",\"backend\":\"" << json_escape(g_state.backend) << "\""
        << ",\"frames\":" << g_state.frame_count
        << ",\"detections\":" << g_state.detection_count
        << ",\"link_test\":" << (g_state.link_test_enabled ? "true" : "false")
        << ",\"manual_active\":" << (g_state.manual_active ? "true" : "false")
        << ",\"chassis_ready\":" << (g_state.chassis_ready ? "true" : "false")
        << ",\"last_command\":\"" << json_escape(g_state.last_command) << "\"";
    print_json(oss.str());
}

bool parse_i16(const std::string& value, int16_t* out) {
    char* end = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    parsed = std::max<long>(-32768, std::min<long>(32767, parsed));
    *out = static_cast<int16_t>(parsed);
    return true;
}

void handle_command(const std::string& raw_line) {
    std::vector<std::string> words = split_words(raw_line);
    if (words.empty()) return;

    if (words[0] == "q" || words[0] == "Q" || words[0] == "exit" || words[0] == "quit") {
        std::lock_guard<std::mutex> lock(g_state_mtx);
        g_state.exit_requested = true;
        g_state.last_command = raw_line;
        print_json("\"success\":true,\"type\":\"exit\"");
        return;
    }

    if (words[0] == "A1_TEST") {
        words.erase(words.begin());
    }
    if (words.empty()) {
        print_json("\"success\":false,\"error\":\"missing_command\"");
        return;
    }

    const std::string& cmd = words[0];
    std::lock_guard<std::mutex> lock(g_state_mtx);
    g_state.last_command = raw_line;

    if (cmd == "help") {
        print_json("\"success\":true,\"command\":\"help\",\"commands\":[\"help\",\"status\",\"test_echo\",\"debug_status\",\"debug_frame\",\"debug_last\",\"link_test on|off\",\"stop\",\"move vx vy vz\"]");
    } else if (cmd == "status" || cmd == "debug_status") {
        print_status_locked();
    } else if (cmd == "debug_frame" || cmd == "debug_last") {
        std::ostringstream oss;
        oss << "\"success\":true,\"command\":\"" << cmd << "\""
            << ",\"frames\":" << g_state.frame_count
            << ",\"detections\":" << g_state.detection_count
            << ",\"backend\":\"" << json_escape(g_state.backend) << "\"";
        print_json(oss.str());
    } else if (cmd == "test_echo") {
        print_json("\"success\":true,\"type\":\"test_echo\",\"command\":\"test_echo\",\"echo\":\"A1_TEST\",\"message\":\"测试回传成功\"");
    } else if (cmd == "link_test") {
        if (words.size() < 2) {
            print_json("\"success\":false,\"error\":\"usage: A1_TEST link_test on|off\"");
            return;
        }
        g_state.link_test_enabled = (words[1] == "on" || words[1] == "1" || words[1] == "true");
        g_state.manual_active = false;
        print_json(std::string("\"success\":true,\"type\":\"link_test\",\"command\":\"link_test\",\"enabled\":") +
                   (g_state.link_test_enabled ? "true" : "false"));
    } else if (cmd == "stop") {
        g_state.link_test_enabled = false;
        g_state.manual_active = true;
        g_state.manual_vx = 0;
        g_state.manual_vy = 0;
        g_state.manual_vz = 0;
        g_state.manual_until_us = 0;
        print_json("\"success\":true,\"type\":\"stop\",\"command\":\"stop\"");
    } else if (cmd == "move") {
        if (words.size() < 4) {
            print_json("\"success\":false,\"error\":\"usage: A1_TEST move vx vy vz\"");
            return;
        }
        int16_t vx = 0, vy = 0, vz = 0;
        if (!parse_i16(words[1], &vx) || !parse_i16(words[2], &vy) || !parse_i16(words[3], &vz)) {
            print_json("\"success\":false,\"error\":\"invalid_velocity\"");
            return;
        }
        g_state.link_test_enabled = false;
        g_state.manual_active = true;
        g_state.manual_vx = vx;
        g_state.manual_vy = vy;
        g_state.manual_vz = vz;
        g_state.manual_until_us = monotonic_time_us() + 2000000ULL;
        std::ostringstream oss;
        oss << "\"success\":true,\"type\":\"move\",\"command\":\"move\",\"vx\":" << vx
            << ",\"vy\":" << vy << ",\"vz\":" << vz;
        print_json(oss.str());
    } else {
        print_json("\"success\":false,\"error\":\"unknown_command\"");
    }
}

void stdin_listener() {
    std::string line;
    std::cout << "[A1_TEST] ready. Type A1_TEST help for commands." << std::endl;
    while (std::getline(std::cin, line)) {
        handle_command(line);
        std::lock_guard<std::mutex> lock(g_state_mtx);
        if (g_state.exit_requested) break;
    }
}

RuntimeState snapshot_state() {
    std::lock_guard<std::mutex> lock(g_state_mtx);
    return g_state;
}

bool should_exit() {
    std::lock_guard<std::mutex> lock(g_state_mtx);
    return g_state.exit_requested;
}

void update_frame_stats(size_t detection_count) {
    std::lock_guard<std::mutex> lock(g_state_mtx);
    g_state.frame_count += 1;
    g_state.detection_count = detection_count;
}

void set_chassis_ready(bool ready) {
    std::lock_guard<std::mutex> lock(g_state_mtx);
    g_state.chassis_ready = ready;
}

void select_velocity(const RuntimeState& state, bool has_detection,
                     int16_t* vx, int16_t* vy, int16_t* vz) {
    *vx = 0;
    *vy = 0;
    *vz = 0;

    const uint64_t now_us = monotonic_time_us();
    if (state.manual_active) {
        if (state.manual_until_us == 0 || now_us <= state.manual_until_us) {
            *vx = state.manual_vx;
            *vy = state.manual_vy;
            *vz = state.manual_vz;
        }
        return;
    }

    if (state.link_test_enabled) {
        const uint64_t phase = now_us % cfg::LINK_TEST_PERIOD_US;
        if (phase < cfg::LINK_TEST_FORWARD_WINDOW_US) {
            *vx = cfg::LINK_TEST_FORWARD_VX;
        }
        return;
    }

    if (has_detection) {
        *vx = cfg::VX_FORWARD;
    }
}

}  // namespace

std::vector<std::string> collect_args(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc > 1 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return args;
}

int main(int argc, char** argv) {
    const std::vector<std::string> args = collect_args(argc, argv);
    if (!args.empty() && args[0] == "--gpio-test") {
        GpioTestConfig gpio_test_config;
        std::string error_message;
        const std::vector<std::string> gpio_args(args.begin() + 1, args.end());
        if (!ParseGpioTestArgs(gpio_args, &gpio_test_config, &error_message)) {
            std::fprintf(stderr, "[gpio_test] %s\n", error_message.c_str());
            return 2;
        }
        return RunGpioTest(gpio_test_config);
    }
    std::array<int, 2> img_shape = {cfg::SENSOR_WIDTH, cfg::SENSOR_HEIGHT};
    std::array<int, 2> det_shape = {cfg::DET_WIDTH, cfg::DET_HEIGHT};
    std::array<int, 2> crop_shape = {cfg::PIPE_CROP_WIDTH, cfg::PIPE_CROP_HEIGHT};
    std::string model_path = cfg::MODEL_PATH;

    if (ssne_initial()) {
        std::fprintf(stderr, "[A1] SSNE initialization failed\n");
        return 1;
    }

    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape);

    SCRFDGRAY scrfd_detector;
    YOLOV8 yolo_detector;
    if (cfg::USE_SCRFD_BACKEND) {
        int box_len = det_shape[0] * det_shape[1] / 512 * 21;
        scrfd_detector.Initialize(model_path, &crop_shape, &det_shape, false, box_len);
    } else {
        yolo_detector.Initialize(model_path, &img_shape, &det_shape);
    }

    VISUALIZER visualizer;
    visualizer.Initialize(img_shape);

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();
    set_chassis_ready(chassis_ready);
    if (!chassis_ready) {
        std::fprintf(stderr, "[A1] chassis UART unavailable; detection and OSD will continue\n");
    }

    FaceDetectionResult det_result;
    ssne_tensor_t img_sensor;
    std::thread listener_thread(stdin_listener);

    uint64_t last_velocity_us = 0;
    int16_t last_vx = 0;
    int16_t last_vy = 0;
    int16_t last_vz = 0;

    while (!should_exit()) {
        processor.GetImage(&img_sensor);

        if (cfg::USE_SCRFD_BACKEND) {
            scrfd_detector.Predict(&img_sensor, &det_result, cfg::DET_CONF_THRESH);
        } else {
            yolo_detector.Predict(&img_sensor, &det_result, cfg::DET_CONF_THRESH);
        }

        std::vector<std::array<float, 4>> osd_boxes = det_result.boxes;
        if (cfg::USE_SCRFD_BACKEND) {
            for (auto& box : osd_boxes) {
                box[1] += static_cast<float>(cfg::PIPE_CROP_Y1);
                box[3] += static_cast<float>(cfg::PIPE_CROP_Y1);
            }
        }
        visualizer.Draw(osd_boxes);
        update_frame_stats(det_result.boxes.size());

        RuntimeState state = snapshot_state();
        int16_t vx = 0, vy = 0, vz = 0;
        select_velocity(state, !det_result.boxes.empty(), &vx, &vy, &vz);

        const uint64_t now_us = monotonic_time_us();
        if (chassis_ready && (now_us - last_velocity_us >= 100000ULL ||
                              vx != last_vx || vy != last_vy || vz != last_vz)) {
            chassis.SendVelocity(vx, vy, vz);
            last_velocity_us = now_us;
            last_vx = vx;
            last_vy = vy;
            last_vz = vz;
        }
    }

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    if (listener_thread.joinable()) {
        listener_thread.join();
    }

    if (cfg::USE_SCRFD_BACKEND) {
        scrfd_detector.Release();
    } else {
        yolo_detector.Release();
    }
    chassis.Release();
    processor.Release();
    visualizer.Release();

    if (ssne_release()) {
        std::fprintf(stderr, "[A1] SSNE release failed\n");
        return 1;
    }
    return 0;
}
