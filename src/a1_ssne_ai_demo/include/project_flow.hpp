#pragma once

#include <atomic>
#include <thread>

#include "common.hpp"
#include "lidar_sdk_adapter.hpp"
#include "project_paths.hpp"
#include "utils.hpp"

namespace ssne_demo {

class FaceDetectionDemoApp {
 public:
  explicit FaceDetectionDemoApp(ProjectPaths paths = ProjectPaths{});

  int Run();

 private:
  void Initialize();
  void ProcessOnce();
  void Shutdown();
  void StartKeyboardThread();
  void KeyboardLoop();

  ProjectPaths paths_;
  IMAGEPROCESSOR processor_;
  SCRFDGRAY detector_;
  VISUALIZER visualizer_;
  RplidarSdkAdapter lidar_;
  FaceDetectionResult result_;

  std::atomic<bool> exit_requested_{false};
  std::thread keyboard_thread_;
};

}  // namespace ssne_demo