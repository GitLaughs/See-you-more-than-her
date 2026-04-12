/**
 * project_paths.hpp — ssne_ai_demo 全局配置
 *
 * 分辨率设计说明:
 *   - 传感器采集: 1280 × 720 (16:9, Y8 灰度)
 *   - 推理输入:   640  × 360 (16:9, 直接缩放, 无裁剪)
 *   原有的 crop_shape / crop_offset_y 已废弃, 统一使用缩放方案.
 */

#pragma once

#include <array>
#include <string>

namespace cfg {

// ─── 摄像头采集分辨率 ────────────────────────────────────────────────────────
// SC132GS 传感器原生 1280×720 灰度 (Y8)
constexpr int SENSOR_WIDTH  = 1280;
constexpr int SENSOR_HEIGHT = 720;

// ─── 推理输入分辨率 ──────────────────────────────────────────────────────────
// 将采集帧 1280×720 缩放至 640×360 送入 SCRFD 模型
constexpr int DET_WIDTH  = 640;
constexpr int DET_HEIGHT = 360;

// ─── 模型文件路径 ────────────────────────────────────────────────────────────
inline const std::string MODEL_PATH = "/app_demo/app_assets/models/face_640x480.m1model";

// ─── 检测参数 ────────────────────────────────────────────────────────────────
// SCRFD 置信度阈值
constexpr float DET_CONF_THRESH = 0.4f;
// NMS IoU 阈值
constexpr float DET_NMS_THRESH  = 0.45f;

// ─── 底盘控制参数 ─────────────────────────────────────────────────────────────
constexpr uint32_t UART_BAUD        = 115200;   // 波特率
constexpr int16_t  VX_FORWARD       =  100;     // 前进速度 (mm/s)
constexpr int16_t  VX_STOP          =    0;     // 停止

// ─── SCRFD 输出解析参数 ──────────────────────────────────────────────────────
// SCRFD 三个检测尺度的步长 (对应 640×360 输入)
constexpr std::array<int, 3> STRIDES = {8, 16, 32};
// 每个锚点预测数量
constexpr int ANCHORS_PER_STRIDE = 2;
// 输出数量 = 6 个 head (cls×3 + reg×3)
constexpr int OUTPUT_HEAD_NUM = 6;

} // namespace cfg
