/**
 * @file face_drive_app.cpp
 * @brief 人脸检测驱动应用实现
 *
 * 核心逻辑 (参考 SDK demo_face 检测流程):
 *   - 检测到人脸 → 小车以低速直行 (100 mm/s)
 *   - 未检测到人脸 → 立即停车
 *
 * 暂不使用雷达, 后续安装雷达后再启用避障功能。
 */

#include "../include/face_drive_app.hpp"

#include <chrono>
#include <iostream>
#include <utility>

namespace ssne_demo {

static const int16_t kForwardSpeed = 100;  // 前进速度: 100 mm/s

FaceDriveApp::FaceDriveApp(ProjectPaths paths) : paths_(std::move(paths)) {}

void FaceDriveApp::Initialize() {
  // 1. SSNE 硬件初始化
  if (ssne_initial()) {
    std::fprintf(stderr, "[FATAL] SSNE 初始化失败!\n");
    return;
  }

  // 2. 图像管道 (SC132GS → 720×540 裁剪)
  processor_.Initialize(&paths_.image_shape);

  // 3. SCRFD 人脸检测器 (与 SDK demo_face 相同)
  detector_.Initialize(paths_.face_model_path, &paths_.crop_shape,
                       &paths_.det_shape, false,
                       paths_.det_shape[0] * paths_.det_shape[1] / 512 * 21);

  // 4. OSD 可视化
  visualizer_.Initialize(paths_.image_shape);

  // 5. 底盘 UART 控制器 (A1 GPIO UART0 → STM32 UART3)
  if (!chassis_.Open(paths_.chassis_baudrate)) {
    std::cerr << "[WARN] 底盘 UART 初始化失败, 将仅运行视觉检测" << std::endl;
  } else {
    std::cout << "[INFO] 底盘控制已连接: GPIO UART0 @ "
              << paths_.chassis_baudrate << " baud" << std::endl;
    chassis_.SendStop();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::cout << "[INFO] FaceDriveApp 初始化完成" << std::endl;
  std::cout << "[INFO] 检测到人脸 → 直行 " << kForwardSpeed << " mm/s" << std::endl;
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

  // 2. 人脸检测 (SCRFD, 与 SDK demo_face 相同逻辑)
  detector_.Predict(&img_sensor, &result_, paths_.confidence_threshold);

  // 3. 决策与控制
  bool face_detected = !result_.boxes.empty();

  if (face_detected) {
    chassis_.SendVelocity(kForwardSpeed, 0, 0);
    std::printf("[DRIVE] 人脸 (%zu), 直行 %d mm/s\n",
                result_.boxes.size(), kForwardSpeed);
  } else {
    chassis_.SendStop();
    std::printf("[STOP] 未检测到人脸\n");
  }

  // 4. OSD 渲染 (与 SDK demo_face 相同坐标还原)
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
  if (chassis_.IsOpen()) {
    chassis_.SendStop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  chassis_.Close();

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
