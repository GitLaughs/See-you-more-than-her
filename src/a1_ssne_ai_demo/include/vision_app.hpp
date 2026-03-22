#pragma once

#include <atomic>
#include <string>
#include <thread>
#include "common.hpp"
#include "lidar_sdk_adapter.hpp"
#include "osd_visualizer.hpp"
#include "project_paths.hpp"
#include "yolov8_detector.hpp"

namespace ssne_demo {

/// 集成YOLOv8目标检测、人脸检测、RPLidar扫描和OSD渲染的主应用。
///
/// 每帧处理流程：
///   1. 捕获传感器图像（720×1280灰度图）
///   2. 在裁剪的ROI上运行SCRFD人脸检测
///   3. 在相同或全帧输入上运行YOLOv8目标检测
///   4. 读取一次激光雷达扫描（360°点云）
///   5. 通过OSD硬件渲染人脸框（绿色）、YOLO框（按类别着色）和
///      激光雷达警告信息覆盖
///   6. 将检测结果+激光雷达数据输出到调试接口（通过TCP的JSON）
class VisionApp {
 public:
  explicit VisionApp(ProjectPaths paths = ProjectPaths{});
  int Run();

 private:
  void Initialize();
  void ProcessOnce();
  void Shutdown();
  void StartKeyboardThread();
  void KeyboardLoop();

  /// 将检测摘要打印到标准输出以进行调试
  void LogDetections(const DetectionResult& yolo_result,
                     const FaceDetectionResult& face_result,
                     const std::vector<LidarSample>& lidar_samples);

  ProjectPaths paths_;
  IMAGEPROCESSOR processor_;
  SCRFDGRAY face_detector_;
  Yolov8Detector yolo_detector_;
  OsdVisualizer visualizer_;
  RplidarSdkAdapter lidar_;

  FaceDetectionResult face_result_;
  DetectionResult yolo_result_;

  std::atomic<bool> exit_requested_{false};
  std::thread keyboard_thread_;
};

}  // namespace ssne_demo
