/**
 * project_paths.hpp — ssne_ai_demo 全局配置
 *
 * 分辨率设计说明:
 *   - OSD 画布: 1920 × 1080，背景和右侧动画使用全画布绝对坐标
 *   - 摄像头源: 720 × 1280，取中心 360 × 640 作为左侧视频窗口
 *   - 摄像头窗口: OSD 坐标 (150, 220)，尺寸 360 × 640
 *   - 推理输入: 640 × 480，RunAiPreprocessPipe 将摄像头窗口缩放到模型输入
 *
 * 模型说明:
 *   - 当前板端默认使用 YOLOv8 灰度 4 类模型
 *   - 模型路径: /app_demo/app_assets/models/best_a1_640x480.m1model
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace cfg {

// ─── 摄像头源分辨率 ────────────────────────────────────────────────────────
constexpr int SENSOR_WIDTH  = 1920;
constexpr int SENSOR_HEIGHT = 1080;

// ─── OSD 显示画布 ────────────────────────────────────────────────────────────
constexpr int OSD_WIDTH  = 1920;
constexpr int OSD_HEIGHT = 1080;

// ─── 摄像头视频窗口：中心裁剪后贴到背景左侧 ───────────────────────────────────
constexpr int CAMERA_VIEW_X = 150;
constexpr int CAMERA_VIEW_WIDTH = 360;
constexpr int CAMERA_VIEW_HEIGHT = 640;
constexpr int CAMERA_VIEW_Y = (OSD_HEIGHT - CAMERA_VIEW_HEIGHT) / 2;
constexpr int CAMERA_VIEW_RIGHT = CAMERA_VIEW_X + CAMERA_VIEW_WIDTH;
constexpr int CAMERA_VIEW_BOTTOM = CAMERA_VIEW_Y + CAMERA_VIEW_HEIGHT;

// ─── 在线裁剪区域：720×1280 源图中心 384×640 ────────────────────────────────
constexpr int PIPE_CROP_WIDTH  = 384;
constexpr int PIPE_CROP_HEIGHT = CAMERA_VIEW_HEIGHT;
constexpr int PIPE_CROP_X1 = (SENSOR_WIDTH - PIPE_CROP_WIDTH) / 2;
constexpr int PIPE_CROP_X2 = PIPE_CROP_X1 + PIPE_CROP_WIDTH;
constexpr int PIPE_CROP_Y1 = (SENSOR_HEIGHT - PIPE_CROP_HEIGHT) / 2;
constexpr int PIPE_CROP_Y2 = PIPE_CROP_Y1 + PIPE_CROP_HEIGHT;

// ─── 推理后端选择 ────────────────────────────────────────────────────────────
constexpr bool USE_SCRFD_BACKEND = true;

// ─── 当前模型推理输入分辨率 ──────────────────────────────────────────────────
constexpr int DET_WIDTH  = 640;
constexpr int DET_HEIGHT = 480;

// ─── 模型文件路径 ────────────────────────────────────────────────────────────
const std::string MODEL_PATH =
    "/app_demo/app_assets/models/face_640x480.m1model";

// ─── 人脸问候 OSD 参数 ───────────────────────────────────────────────────────
constexpr int FACE_PRESENT_CONFIRM_FRAMES = 2;
constexpr int FACE_CLEAR_CONFIRM_FRAMES = 4;
constexpr int HELLO_OSD_LAYER = 4;

// ─── YOLOv8 模型参数 ─────────────────────────────────────────────────────────
constexpr int  YOLO_NUM_CLASSES = 4;
constexpr int  TARGET_CLASS_PERSON       = 0;
constexpr int  TARGET_CLASS_FORWARD      = 1;
constexpr int  TARGET_CLASS_STOP         = 2;
constexpr int  TARGET_CLASS_OBSTACLE_BOX = 3;
constexpr int  YOLO_REG_BINS    = 16;
constexpr std::array<int, 3> STRIDES = {8, 16, 32};
constexpr int OUTPUT_HEAD_NUM = 6;

// ─── 检测后处理参数 ──────────────────────────────────────────────────────────
constexpr float DET_CONF_THRESH = 0.4f;
constexpr float DET_NMS_THRESH  = 0.45f;
constexpr int   DET_TOP_K       = 150;
constexpr int   DET_KEEP_TOP_K  = 30;

// ─── 板端动作状态机参数 ──────────────────────────────────────────────────────
constexpr int   GESTURE_CONFIRM_FRAMES   = 3;
constexpr int   OBSTACLE_CONFIRM_FRAMES  = 2;
constexpr int   CLEAR_CONFIRM_FRAMES     = 4;
constexpr float OBSTACLE_NEAR_AREA_RATIO = 0.20f;
constexpr float OBSTACLE_WARN_AREA_RATIO = 0.10f;
constexpr float OBSTACLE_BOTTOM_RATIO    = 0.72f;
constexpr float OBSTACLE_CENTER_LEFT     = 0.42f;
constexpr float OBSTACLE_CENTER_RIGHT    = 0.58f;

// ─── 底盘控制参数 ─────────────────────────────────────────────────────────────
constexpr uint32_t UART_BAUD   = 115200;
constexpr int16_t  VX_FORWARD  = 100;
constexpr int16_t  VX_STOP     = 0;
constexpr int16_t  VZ_TURN     = 140;

// ─── A1↔STM32 联通性测试参数（临时）──────────────────────────────────────────
constexpr bool     LINK_TEST_ENABLED            = false;
constexpr int16_t  LINK_TEST_FORWARD_VX         = 60;
constexpr uint64_t LINK_TEST_PERIOD_US          = 5000000ULL;
constexpr uint64_t LINK_TEST_FORWARD_WINDOW_US  = 1000000ULL;

} // namespace cfg
