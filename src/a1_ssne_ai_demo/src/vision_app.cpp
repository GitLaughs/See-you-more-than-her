/**
 * @file vision_app.cpp
 * @brief 集成YOLOv8、人脸检测、激光雷达和OSD的主应用循环
 */

#include "../include/vision_app.hpp"
#include <chrono>
#include <iostream>
#include <utility>

namespace ssne_demo {

VisionApp::VisionApp(ProjectPaths paths) : paths_(std::move(paths)) {}

void VisionApp::Initialize() {
  // 初始化SSNE硬件
  if (ssne_initial()) {
    std::fprintf(stderr, "[FATAL] SSNE初始化失败!\n");
    return;
  }

  // 图像管道
  processor_.Initialize(&paths_.image_shape);

  // 人脸检测器（SCRFD）
  face_detector_.Initialize(
      paths_.face_model_path, &paths_.crop_shape, &paths_.det_shape,
      false, paths_.det_shape[0] * paths_.det_shape[1] / 512 * 21);

  // YOLOv8目标检测器
  yolo_detector_.Initialize(
      paths_.yolo_model_path,
      paths_.crop_shape,     // 重用裁剪的ROI
      paths_.yolo_det_shape, // 例如 640x640
      paths_.yolo_num_classes,
      paths_.yolo_confidence_threshold,
      paths_.yolo_nms_threshold);
  yolo_detector_.SetClassNames(paths_.yolo_class_names);

  // OSD可视化器
  visualizer_.Initialize(paths_.image_shape);

  // RPLidar激光雷达
  lidar_.Configure(paths_.lidar_serial_port, paths_.lidar_baudrate);
  if (!lidar_.Start()) {
    std::cerr << "[WARN] RPLidar适配器启动失败" << std::endl;
  } else {
    std::cout << "[INFO] RPLidar适配器已启动" << std::endl;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::cout << "[INFO] VisionApp初始化完成" << std::endl;
}

void VisionApp::KeyboardLoop() {
  std::cout << "输入 'q' 退出程序..." << std::endl;
  while (!exit_requested_.load()) {
    std::string input;
    std::cin >> input;
    if (input == "q" || input == "Q") {
      exit_requested_.store(true);
      std::cout << "检测到退出指令" << std::endl;
      break;
    }
  }
}

void VisionApp::StartKeyboardThread() {
  keyboard_thread_ = std::thread(&VisionApp::KeyboardLoop, this);
}

void VisionApp::ProcessOnce() {
  ssne_tensor_t img_sensor{};
  face_result_.Clear();
  yolo_result_.Clear();

  // 1. 捕获图像
  processor_.GetImage(&img_sensor);

  // 2. 人脸检测
  face_detector_.Predict(&img_sensor, &face_result_, paths_.confidence_threshold);

  // 3. YOLOv8目标检测
  yolo_detector_.Predict(&img_sensor, &yolo_result_);

  // 4. 激光雷达扫描
  const auto lidar_samples = lidar_.ScanOnce();

  // 5. OSD渲染

  // 人脸框 - 转换到原始图像坐标
  std::vector<std::array<float, 4>> face_boxes;
  face_boxes.reserve(face_result_.boxes.size());
  for (const auto& box : face_result_.boxes) {
    face_boxes.push_back(
        {box[0], box[1] + static_cast<float>(paths_.crop_offset_y),
         box[2], box[3] + static_cast<float>(paths_.crop_offset_y)});
  }
  visualizer_.DrawFaces(face_boxes);

  // YOLOv8检测框（已经在原始图像坐标中）
  visualizer_.DrawDetections(yolo_result_);

  // 激光雷达 proximity 警告覆盖
  if (!lidar_samples.empty()) {
    // 寻找前方区域（±30°）的最小距离
    float min_dist = 999.0f;
    for (const auto& s : lidar_samples) {
      float angle_deg = s.angle_deg * 180.0f / 3.14159f;
      if (angle_deg > 330.0f || angle_deg < 30.0f) {
        if (s.distance_m > 0.01f && s.distance_m < min_dist) {
          min_dist = s.distance_m;
        }
      }
    }
    // 如果前方障碍物小于0.5m，显示红色警告覆盖
    if (min_dist < 0.5f) {
      visualizer_.DrawInfoRegion({0, 0, 100, 50}, kColorObstacle);
    }
  }

  // 6. 调试日志
  LogDetections(yolo_result_, face_result_, lidar_samples);
}

void VisionApp::LogDetections(const DetectionResult& yolo_result,
                              const FaceDetectionResult& face_result,
                              const std::vector<LidarSample>& lidar_samples) {
  if (!yolo_result.detections.empty()) {
    std::printf("[YOLO] %zu detections: ", yolo_result.detections.size());
    for (const auto& d : yolo_result.detections) {
      std::printf("%s(%.2f) ", yolo_detector_.ClassName(d.class_id).c_str(),
                  d.score);
    }
    std::printf("\n");
  }

  if (!face_result.boxes.empty()) {
    std::printf("[FACE] %zu faces\n", face_result.boxes.size());
  }

  if (!lidar_samples.empty()) {
    std::printf("[LIDAR] %zu samples\n", lidar_samples.size());
  }
}

void VisionApp::Shutdown() {
  lidar_.Stop();
  if (keyboard_thread_.joinable()) {
    keyboard_thread_.join();
  }

  face_detector_.Release();
  yolo_detector_.Release();
  processor_.Release();
  visualizer_.Release();

  if (ssne_release()) {
    std::fprintf(stderr, "SSNE释放失败!\n");
  }
  std::cout << "[INFO] VisionApp关闭完成" << std::endl;
}

int VisionApp::Run() {
  Initialize();
  StartKeyboardThread();

  while (!exit_requested_.load()) {
    ProcessOnce();
  }

  Shutdown();
  return 0;
}

}  // namespace ssne_demo
