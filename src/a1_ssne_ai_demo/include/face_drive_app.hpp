#pragma once

#include <atomic>
#include <thread>

#include "chassis_controller.hpp"
#include "common.hpp"
#include "lidar_sdk_adapter.hpp"
#include "project_paths.hpp"
#include "utils.hpp"

namespace ssne_demo {

/// 人脸驱动兼容性测试应用
///
/// 功能: 检测到人脸时，小车直行；未检测到人脸或前方有障碍物时，停车。
///
/// 处理流程 (每帧):
///   1. 捕获传感器图像
///   2. SCRFD 人脸检测
///   3. RPLidar 雷达扫描 (安全检查)
///   4. 决策: face_detected AND no_obstacle → 前进; 否则 → 停止
///   5. 通过 GPIO UART 向 STM32 发送运动指令
///   6. OSD 渲染检测框和状态信息
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
  RplidarSdkAdapter lidar_;
  ChassisController chassis_;
  FaceDetectionResult result_;

  std::atomic<bool> exit_requested_{false};
  std::thread keyboard_thread_;
};

}  // namespace ssne_demo
