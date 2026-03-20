#include "../include/project_flow.hpp"

#include <chrono>
#include <iostream>
#include <utility>

using namespace std::chrono_literals;

namespace ssne_demo {

FaceDetectionDemoApp::FaceDetectionDemoApp(ProjectPaths paths) : paths_(std::move(paths)) {}

void FaceDetectionDemoApp::Initialize() {
  if (ssne_initial()) {
    std::fprintf(stderr, "SSNE initialization failed!\n");
  }

  processor_.Initialize(&paths_.image_shape);
  detector_.Initialize(paths_.face_model_path, &paths_.crop_shape, &paths_.det_shape, false,
                       paths_.det_shape[0] * paths_.det_shape[1] / 512 * 21);
  visualizer_.Initialize(paths_.image_shape);

  std::cout << "sleep for 0.2 second!" << std::endl;
  std::this_thread::sleep_for(200ms);
}

void FaceDetectionDemoApp::KeyboardLoop() {
  std::cout << "键盘监听线程已启动，输入 'q' 退出程序..." << std::endl;
  while (!exit_requested_.load()) {
    std::string input;
    std::cin >> input;
    if (input == "q" || input == "Q") {
      exit_requested_.store(true);
      std::cout << "检测到退出指令，通知主线程退出..." << std::endl;
      break;
    }
    std::cout << "输入无效（仅 'q' 有效），请重新输入：" << std::endl;
  }
}

void FaceDetectionDemoApp::StartKeyboardThread() {
  keyboard_thread_ = std::thread(&FaceDetectionDemoApp::KeyboardLoop, this);
}

void FaceDetectionDemoApp::ProcessOnce() {
  ssne_tensor_t img_sensor{};
  result_.Clear();
  processor_.GetImage(&img_sensor);
  detector_.Predict(&img_sensor, &result_, paths_.confidence_threshold);

  if (result_.boxes.empty()) {
    std::cout << "[INFO] No face detected" << std::endl;
    std::vector<std::array<float, 4>> empty_boxes;
    visualizer_.Draw(empty_boxes);
    return;
  }

  std::vector<std::array<float, 4>> boxes_original_coord;
  boxes_original_coord.reserve(result_.boxes.size());
  for (const auto& box : result_.boxes) {
    boxes_original_coord.push_back({box[0], box[1] + static_cast<float>(paths_.crop_offset_y), box[2],
                                    box[3] + static_cast<float>(paths_.crop_offset_y)});
  }
  visualizer_.Draw(boxes_original_coord);
}

void FaceDetectionDemoApp::Shutdown() {
  if (keyboard_thread_.joinable()) {
    keyboard_thread_.join();
  }

  detector_.Release();
  processor_.Release();
  visualizer_.Release();

  if (ssne_release()) {
    std::fprintf(stderr, "SSNE release failed!\n");
  }
}

int FaceDetectionDemoApp::Run() {
  Initialize();
  StartKeyboardThread();

  while (!exit_requested_.load()) {
    ProcessOnce();
  }

  Shutdown();
  return 0;
}

}  // namespace ssne_demo