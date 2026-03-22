/**
 * @file demo_vision.cpp
 * @brief Entry point for the integrated vision application
 *
 * This demo runs YOLOv8 object detection + SCRFD face detection + RPLidar
 * with OSD hardware rendering on the A1 development board.
 */
#include "include/vision_app.hpp"

int main() {
  ssne_demo::VisionApp app;
  return app.Run();
}
