#pragma once

#include <atomic>
#include <thread>

#include "chassis_controller.hpp"
#include "common.hpp"
#include "project_paths.hpp"
#include "utils.hpp"

namespace ssne_demo {

/// 人脸驱动应用
///
/// 功能: 检测到人脸 → 小车直行；未检测到 → 停车。
///
/// 处理流程 (每帧):
///   1. 捕获传感器图像 (SC132GS 720×1280)
///   2. SCRFD 人脸检测 (640×480)
///   3. 决策: face_detected → 前进; 否则 → 停止
///   4. 通过 GPIO UART0 向 STM32 发送运动指令
///   5. OSD 渲染检测框
class FaceDriveApp {
 public:
  explicit FaceDriveApp(ProjectPaths paths = ProjectPaths{});

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
  ChassisController chassis_;
  FaceDetectionResult result_;

  std::atomic<bool> exit_requested_{false};
  std::thread keyboard_thread_;
};

}  // namespace ssne_demo
