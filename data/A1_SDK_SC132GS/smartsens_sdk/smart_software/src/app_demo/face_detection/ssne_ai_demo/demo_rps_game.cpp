/**
 * demo_rps_game.cpp — A1 板端 5 分类视觉导航主程序
 *
 * 功能：
 *   1. 初始化 SSNE 推理引擎，加载 5 分类 MobileNet 模型（test.m1model）
 *   2. 通过 Online Pipeline 采集摄像头 720×1280 Y8 灰度图像
 *   3. 中心裁剪至 320×320，送入分类器推理
 *   4. 根据分类结果控制 STM32 底盘（forward 前进，其余停止）
 *   5. 通过 stdin 监听 A1_TEST 命令（ping / rps_snapshot / depth_snapshot / chassis_test / move / stop）
 *   6. 定时发送模拟深度帧（A1_DEPTH），用于 Aurora 联调
 *
 * 类别标签：person / stop / forward / obstacle / NoTarget（5 类）
 * 推理阈值：置信度 ≥ 0.6 输出有效类别，否则归为 NoTarget
 * 底盘动作：forward → 前进（vx=200），其余 → 停止
 *
 */

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
#include <vector>

#include "include/chassis_controller.hpp"
#include "include/rps_classifier.hpp"
#include "include/utils.hpp"

namespace {

// ---- 常量定义 ----
constexpr int kCameraWidth = 720;            // 摄像头图像宽度
constexpr int kCameraHeight = 1280;          // 摄像头图像高度
constexpr int kClassifierInputWidth = 320;   // 分类器输入宽度（中心裁剪后）
constexpr int kClassifierInputHeight = 320;  // 分类器输入高度（中心裁剪后）
constexpr int kClassCount = 5;               // 分类类别数
constexpr int16_t kForwardVelocity = 200;    // 前进速度（mm/s）
constexpr int kDepthWidth = 80;              // 深度帧宽度
constexpr int kDepthHeight = 60;             // 深度帧高度
constexpr int kDepthChunkChars = 960;        // 深度帧 Base64 分片字符数
constexpr int kDepthAutoIntervalMs = 1000;   // 深度帧自动发送间隔（ms）
constexpr const char* kLabels[kClassCount] = {"person", "stop", "forward", "obstacle", "NoTarget"};

volatile sig_atomic_t g_exit_flag = 0;
ChassisController* g_chassis = nullptr;
bool g_chassis_ready = false;
std::string g_last_cli_action = "stop";
uint64_t g_depth_frame_index = 0;

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

// ---- JSON 字符串转义 ----
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

// ---- Base64 编码（用于深度帧传输） ----
std::string base64_encode(const uint8_t* data, size_t len) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = (i + 1 < len) ? data[i + 1] : 0;
        const uint32_t b2 = (i + 2 < len) ? data[i + 2] : 0;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
        out.push_back(table[(triple >> 18) & 0x3F]);
        out.push_back(table[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? table[(triple >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? table[triple & 0x3F] : '=');
    }
    return out;
}

// ---- 深度帧构造（模拟数据，用于 Aurora 联调验证深度传输通道） ----
// 生成 80×60 的模拟深度图，包含动态波纹和径向渐变，模拟真实深度传感器输出
std::vector<uint8_t> build_fake_depth_frame(uint64_t frame_index) {
    std::vector<uint8_t> depth(kDepthWidth * kDepthHeight);
    const int cx = kDepthWidth / 2 + static_cast<int>((frame_index % 21) - 10);
    const int cy = kDepthHeight / 2;
    for (int y = 0; y < kDepthHeight; ++y) {
        for (int x = 0; x < kDepthWidth; ++x) {
            const int dx = x - cx;
            const int dy = y - cy;
            const int dist = dx * dx + dy * dy;
            const int wave = static_cast<int>((x * 3 + y * 5 + frame_index * 7) & 0x3F);
            int value = 40 + ((x * 180) / kDepthWidth) + wave;
            if (dist < 180) value = 230 - dist / 2;
            if (value < 0) value = 0;
            if (value > 255) value = 255;
            depth[y * kDepthWidth + x] = static_cast<uint8_t>(value);
        }
    }
    return depth;
}

void emit_depth_frame(uint64_t depth_frame_index) {
    const std::vector<uint8_t> depth = build_fake_depth_frame(depth_frame_index);
    const std::string encoded = base64_encode(depth.data(), depth.size());
    const int chunks = static_cast<int>((encoded.size() + kDepthChunkChars - 1) / kDepthChunkChars);
    std::cout << "A1_DEPTH_BEGIN frame=" << depth_frame_index
              << " w=" << kDepthWidth
              << " h=" << kDepthHeight
              << " fmt=u8 encoding=base64 chunks=" << chunks
              << " bytes=" << depth.size() << std::endl;
    for (int i = 0; i < chunks; ++i) {
        const size_t start = static_cast<size_t>(i) * kDepthChunkChars;
        std::cout << "A1_DEPTH_CHUNK frame=" << depth_frame_index
                  << " index=" << i
                  << " data=" << encoded.substr(start, kDepthChunkChars) << std::endl;
    }
    std::cout << "A1_DEPTH_OBJECT frame=" << depth_frame_index
              << " cls=fake score=1.00 bucket=mid depth=1.20 box=0.35,0.35,0.30,0.30" << std::endl;
    std::cout << "A1_DEPTH_END frame=" << depth_frame_index << std::endl;
}

// ---- 分类标签 → 底盘动作映射 ----
// 仅 "forward" 映射为前进，其余所有类别映射为停止
std::string action_for_label(const std::string& label) {
    return label == "forward" ? "forward" : "stop";
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
         << ",\"scores\":[" << snapshot.scores[0] << "," << snapshot.scores[1] << "," << snapshot.scores[2] << "," << snapshot.scores[3] << "," << snapshot.scores[4] << "]"
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

// ---- A1_TEST 命令处理 ----
// 通过 stdin 接收 Aurora 端发送的命令：
//   A1_TEST ping             — 连接测试
//   A1_TEST test_echo <msg>  — 回声测试
//   A1_TEST depth_snapshot   — 发射一帧模拟深度数据
//   A1_TEST rps_snapshot <id>— 获取最新分类快照（等待最多 3 秒）
//   A1_TEST chassis_test <a> — 底盘动作测试（forward/stop）
//   A1_TEST move <vx> <vy> <vz> — 直接底盘速度控制
//   A1_TEST stop             — 紧急停止
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
    if (command == "test_echo") {
        std::string message;
        std::getline(iss, message);
        if (!message.empty() && message[0] == ' ') message.erase(0, 1);
        if (message.empty()) message = "pc_frontend_test";
        print_debug_response("test_echo", "\"message\":\"测试回传成功: " + json_escape(message) + "\",\"chassis_ok\":" + std::string(g_chassis_ready ? "true" : "false"));
        return;
    }
    if (command == "depth_snapshot") {
        emit_depth_frame(++g_depth_frame_index);
        print_debug_response("depth_snapshot", "\"message\":\"depth frame emitted\",\"frame\":" + std::to_string(g_depth_frame_index));
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
    std::cout << "Input 'q' to exit or A1_TEST ping/test_echo/depth_snapshot/rps_snapshot/chassis_test/move commands..." << std::endl;
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

// ============================================================
// 主函数：初始化 → 推理循环 → 清理
// ============================================================
int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::array<int, 2> camera_shape = {kCameraWidth, kCameraHeight};
    std::array<int, 2> cls_shape = {kClassifierInputWidth, kClassifierInputHeight};
    std::string model_path = "/app_demo/app_assets/models/test.m1model";
    std::cout << "[APP] classifier_model=" << model_path << std::endl;

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
    auto last_depth_emit = std::chrono::steady_clock::now() - std::chrono::milliseconds(kDepthAutoIntervalMs);

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    // ---- 主推理循环（200ms 间隔） ----
    while (!g_exit_flag) {
        ++frame_index;
        processor.GetImage(&img_sensor);  // 从摄像头获取一帧 Y8 灰度图

        std::string label;
        float confidence = 0.0f;
        float scores[kClassCount] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        classifier.Predict(&img_sensor, label, confidence, scores);
        const std::string action = action_for_label(label);

        update_latest_rps_snapshot(frame_index, g_latest_rps_snapshot.request_id, label, confidence, scores, action);
        send_velocity_if_changed(chassis, chassis_ready, action, last_action);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_depth_emit >= std::chrono::milliseconds(kDepthAutoIntervalMs)) {
            emit_depth_frame(++g_depth_frame_index);
            last_depth_emit = now;
        }
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

        usleep(200000);
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
