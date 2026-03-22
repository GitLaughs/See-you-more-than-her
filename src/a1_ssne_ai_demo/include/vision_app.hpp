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

/// Main application integrating YOLOv8 object detection, face detection,
/// RPLidar scanning, and OSD rendering.
///
/// Pipeline per frame:
///   1. Capture sensor image (720×1280 grayscale)
///   2. Run SCRFD face detection on cropped ROI
///   3. Run YOLOv8 object detection on same or full-frame input
///   4. Read one lidar scan (360° point cloud)
///   5. Render face boxes (green), yolo boxes (per-class colored), and
///      lidar warning info overlay via OSD hardware
///   6. Output detection + lidar data to debug interface (JSON over TCP)
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

  /// Print detection summary to stdout for debug
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
