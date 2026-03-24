/**
 * @file face_drive_app.cpp
 * @brief 人脸检测驱动兼容性测试应用实现
 *
 * 核心逻辑:
 *   - 检测到人脸 + 前方无障碍物 → 小车以低速直行 (0.1 m/s)
 *   - 未检测到人脸 或 前方有障碍物 → 立即停车
 *
 * 安全机制:
 *   - RPLidar 前方障碍物检测 (±30°, 阈值 0.3m)
 *   - 低速运行, 适合硬件兼容性验证
 *   - 退出时自动发送停止指令并关闭串口
 */

#include "../include/face_drive_app.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <utility>

namespace ssne_demo {

// 兼容性测试参数
static const int16_t kForwardSpeed = 100;       // 前进速度: 100 mm/s (0.1 m/s)
static const float kObstacleThreshold = 0.3f;   // 前方障碍物安全距离: 0.3m
static const float kFrontAngleRange = 30.0f;    // 前方检测角度范围: ±30°

FaceDriveApp::FaceDriveApp(ProjectPaths paths) : paths_(std::move(paths)) {}

void FaceDriveApp::Initialize() {
  // 1. 初始化 SSNE 硬件
  if (ssne_initial()) {
    std::fprintf(stderr, "[FATAL] SSNE 初始化失败!\n");
    return;
  }

  // 2. 图像管道
  processor_.Initialize(&paths_.image_shape);

  // 3. SCRFD 人脸检测器
  detector_.Initialize(paths_.face_model_path, &paths_.crop_shape,
                       &paths_.det_shape, false,
                       paths_.det_shape[0] * paths_.det_shape[1] / 512 * 21);

  // 4. OSD 可视化
  visualizer_.Initialize(paths_.image_shape);

  // 5. RPLidar 雷达 (安全保护)
  lidar_.Configure(paths_.lidar_serial_port, paths_.lidar_baudrate);
  if (!lidar_.Start()) {
    std::cerr << "[WARN] RPLidar 启动失败, 将不使用雷达安全保护" << std::endl;
  } else {
    std::cout << "[INFO] RPLidar 已启动" << std::endl;
  }

  // 6. 底盘 UART 控制器 (GPIO UART → STM32)
  if (!chassis_.Open(paths_.chassis_serial_port, paths_.chassis_baudrate)) {
    std::cerr << "[WARN] 底盘串口打开失败 (" << paths_.chassis_serial_port
              << "), 将仅运行视觉检测, 不发送运动指令" << std::endl;
  } else {
    std::cout << "[INFO] 底盘控制已连接: " << paths_.chassis_serial_port
              << std::endl;
    // 上电后先发送停止指令确保安全
    chassis_.SendStop();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::cout << "[INFO] FaceDriveApp 初始化完成" << std::endl;
  std::cout << "[INFO] 兼容性测试模式: 检测到人脸 → 直行 "
            << kForwardSpeed << " mm/s" << std::endl;
}

void FaceDriveApp::KeyboardLoop() {
  std::cout << "输入 'q' 退出程序..." << std::endl;
  while (!exit_requested_.load()) {
    std::string input;
    std::cin >> input;
    if (input == "q" || input == "Q") {
      exit_requested_.store(true);
      std::cout << "检测到退出指令, 停车并退出..." << std::endl;
      break;
    }
  }
}

void FaceDriveApp::StartKeyboardThread() {
  keyboard_thread_ = std::thread(&FaceDriveApp::KeyboardLoop, this);
}

void FaceDriveApp::ProcessOnce() {
  ssne_tensor_t img_sensor{};
  result_.Clear();

  // 1. 捕获图像
  processor_.GetImage(&img_sensor);

  // 2. 人脸检测
  detector_.Predict(&img_sensor, &result_, paths_.confidence_threshold);

  // 3. 雷达扫描 — 检查前方障碍物
  bool obstacle_ahead = false;
  const auto lidar_samples = lidar_.ScanOnce();
  if (!lidar_samples.empty()) {
    for (const auto& s : lidar_samples) {
      // 将弧度转为角度
      float angle_deg = s.angle_deg * 180.0f / 3.14159f;
      // 前方区域: 330°~360° 或 0°~30°
      bool is_front = (angle_deg > (360.0f - kFrontAngleRange)) ||
                      (angle_deg < kFrontAngleRange);
      if (is_front && s.distance_m > 0.01f &&
          s.distance_m < kObstacleThreshold) {
        obstacle_ahead = true;
        break;
      }
    }
  }

  // 4. 决策与控制
  bool face_detected = !result_.boxes.empty();

  if (face_detected && !obstacle_ahead) {
    // 检测到人脸且前方安全 → 直行
    chassis_.SendVelocity(kForwardSpeed, 0, 0);
    std::printf("[DRIVE] 人脸检测到 (%zu), 直行 %d mm/s\n",
                result_.boxes.size(), kForwardSpeed);
  } else {
    // 未检测到人脸 或 前方有障碍物 → 停止
    chassis_.SendStop();
    if (obstacle_ahead) {
      std::printf("[STOP] 前方障碍物 (< %.1fm), 紧急停车\n",
                  kObstacleThreshold);
    } else {
      std::printf("[STOP] 未检测到人脸\n");
    }
  }

  // 5. OSD 渲染
  if (result_.boxes.empty()) {
    std::vector<std::array<float, 4>> empty_boxes;
    visualizer_.Draw(empty_boxes);
  } else {
    std::vector<std::array<float, 4>> boxes_original_coord;
    boxes_original_coord.reserve(result_.boxes.size());
    for (const auto& box : result_.boxes) {
      boxes_original_coord.push_back(
          {box[0], box[1] + static_cast<float>(paths_.crop_offset_y),
           box[2], box[3] + static_cast<float>(paths_.crop_offset_y)});
    }
    visualizer_.Draw(boxes_original_coord);
  }
}

void FaceDriveApp::Shutdown() {
  // 确保停车
  if (chassis_.IsOpen()) {
    chassis_.SendStop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  chassis_.Close();

  lidar_.Stop();

  if (keyboard_thread_.joinable()) {
    keyboard_thread_.join();
  }

  detector_.Release();
  processor_.Release();
  visualizer_.Release();

  if (ssne_release()) {
    std::fprintf(stderr, "SSNE 释放失败!\n");
  }

  std::cout << "[INFO] FaceDriveApp 关闭完成" << std::endl;
}

int FaceDriveApp::Run() {
  Initialize();
  StartKeyboardThread();

  while (!exit_requested_.load()) {
    ProcessOnce();
  }

  Shutdown();
  return 0;
}

}  // namespace ssne_demo
