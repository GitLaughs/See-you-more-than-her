/*
 * @Filename: utils.hpp
 * @Author: Twyla Tu
 * @Email: qian.tu@smartsenstech.com
 * @Date: 2024-08-08 15-07-50
 * @Copyright (c) 2024 SmartSens
 */
#pragma once

#include "osd-device.hpp"
#include <algorithm>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#define NUM_MAX_PED 10
#define WEIGHT_CURRENT 0.5
#define FRAMES_OUTSIDE 5
#define FRAMES_EMPTY 15

#define FRAMES_AVG 15
#define FRAMES_RED 10
#define FRAMES_YELLOW 10
#define FRAMES_GREEN 10
#define FRAMES_BLUE 10

namespace utils {
  // 人脸检测模型所需的函数
  /* 合并两段结果 */
  void Merge(DetectionResult* result, size_t low, size_t mid, size_t high);
  /* 归并排序算法 */
  void MergeSort(DetectionResult* result, size_t low, size_t high);
  /* 对检测结果进行排序 */
  void SortDetectionResult(DetectionResult* result);
  /* 非极大值抑制 */
  void NMS(DetectionResult* result, float iou_threshold, int top_k);

  // 人脸进程所需的函数
  struct CropResult {
    ~CropResult() {
      if (crop_img.data != nullptr) {
        release_tensor(crop_img);
      }
    }
    ssne_tensor_t crop_img;
    int x1;
    int y1;
    int x2;
    int y2;
    int h_size;
    int w_size;
  };

  /* 选取离镜头最近的人脸 */
  void processDetection(DetectionResult* det_result, std::array<float, 4>& det_out);

  float calculateArea(const std::array<float, 4>& det_box);
  float calculateIou(const std::array<float, 4>& bbox1, const std::array<float, 4>& bbox2);
  void calculateRecurrent(const std::array<float, 4>& bbox1, const std::array<float, 4>& bbox2,
    float weight, std::array<float, 4>& bbox);
} // namespace utils

typedef enum
{
  EMPTY = 0,
  RED = 1,
  YELLOW = 2,
  GREEN = 3,
  BLUE = 4
} PedColor;

typedef enum
{
  NO_PERSON = 0,
  OUTSIDE = 1,
  INSIDE = 2
} PedStatus;

struct DetPed {
  uint8_t fcnt_red = 0;
  uint8_t fcnt_yellow = 0;
  uint8_t fcnt_green = 0;
  uint8_t fcnt_blue = 0;
  std::array<float, 4> bbox = {0, 0, 0, 0};
  PedColor color_judge = EMPTY;
  PedColor color_draw = EMPTY;
};

struct DistanceResult {
  std::array<float, FRAMES_AVG> x;
  std::array<float, FRAMES_AVG> z;
  std::array<PedStatus, FRAMES_AVG> status;
  uint8_t idx_e = 0; // 结束帧位置
  uint8_t size_res = 0; // 记录的帧数
  uint8_t outside_cnt = 0;
  uint8_t empty_cnt = 0;

  float avgX() {
    float sum_x = 0;
    uint8_t size_sum = 0;
    for (int i = 0; i < size_res; i++) {
      if (status[i] != INSIDE) {
        continue;
      }
      else {
        sum_x += x[i];
        size_sum += 1;
      }
    }
    float avg_x = sum_x / size_sum;
    return avg_x;
  }

  float avgZ() {
    float sum_z = 0;
    uint8_t size_sum = 0;
    for (int i = 0; i < size_res; i++) {
      if (status[i] != INSIDE) {
        continue;
      }
      else {
        sum_z += z[i];
        size_sum += 1;
      }
    }
    float avg_z = sum_z / size_sum;
    return avg_z;
  }

  void update(float cal_x, float cal_z, PedStatus ped_status) {
    // 更新状态
    if (ped_status == OUTSIDE) {
        outside_cnt += 1;
    }
    else if (ped_status == NO_PERSON) {
      empty_cnt += 1;
    }
    else {
      outside_cnt = 0;
      empty_cnt = 0;
    }

    x[idx_e] = cal_x;
    z[idx_e] = cal_z;
    status[idx_e] = ped_status;
    idx_e = (idx_e + 1) % FRAMES_AVG;
    size_res = std::max(size_res + 1, FRAMES_AVG);
  }
};

class VISUALIZER {
  public:
    void Initialize(float& in_scale, std::array<int, 2>& in_img_shape);
    void Run(ssne_tensor_t output[], DetectionResult* result, float& conf_threshold);
    void Release();
    // const OSD& getOSD() const;
    void Draw();

  private:
    // 后处理时，cfg所包含的每个stage的图像尺寸要求
    std::vector<std::vector<std::array<int, 2>>> min_sizes_yolo;
    // 后处理时，nms阈值
    float nms_threshold;
    // 后处理时，做完nms之后最多保存的box个数
    int keep_top_k;
    // 后处理时，做nms之前最多保存的box个数
    int top_k;
    // 后处理时，检测的缩放尺度
    float det_scale;
    // 后处理函数
    void Postprocess(ssne_tensor_t output[], DetectionResult* result, float& conf_threshold);
    // osd初始化
    sst::device::osd::OsdDevice osd_device;

    // std::chrono::high_resolution_clock::time_point start;
    // std::chrono::high_resolution_clock::time_point end;
};

class Calculator {
  public:
    void Initialize(std::string filename_x, std::string filename_z, bool flip, std::array<int, 2>& img_shape);
    void Run(DetectionResult* result, bool flag_dirty, bool out_uart);
    void Release();
    const std::vector<DetPed>& getPedestrians() const;

  private:
    uint32_t filesize;
    int z_max, z_min;
    int x_max, x_min;
    int camera_cx;
    int width, height;
    std::vector<uint8_t> map_z;
    std::vector<uint8_t> map_x;
    bool img_flip = false;
    int ParseFile(std::vector<uint8_t>& data, std::string filename);
    int fd;
    // 逻辑处理
    std::vector<DetPed> pedestrians;
    std::vector<int> match_idxs;
    DistanceResult distance_result;
    // 将历史检测框与当前检测结果进行匹配，输出匹配的idx
    void match(DetectionResult* result, std::vector<int>& flag_matched);
    // 根据匹配的idx更新存储的检测结果
    void update(DetectionResult* result, std::vector<int>& flag_matched);
    void write_serial(const std::array<char, 3>& bytes);

		bool box_inside(const std::array<float, 4>& det);
    bool point_inside(Point p);
    float crossProduct(const Point& A, const Point& B, const Point& C);
    bool isPointInTriangle(const Point& A, const Point& B, const Point& C, const Point& P);
    bool isPointInParallelogram(const Point& A, const Point& B, const Point& C, const Point& D, const Point& P);


    // 3m
    // const std::array<float, 8> trapezoid = {737, 608, 1067, 608, 216, 1280, 1586, 1280};
		// const std::array<float, 4> line00 = {538, 827, 312, 1146};
    // const std::array<float, 4> line01 = {1264, 827, 1490, 1146};
    // const std::array<float, 4> line1 = {538, 827, 1264, 827};
    // const std::array<float, 4> line2 = {671, 678, 1133, 678};
    // const std::array<float, 4> line3 = {737, 608, 1067, 608};
		const std::array<float, 8> ped_trapezoid = {795, 605, 1125, 605, 262, 1280, 1658, 1280};
		const std::array<float, 4> ped_line00 = {595, 830, 360, 1154};
    const std::array<float, 4> ped_line01 = {1325, 830, 1560, 1154};
    const std::array<float, 4> ped_line1 = {595, 830, 1325, 830};
    const std::array<float, 4> ped_line2 = {727, 677, 1193, 678};
    const std::array<float, 4> ped_line3 = {795, 605, 1125, 605};


};
