/**
 * project_paths.hpp — ssne_ai_demo 全局配置
 *
 * 分辨率设计说明:
 *   - 传感器采集: 1280 × 720 (16:9, Y8 灰度)
 *   - 推理输入:   640  × 360 (16:9, RunAiPreprocessPipe 直接缩放, 无裁剪)
 *   旧的 crop_shape / crop_offset_y 已废弃.
 *
 * 模型说明:
 *   - 模型架构: YOLOv8 (head6 切分版, 后处理在 CPU 实现)
 *   - 训练分辨率: 640×360 灰度
 *   - 输出: 6 head (cv3×3 分类 + cv2×3 回归)
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace cfg {

// ─── 摄像头采集分辨率 ────────────────────────────────────────────────────────
// SC132GS 传感器原生 1280×720 灰度 (Y8)
constexpr int SENSOR_WIDTH  = 1280;
constexpr int SENSOR_HEIGHT = 720;

// ─── 推理输入分辨率 ──────────────────────────────────────────────────────────
// 将采集帧 1280×720 缩放至 640×360 送入 YOLOv8 模型 (保持 16:9, 无裁剪)
constexpr int DET_WIDTH  = 640;
constexpr int DET_HEIGHT = 360;

// ─── 模型文件路径 ────────────────────────────────────────────────────────────
// YOLOv8 head6 模型: 在 Detect Head 切分, 后处理在 CPU 完成
inline const std::string MODEL_PATH =
    "/app_demo/app_assets/models/best_a1_formal_head6.m1model";

// ─── YOLOv8 模型参数 ─────────────────────────────────────────────────────────
// 模型训练类别数 (与导出的 best_a1_formal.onnx 语义保持一致)
constexpr int  YOLO_NUM_CLASSES = 4;
constexpr int  TARGET_CLASS_PERSON       = 0;
constexpr int  TARGET_CLASS_FORWARD      = 1;
constexpr int  TARGET_CLASS_STOP         = 2;
constexpr int  TARGET_CLASS_OBSTACLE_BOX = 3;
// DFL 回归 bins 数量
constexpr int  YOLO_REG_BINS    = 16;
// YOLOv8 三个 FPN 尺度的步长
constexpr std::array<int, 3> STRIDES = {8, 16, 32};
// 输出 head 数量 (cls×3 + reg×3)
constexpr int OUTPUT_HEAD_NUM = 6;

// ─── 检测后处理参数 ──────────────────────────────────────────────────────────
// 置信度阈值
constexpr float DET_CONF_THRESH = 0.4f;
// NMS IoU 阈值
constexpr float DET_NMS_THRESH  = 0.45f;
// NMS 前保留 top-k 个候选框
constexpr int   DET_TOP_K       = 150;
// NMS 后最终保留数量
constexpr int   DET_KEEP_TOP_K  = 30;

// ─── 底盘控制参数 ─────────────────────────────────────────────────────────────
constexpr uint32_t UART_BAUD   = 115200;  // 波特率
constexpr int16_t  VX_FORWARD  =  100;   // 前进速度 (mm/s)
constexpr int16_t  VX_STOP     =    0;   // 停止

} // namespace cfg
