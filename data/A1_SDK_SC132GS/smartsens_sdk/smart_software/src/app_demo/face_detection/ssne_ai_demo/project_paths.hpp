/**
 * project_paths.hpp — ssne_ai_demo 全局配置
 *
 * 分辨率设计说明:
 *   - 传感器采集: 当前板端返回 720 × 1280 (Y8 灰度, Aurora 显示层会旋转)
 *   - 推理输入:   640 × 480 (RunAiPreprocessPipe 直接缩放, 无裁剪)
 *   旧的 crop_shape / crop_offset_y 已废弃.
 *
 * 模型说明:
 *   - 当前板端默认回退到历史 SCRFD 灰度人脸模型
 *   - 模型路径: /app_demo/app_assets/models/face_640x480.m1model
 *   - 若后续恢复 YOLOv8，需要重新提供可用的 YOLOv8 模型文件
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace cfg {

// ─── 摄像头采集分辨率 ────────────────────────────────────────────────────────
// 当前板端实际返回为竖屏 720×1280 灰度 (Y8)；
// Aurora.exe 会在显示层做旋转，因此桌面看到的是转正后的画面。
constexpr int SENSOR_WIDTH  = 720;
constexpr int SENSOR_HEIGHT = 1280;

// ─── 推理后端选择 ────────────────────────────────────────────────────────────
// 当前默认回退到历史 face_640x480.m1model，并走 SCRFDGRAY 推理链路。
constexpr bool USE_SCRFD_BACKEND = true;

// ─── 当前模型推理输入分辨率 ──────────────────────────────────────────────────
// 历史 face_640x480.m1model 对应 640×480 输入
constexpr int DET_WIDTH  = 640;
constexpr int DET_HEIGHT = 480;

// ─── 模型文件路径 ────────────────────────────────────────────────────────────
// 当前默认模型文件：旧版 SCRFD 人脸检测模型
const std::string MODEL_PATH =
    "/app_demo/app_assets/models/face_640x480.m1model";

// ─── YOLOv8 模型参数 ─────────────────────────────────────────────────────────
// 模型训练类别数 (与导出的 best_a1_formal.onnx 语义保持一致)
constexpr int  YOLO_NUM_CLASSES = 4;
constexpr int  TARGET_CLASS_PERSON       = 0;
constexpr int  TARGET_CLASS_FORWARD      = 1;
constexpr int  TARGET_CLASS_STOP         = 2;
constexpr int  TARGET_CLASS_OBSTACLE_BOX = 3;
constexpr int  TARGET_CLASS_BACKWARD     = 4;
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
constexpr int16_t  VX_BACKWARD = -100;   // 后退速度 (mm/s)
constexpr int16_t  VX_STOP     =    0;   // 停止

// ─── A1↔STM32 联通性测试参数（临时）──────────────────────────────────────────
// 这一组常量只服务于“验证上下位机链路是否通”的临时测试模块。
// 当前需求是：每隔 5 秒给 STM32 下发 1 秒轻微前进，其余 4 秒持续停车。
// 之后恢复正式识别联动时，只需要删除 demo_face.cpp 中引用这些常量的临时代码块即可。
constexpr bool     LINK_TEST_ENABLED            = true;
constexpr int16_t  LINK_TEST_FORWARD_VX         = 60;    // mm/s，故意放慢，避免联通性测试时车速过快
constexpr uint64_t LINK_TEST_PERIOD_US          = 5000000ULL;
constexpr uint64_t LINK_TEST_FORWARD_WINDOW_US  = 1000000ULL;

} // namespace cfg
