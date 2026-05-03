#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <unistd.h>

#include "include/chassis_controller.hpp"
#include "include/utils.hpp"

namespace {

constexpr int kImageWidth = 1920;
constexpr int kImageHeight = 1080;
constexpr int16_t kForwardVelocity = 200;
constexpr int16_t kBackwardVelocity = -200;

struct OsdInfo {
    std::string filename;
    uint16_t x;
    uint16_t y;
};

struct GestureAction {
    std::string name;
    int16_t vx;
    const OsdInfo* osd;
};

volatile sig_atomic_t g_exit_flag = 0;
ChassisController* g_chassis = nullptr;
bool g_chassis_ready = false;
std::string g_last_cli_action = "stop";

void print_debug_response(const std::string& command, const std::string& body, bool success = true) {
    std::cout << "A1_DEBUG {\"command\":\"" << command << "\",\"success\":"
              << (success ? "true" : "false") << "," << body << "}" << std::endl;
}

void handle_signal(int) {
    g_exit_flag = 1;
}

void send_cli_chassis_action(const std::string& action) {
    int16_t vx = 0;
    std::string gesture = "P/NoTarget";
    if (action == "forward") {
        vx = kForwardVelocity;
        gesture = "R";
    } else if (action == "backward") {
        vx = kBackwardVelocity;
        gesture = "S";
    }

    if (g_chassis != nullptr && g_chassis_ready) {
        g_chassis->SendVelocity(vx, 0, 0);
        g_last_cli_action = action;
    }

    std::ostringstream body;
    body << "\"gesture\":\"" << gesture << "\",\"action\":\"" << action
         << "\",\"vx\":" << vx << ",\"chassis_ok\":" << (g_chassis_ready ? "true" : "false")
         << ",\"message\":\"R/rock=forward, S/scissors=backward, P/paper/no target=stop\"";
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
        print_debug_response("osd_status", "\"message\":\"RPS action OSD active\",\"layers\":[2,3]");
        return;
    }
    if (command == "chassis_test") {
        std::string action;
        iss >> action;
        if (action == "forward" || action == "backward" || action == "stop") {
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

GestureAction map_gesture_to_action(const std::string& label,
                                    const OsdInfo& rock_osd,
                                    const OsdInfo& paper_osd,
                                    const OsdInfo& scissors_osd,
                                    const OsdInfo& stop_osd) {
    if (label == "R") {
        return {"forward", kForwardVelocity, &rock_osd};
    }
    if (label == "S") {
        return {"backward", kBackwardVelocity, &scissors_osd};
    }
    if (label == "P") {
        return {"stop", 0, &paper_osd};
    }
    return {"stop", 0, &stop_osd};
}

void send_action_if_changed(ChassisController& chassis,
                            bool chassis_ready,
                            const GestureAction& action,
                            std::string& last_action) {
    if (!chassis_ready) {
        return;
    }
    if (action.name == last_action) {
        return;
    }
    chassis.SendVelocity(action.vx, 0, 0);
    last_action = action.name;
    std::cout << "[RPS_CHASSIS] action=" << action.name << " vx=" << action.vx << std::endl;
}

}  // namespace

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::array<int, 2> img_shape = {kImageWidth, kImageHeight};
    std::array<int, 2> cls_shape = {320, 320};
    std::string model_path = "/app_demo/app_assets/models/model_rps.m1model";

    const OsdInfo background_osd = {"background.ssbmp", 0, 0};
    const OsdInfo rock_osd = {"r.ssbmp", 960, 300};
    const OsdInfo paper_osd = {"p.ssbmp", 960, 300};
    const OsdInfo scissors_osd = {"s.ssbmp", 960, 300};
    const OsdInfo stop_osd = {"ready.ssbmp", 960, 270};

    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
        return 1;
    }

    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape);

    RPS_CLASSIFIER classifier;
    classifier.Initialize(model_path, &img_shape, &cls_shape);

    VISUALIZER visualizer;
    visualizer.Initialize(img_shape, "shared_colorLUT.sscl");
    visualizer.DrawBitmap(background_osd.filename, "shared_colorLUT.sscl", background_osd.x, background_osd.y, 2);
    visualizer.DrawBitmap(stop_osd.filename, "shared_colorLUT.sscl", stop_osd.x, stop_osd.y, 3);

    ChassisController chassis;
    const bool chassis_ready = chassis.Init();
    g_chassis = &chassis;
    g_chassis_ready = chassis_ready;
    if (!chassis_ready) {
        std::cout << "[RPS_CHASSIS] chassis unavailable, gesture display only" << std::endl;
    }

    std::thread listener_thread(keyboard_listener);

    ssne_tensor_t img_sensor;
    std::string last_action = "stop";
    std::string last_osd = stop_osd.filename;

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    while (!g_exit_flag) {
        processor.GetImage(&img_sensor);

        std::string label;
        float score = 0.0f;
        float scores[3] = {0.0f, 0.0f, 0.0f};
        classifier.Predict(&img_sensor, label, score, scores);

        GestureAction action = map_gesture_to_action(label, rock_osd, paper_osd, scissors_osd, stop_osd);
        send_action_if_changed(chassis, chassis_ready, action, last_action);

        if (action.osd != nullptr && action.osd->filename != last_osd) {
            visualizer.DrawBitmap(action.osd->filename, "shared_colorLUT.sscl", action.osd->x, action.osd->y, 3);
            last_osd = action.osd->filename;
        }

        std::cout << "[RPS] label=" << label
                  << " score=" << score
                  << " action=" << action.name
                  << " scores(P,R,S)=" << scores[0] << "," << scores[1] << "," << scores[2]
                  << std::endl;

        usleep(100000);
    }

    if (chassis_ready) {
        chassis.SendVelocity(0, 0, 0);
    }

    classifier.Release();
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
