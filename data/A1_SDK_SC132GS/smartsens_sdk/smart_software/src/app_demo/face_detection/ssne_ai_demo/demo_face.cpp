#include <fstream>
#include <iostream>
#include <cstring>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <fcntl.h>
#include <regex>
#include <dirent.h>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/time.h>
#include "include/utils.hpp"
#include "include/chassis_controller.hpp"
#include "project_paths.hpp"

using namespace std;

// 全局退出标志（线程安全）
bool g_exit_flag = false;
// 保护退出标志的互斥锁
std::mutex g_mtx;
std::mutex g_debug_mtx;

struct DebugRuntimeState {
    uint64_t frame_index = 0;
    size_t det_count = 0;
    size_t person_count = 0;
    size_t forward_count = 0;
    size_t stop_count = 0;
    size_t obstacle_count = 0;
    size_t backward_count = 0;
    bool chassis_ok = false;
    bool link_test_enabled = cfg::LINK_TEST_ENABLED;
    bool link_test_forward = false;
};

DebugRuntimeState g_debug_state;
bool g_link_test_enabled = cfg::LINK_TEST_ENABLED;
ChassisController* g_chassis = nullptr;

std::string trim_copy(const std::string& input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::string json_escape(const std::string& text) {
    std::ostringstream oss;
    for (const unsigned char ch : text) {
        switch (ch) {
            case '\\': oss << "\\\\"; break;
            case '"':  oss << "\\\""; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << ch; break;
        }
    }
    return oss.str();
}

std::vector<std::string> split_tokens(const std::string& input) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool input_has_command(const std::string& input, const std::string& command) {
    return input == command ||
           input.find(" " + command) != std::string::npos ||
           input.find("A1_TEST " + command) != std::string::npos;
}

DebugRuntimeState snapshot_debug_state() {
    std::lock_guard<std::mutex> lock(g_debug_mtx);
    return g_debug_state;
}

void print_debug_snapshot(const std::string& command) {
    const DebugRuntimeState s = snapshot_debug_state();
    std::cout
        << "{\"success\":true,"
        << "\"channel\":\"serial\","
        << "\"command\":\"" << command << "\","
        << "\"frame_index\":" << s.frame_index << ","
        << "\"det_count\":" << s.det_count << ","
        << "\"person_count\":" << s.person_count << ","
        << "\"forward_count\":" << s.forward_count << ","
        << "\"stop_count\":" << s.stop_count << ","
        << "\"obstacle_count\":" << s.obstacle_count << ","
        << "\"backward_count\":" << s.backward_count << ","
        << "\"chassis_ok\":" << (s.chassis_ok ? "true" : "false") << ","
        << "\"link_test_enabled\":" << (s.link_test_enabled ? "true" : "false") << ","
        << "\"link_test_forward\":" << (s.link_test_forward ? "true" : "false")
        << "}" << std::endl;
}

bool handle_console_command(const std::string& raw_input) {
    const std::string input = trim_copy(raw_input);
    if (input.empty()) {
        return false;
    }
    if (input == "q" || input == "Q") {
        std::lock_guard<std::mutex> lock(g_mtx);
        g_exit_flag = true;
        std::cout << "检测到退出指令，通知主线程退出..." << std::endl;
        return true;
    }

    if (input == "help" || input == "HELP") {
        std::cout
            << "[A1_SERIAL] 可用命令: help | status | A1_TEST test_echo <msg> | "
            << "A1_TEST debug_status | A1_TEST debug_frame | A1_TEST debug_last | "
            << "A1_TEST link_test on/off | A1_TEST move <vx> <vy> <vz> | A1_TEST stop | q"
            << std::endl;
        return false;
    }

    if (input == "status" || input == "STATUS") {
        std::cout
            << "{\"success\":true,"
            << "\"channel\":\"serial\","
            << "\"message\":\"A1 串口调试在线\","
            << "\"sensor_width\":" << cfg::SENSOR_WIDTH << ","
            << "\"sensor_height\":" << cfg::SENSOR_HEIGHT << ","
            << "\"link_test_enabled\":" << (g_link_test_enabled ? "true" : "false")
            << "}" << std::endl;
        return false;
    }

    if (input_has_command(input, "debug_status") ||
        input_has_command(input, "debug_frame") ||
        input_has_command(input, "debug_last")) {
        const std::vector<std::string> tokens = split_tokens(input);
        std::string command = "debug_status";
        for (const std::string& token : tokens) {
            if (token == "debug_frame" || token == "debug_last" || token == "debug_status") {
                command = token;
                break;
            }
        }
        print_debug_snapshot(command);
        return false;
    }

    if (input_has_command(input, "link_test") ||
        input_has_command(input, "link_test_on") ||
        input_has_command(input, "link_test_off")) {
        const bool enable = input.find(" off") == std::string::npos &&
                            input.find("_off") == std::string::npos;
        {
            std::lock_guard<std::mutex> lock(g_debug_mtx);
            g_link_test_enabled = enable;
            g_debug_state.link_test_enabled = enable;
        }
        std::cout
            << "{\"success\":true,"
            << "\"channel\":\"serial\","
            << "\"command\":\"link_test\","
            << "\"enabled\":" << (enable ? "true" : "false") << ","
            << "\"message\":\"link_test 已" << (enable ? "开启" : "关闭") << "\"}"
            << std::endl;
        return false;
    }

    if (input_has_command(input, "stop")) {
        {
            std::lock_guard<std::mutex> lock(g_debug_mtx);
            g_link_test_enabled = false;
            g_debug_state.link_test_enabled = false;
            g_debug_state.link_test_forward = false;
        }
        if (g_chassis != nullptr && g_chassis->is_connected()) {
            g_chassis->SendVelocity(cfg::VX_STOP, 0, 0);
        }
        std::cout
            << "{\"success\":true,"
            << "\"channel\":\"serial\","
            << "\"command\":\"stop\","
            << "\"message\":\"已发送停车指令\"}"
            << std::endl;
        return false;
    }

    if (input_has_command(input, "move")) {
        const std::vector<std::string> tokens = split_tokens(input);
        int move_index = -1;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "move") {
                move_index = static_cast<int>(i);
                break;
            }
        }
        if (move_index >= 0 && move_index + 3 < static_cast<int>(tokens.size())) {
            const int vx = std::atoi(tokens[move_index + 1].c_str());
            const int vy = std::atoi(tokens[move_index + 2].c_str());
            const int vz = std::atoi(tokens[move_index + 3].c_str());
            {
                std::lock_guard<std::mutex> lock(g_debug_mtx);
                g_link_test_enabled = false;
                g_debug_state.link_test_enabled = false;
                g_debug_state.link_test_forward = false;
            }
            if (g_chassis != nullptr && g_chassis->is_connected()) {
                g_chassis->SendVelocity(static_cast<int16_t>(vx),
                                        static_cast<int16_t>(vy),
                                        static_cast<int16_t>(vz));
            }
            std::cout
                << "{\"success\":true,"
                << "\"channel\":\"serial\","
                << "\"command\":\"move\","
                << "\"vx\":" << vx << ","
                << "\"vy\":" << vy << ","
                << "\"vz\":" << vz << ","
                << "\"message\":\"已发送手动运动指令并关闭 link_test\"}"
                << std::endl;
            return false;
        }
        std::cout
            << "{\"success\":false,"
            << "\"channel\":\"serial\","
            << "\"command\":\"move\","
            << "\"message\":\"用法: A1_TEST move <vx> <vy> <vz>\"}"
            << std::endl;
        return false;
    }

    if (input.find("A1_TEST") != std::string::npos || input.find("test_echo") != std::string::npos) {
        std::cout
            << "{\"success\":true,"
            << "\"channel\":\"serial\","
            << "\"command\":\"test_echo\","
            << "\"message\":\"测试回传成功\","
            << "\"echo\":\"" << json_escape(input) << "\"}"
            << std::endl;
        return false;
    }

    std::cout
        << "{\"success\":false,"
        << "\"channel\":\"serial\","
        << "\"message\":\"未识别命令\","
        << "\"echo\":\"" << json_escape(input) << "\"}"
        << std::endl;
    return false;
}

/**
 * @brief 键盘监听程序，用于结束demo
 */
void keyboard_listener() {
    std::string input;
    std::cout << "键盘监听线程已启动，可输入 help 查看串口调试命令" << std::endl;

    while (true) {
        if (!std::getline(std::cin, input)) {
            if (std::cin.eof()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                std::cin.clear();
                continue;
            }
            break;
        }
        if (handle_console_command(input)) break;
    }
}

/**
 * @brief 检查退出标志的辅助函数（线程安全）
 * @return 是否需要退出
 */
bool check_exit_flag() {
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_exit_flag;
}

namespace {

uint64_t monotonic_time_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000ULL +
           static_cast<uint64_t>(tv.tv_usec);
}

}  // namespace

/**
 * @brief 人物检测演示程序主函数
 * @return 执行结果，0表示成功
 */
int main() {
    /******************************************************************************************
     * 1. 参数配置
     ******************************************************************************************/
    
    // 图像尺寸配置
    // 当前板端原始输出为竖屏 720×1280；Aurora.exe 显示层会再旋转。
    int img_width = cfg::SENSOR_WIDTH;
    int img_height = cfg::SENSOR_HEIGHT;
    
    // 模型配置参数
    // 推理输入: 640×360（由 RunAiPreprocessPipe 从当前传感器帧缩放）
    array<int, 2> det_shape = {cfg::DET_WIDTH, cfg::DET_HEIGHT};  // 640×360
    string path_det = cfg::MODEL_PATH;  // YOLOv8 head6 模型路径
    
    /******************************************************************************************
     * 2. 系统初始化
     ******************************************************************************************/
    
    // SSNE初始化
    if (ssne_initial()) {
        fprintf(stderr, "SSNE initialization failed!\n");
    }
    
    // 图像处理器初始化
    array<int, 2> img_shape = {img_width, img_height};  // 原始图像尺寸，当前为 720×1280
    // 废弃旧裁剪参数 crop_shape / crop_offset_y
    // 新方案: sensor 当前输出 → RunAiPreprocessPipe 缩放 → 640×360 推理
    // img_shape 同时作为检测器坐标系参考，避免前后端再使用旧裁剪偏移。
    
    IMAGEPROCESSOR processor;
    processor.Initialize(&img_shape);  // 初始化图像处理器（全分辨率 1280×720）
    if (!processor.IsReady()) {
        fprintf(stderr, "[FATAL] 在线图像 pipeline 初始化失败，终止 demo 以避免后续取图段错误\n");
        ssne_release();
        return 1;
    }
    
    // YOLOv8 目标检测器初始化
    YOLOV8 detector;
    detector.nms_threshold = cfg::DET_NMS_THRESH;
    detector.keep_top_k    = cfg::DET_KEEP_TOP_K;
    detector.top_k         = cfg::DET_TOP_K;
    // in_img_shape 传入原始传感器尺寸，使 Postprocess 坐标系与板端 OSD 一致。
    detector.Initialize(path_det, &img_shape, &det_shape);  // 初始化检测器
    
    // 人物检测结果初始化
    FaceDetectionResult* det_result = new FaceDetectionResult;
    
    // OSD可视化器初始化（用于绘制检测框）
    VISUALIZER visualizer;
    visualizer.Initialize(img_shape);  // 初始化可视化器（配置图像尺寸）

    // 底盘控制器初始化（A1 UART TX0/RX0 ↔ STM32 UART3，115200 baud）
    ChassisController chassis;
    bool chassis_ok = chassis.Init();
    g_chassis = &chassis;
    {
        std::lock_guard<std::mutex> lock(g_debug_mtx);
        g_debug_state.chassis_ok = chassis_ok;
        g_debug_state.link_test_enabled = g_link_test_enabled;
    }
    if (chassis_ok) {
        printf("[INFO] 底盘控制器初始化成功 (UART 115200)\n");
    } else {
        fprintf(stderr, "[WARN] 底盘控制器初始化失败，底盘控制不可用\n");
    }

    const uint64_t link_test_start_us = monotonic_time_us();
    bool last_link_test_forward = false;
    uint64_t last_detect_log_us = 0;

    // 系统稳定等待
    cout << "sleep for 0.2 second!" << endl;
    usleep(200000);  // 等待系统稳定
    
    uint64_t num_frames = 0;  // 帧计数器
    ssne_tensor_t img_sensor;  // 图像tensor定义

    // 创建键盘监听线程
    std::thread listener_thread(keyboard_listener);
    
    /******************************************************************************************
     * 3. 主处理循环
     ******************************************************************************************/
    //循环50000帧后推出，循环次数可以修改，也可以改成while(true)
    while (!check_exit_flag()) {
        
        // 从sensor获取图像（裁剪图）
        processor.GetImage(&img_sensor);
        
        // 目标检测模型推理（YOLOv8, 置信度阈值来自 cfg::DET_CONF_THRESH）
        detector.Predict(&img_sensor, det_result, cfg::DET_CONF_THRESH);
        num_frames += 1;

        /**********************************************************************************
         * 3.0 A1 ↔ STM32 联通性测试模块（临时）
         *
         * 需求背景：当前阶段不是验证“识别到什么动作”，而是先验证 A1 板端 UART
         * 到 STM32 底盘控制链路是否稳定可达。因此这里故意把正式的“检测结果驱动底盘”
         * 逻辑整体旁路掉，改成一个非常容易观察、又相对安全的固定节拍：
         *
         *   - 每 5 秒为一个周期
         *   - 周期内前 1 秒发送轻微前进指令
         *   - 剩余 4 秒持续发送停车指令
         *
         * 这样做的好处：
         *   1. 不依赖模型是否识别到 person / forward，能把“链路问题”和“模型问题”拆开；
         *   2. 前进速度固定且较低，便于安全观察；
         *   3. 删除也简单：后续恢复正式识别联动时，直接删掉本代码块，并恢复下面被注释
         *      标记为“正式识别联动逻辑”的分支即可。
         *
         * 注意：这个测试模块只改变底盘控制，不屏蔽检测与 OSD。也就是说，板端仍然会继续
         * 跑 YOLOv8 并继续画框，所以可以同时观察“链路是否通”和“OSD 是否真正有检测框”。
         **********************************************************************************/
        bool link_test_enabled_snapshot = false;
        {
            std::lock_guard<std::mutex> lock(g_debug_mtx);
            link_test_enabled_snapshot = g_link_test_enabled;
            g_debug_state.frame_index = num_frames;
            g_debug_state.link_test_enabled = link_test_enabled_snapshot;
        }

        if (link_test_enabled_snapshot) {
            const uint64_t elapsed_us = monotonic_time_us() - link_test_start_us;
            const uint64_t phase_us = elapsed_us % cfg::LINK_TEST_PERIOD_US;
            const bool should_forward = phase_us < cfg::LINK_TEST_FORWARD_WINDOW_US;
            {
                std::lock_guard<std::mutex> lock(g_debug_mtx);
                g_debug_state.link_test_forward = should_forward;
            }

            if (should_forward != last_link_test_forward) {
                if (should_forward) {
                    printf("[LINK_TEST] 周期触发前进 1 秒，vx=%d mm/s\n",
                           cfg::LINK_TEST_FORWARD_VX);
                } else {
                    printf("[LINK_TEST] 周期前进结束，恢复停车 4 秒\n");
                }
                last_link_test_forward = should_forward;
            }

            if (chassis_ok) {
                if (should_forward) {
                    chassis.SendVelocity(cfg::LINK_TEST_FORWARD_VX, 0, 0);
                } else {
                    chassis.SendVelocity(cfg::VX_STOP, 0, 0);
                }
            }
        } else {
            std::lock_guard<std::mutex> lock(g_debug_mtx);
            g_debug_state.link_test_forward = false;
        }
        
        /**********************************************************************************
         * 3.1 正式识别联动逻辑
         *
         * 这一段目前只保留检测统计与 OSD 绘制，不再驱动底盘。
         * 原因：底盘动作已经被上面的 LINK_TEST 模块接管，用于纯链路联通性测试。
         * 后续要恢复“识别结果直接控制底盘”时：
         *   1. 将 cfg::LINK_TEST_ENABLED 改为 false 或删除该配置；
         *   2. 删掉上面的 3.0 代码块；
         *   3. 再把这里的动作判断重新绑定到 chassis.SendVelocity()。
         **********************************************************************************/
        /**********************************************************************************
         * 3.1 解析检测结果并根据类别决定底盘动作
         **********************************************************************************/
        if (det_result->boxes.size() > 0) {
            /**********************************************************************************
             * 3.2 坐标转换：将crop图坐标转换为原图坐标
             **********************************************************************************/
            std::vector<std::array<float, 4>> boxes_original_coord;  // 存储转换后的原图坐标
            
            bool has_forward = false;
            bool has_stop = false;
            bool has_obstacle = false;
            bool has_backward = false;
            size_t forward_count = 0;
            size_t stop_count = 0;
            size_t obstacle_count = 0;
            size_t backward_count = 0;
            size_t person_count = 0;

            // 遍历所有检测框进行坐标转换
            for (size_t i = 0; i < det_result->boxes.size(); i++) {
                // 检测器已经将坐标映射到原始传感器空间。
                // 不再需要 crop_offset_y 偏移（废弃旧裁剪方案）
                float x1_orig = det_result->boxes[i][0];
                float y1_orig = det_result->boxes[i][1];
                float x2_orig = det_result->boxes[i][2];
                float y2_orig = det_result->boxes[i][3];
                
                // 保存原图坐标用于OSD绘制
                boxes_original_coord.push_back({x1_orig, y1_orig, x2_orig, y2_orig});

                const int cls_id =
                    i < det_result->class_ids.size() ? det_result->class_ids[i] : -1;
                if (cls_id == cfg::TARGET_CLASS_FORWARD) {
                    has_forward = true;
                    forward_count++;
                } else if (cls_id == cfg::TARGET_CLASS_STOP) {
                    has_stop = true;
                    stop_count++;
                } else if (cls_id == cfg::TARGET_CLASS_OBSTACLE_BOX) {
                    has_obstacle = true;
                    obstacle_count++;
                } else if (cls_id == cfg::TARGET_CLASS_BACKWARD) {
                    has_backward = true;
                    backward_count++;
                } else {
                    person_count++;
                }
            }
            
            /**********************************************************************************
             * 3.3 OSD绘图：使用原图坐标在OSD上绘制检测框
             **********************************************************************************/
            visualizer.Draw(boxes_original_coord);

            /**********************************************************************************
             * 3.4 调试打印：保留类别统计，帮助判断“没有 OSD”究竟是没有检测框，还是 OSD
             *     绘制链路异常。
             **********************************************************************************/
            const uint64_t now_log_us = monotonic_time_us();
            if (now_log_us - last_detect_log_us >= 5000000ULL) {
                printf("[DET] count=%zu person=%zu forward=%zu stop=%zu obstacle=%zu backward=%zu\n",
                       det_result->boxes.size(),
                       person_count,
                       forward_count,
                       stop_count,
                       obstacle_count,
                       backward_count);
                last_detect_log_us = now_log_us;
            }
            {
                std::lock_guard<std::mutex> lock(g_debug_mtx);
                g_debug_state.det_count = det_result->boxes.size();
                g_debug_state.person_count = person_count;
                g_debug_state.forward_count = forward_count;
                g_debug_state.stop_count = stop_count;
                g_debug_state.obstacle_count = obstacle_count;
                g_debug_state.backward_count = backward_count;
            }
        }
        else {
            // 未检测到目标时清空 OSD。底盘动作由上方 LINK_TEST 模块统一负责。
            const uint64_t now_log_us = monotonic_time_us();
            if (now_log_us - last_detect_log_us >= 5000000ULL) {
                cout << "[DET] 未检测到目标，已清空 OSD 检测框" << endl;
                last_detect_log_us = now_log_us;
            }
            std::vector<std::array<float, 4>> empty_boxes;
            visualizer.Draw(empty_boxes);  // 传入空向量清除显示
            {
                std::lock_guard<std::mutex> lock(g_debug_mtx);
                g_debug_state.det_count = 0;
                g_debug_state.person_count = 0;
                g_debug_state.forward_count = 0;
                g_debug_state.stop_count = 0;
                g_debug_state.obstacle_count = 0;
                g_debug_state.backward_count = 0;
            }
        }
    }

    // 等待监听线程退出，释放资源
    if (listener_thread.joinable()) {
        listener_thread.join();
    }
    g_chassis = nullptr;
    
    /******************************************************************************************
     * 4. 资源释放
     ******************************************************************************************/
    
    delete det_result;  // 释放检测结果
    detector.Release();  // 释放检测器资源
    processor.Release();  // 释放图像处理器资源
    visualizer.Release();  // 释放可视化器资源
    
    if (ssne_release()) {
        fprintf(stderr, "SSNE release failed!\n");
        return -1;
    }
    
    return 0;
}
 
