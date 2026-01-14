/*
 * @Filename: utils.cpp
 * @Author: Twyla Tu
 * @Email: qian.tu@smartsenstech.com
 * @Date: 2024-08-08 15-07-37
 * @Copyright (c) 2024 SmartSens
 */
#include "../include/utils.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

namespace utils {

void Merge(DetectionResult* result, size_t low, size_t mid, size_t high) {
  std::vector<std::array<float, 4>>& boxes = result->boxes;
  std::vector<float>& scores = result->scores;
  std::vector<std::array<float, 4>> temp_boxes(boxes);
  std::vector<float> temp_scores(scores);
  size_t i = low;
  size_t j = mid + 1;
  size_t k = i;
  for (; i <= mid && j <= high; k++) {
    if (temp_scores[i] >= temp_scores[j]) {
      scores[k] = temp_scores[i];
      boxes[k] = temp_boxes[i];
      i++;
    } else {
      scores[k] = temp_scores[j];
      boxes[k] = temp_boxes[j];
      j++;
    }
  }
  while (i <= mid) {
    scores[k] = temp_scores[i];
    boxes[k] = temp_boxes[i];
    k++;
    i++;
  }
  while (j <= high) {
    scores[k] = temp_scores[j];
    boxes[k] = temp_boxes[j];
    k++;
    j++;
  }
}

void MergeSort(DetectionResult* result, size_t low, size_t high) {
  if (low < high) {
    size_t mid = (high - low) / 2 + low;
    MergeSort(result, low, mid);
    MergeSort(result, mid + 1, high);
    Merge(result, low, mid, high);
  }
}

void SortDetectionResult(DetectionResult* result) {
  size_t low = 0;
  size_t high = result->scores.size();
  if (high == 0) {
    return;
  }
  high = high - 1;
  MergeSort(result, low, high);
}

void NMS(DetectionResult* result, float iou_threshold, int top_k) {
  // 根据检测分数对检测结果进行排序整理
  SortDetectionResult(result);

  // 保留其中的top-K个值
  int res_count = static_cast<int>(result->boxes.size());
  result->Resize(std::min(res_count, top_k));

  // 计算面积
  std::vector<float> area_of_boxes(result->boxes.size());
  std::vector<int> suppressed(result->boxes.size(), 0);
  for (size_t i = 0; i < result->boxes.size(); ++i) {
    area_of_boxes[i] = (result->boxes[i][2] - result->boxes[i][0] + 1) *
                       (result->boxes[i][3] - result->boxes[i][1] + 1);
  }

  // nms过程
  for (size_t i = 0; i < result->boxes.size(); ++i) {
    if (suppressed[i] == 1) {
      continue;
    }
    for (size_t j = i + 1; j < result->boxes.size(); ++j) {
      if (suppressed[j] == 1) {
        continue;
      }
      float xmin = std::max(result->boxes[i][0], result->boxes[j][0]);
      float ymin = std::max(result->boxes[i][1], result->boxes[j][1]);
      float xmax = std::min(result->boxes[i][2], result->boxes[j][2]);
      float ymax = std::min(result->boxes[i][3], result->boxes[j][3]);
      float overlap_w = std::max(0.0f, xmax - xmin + 1);
      float overlap_h = std::max(0.0f, ymax - ymin + 1);
      float overlap_area = overlap_w * overlap_h;
      float overlap_ratio =
          overlap_area / (area_of_boxes[i] + area_of_boxes[j] - overlap_area);
      if (overlap_ratio > iou_threshold) {
        suppressed[j] = 1;
      }
    }
  }
  DetectionResult backup(*result);
  int landmarks_per_obj = result->landmarks_per_obj;

  result->Clear();
  // don't forget to reset the landmarks_per_obj
  // before apply Reserve method.
  result->landmarks_per_obj = landmarks_per_obj;
  result->Reserve(suppressed.size());
  for (size_t i = 0; i < suppressed.size(); ++i) {
    if (suppressed[i] == 1) {
      continue;
    }
    result->boxes.emplace_back(backup.boxes[i]);
    result->scores.push_back(backup.scores[i]);
    // landmarks (if have)
    if (result->landmarks_per_obj > 0) {
      for (size_t j = 0; j < result->landmarks_per_obj; ++j) {
        result->landmarks.emplace_back(
            backup.landmarks[i * result->landmarks_per_obj + j]);
      }
    }
  }
}

void processDetection(DetectionResult* det_result,
    std::array<float, 4>& det_out) {
    unsigned int index = 0;
    float s_max = 0;
    std::array<float, 4> det = {0, 0, 0, 0};
    // 遍历所有检测结果，找到离镜头距离最近的人脸
    for (unsigned int i = 0; i < det_result->boxes.size(); i++) {
        det = det_result->boxes[i];
        float s = (det.at(2) - det.at(0)) * (det.at(3) - det.at(1));
        if (s > s_max) {
            s_max = s;
            index = i;
        }
    }
    det_out = det_result->boxes[index];
    // printf("score selected: %f\n", det_result->scores[index]);
}

float calculateArea(const std::array<float, 4>& det_box) {
  float width = det_box[2] - det_box[0];
  float height = det_box[3] - det_box[1];
  return width * height;
}

float calculateIou(const std::array<float, 4>& bbox1, const std::array<float, 4>& bbox2) {
  float xmin = std::max(bbox1[0], bbox2[0]);
  float ymin = std::max(bbox1[1], bbox2[1]);
  float xmax = std::min(bbox1[2], bbox2[2]);
  float ymax = std::min(bbox1[3], bbox2[3]);
  float overlap_w = std::max(0.0f, xmax - xmin + 1);
  float overlap_h = std::max(0.0f, ymax - ymin + 1);
  float overlap = overlap_w * overlap_h;
  float iou = overlap / (calculateArea(bbox1) + calculateArea(bbox2) - overlap);
  return iou;
}

void calculateRecurrent(const std::array<float, 4>& bbox1, const std::array<float, 4>& bbox2,
    float weight, std::array<float, 4>& bbox) {
      bbox[0] = bbox1[0] * weight + bbox2[0] * (1 - weight);
      bbox[1] = bbox1[1] * weight + bbox2[1] * (1 - weight);
      bbox[2] = bbox1[2] * weight + bbox2[2] * (1 - weight);
      bbox[3] = bbox1[3] * weight + bbox2[3] * (1 - weight);
  }
}  // namespace utils

void VISUALIZER::Initialize(float& in_scale, std::array<int, 2>& in_img_shape) {
    nms_threshold = 0.5;
    keep_top_k = 9;
    top_k = 150;
    det_scale = in_scale;

    min_sizes_yolo = {{{ 12,  16}, { 19,  36}, { 40,  28}},
                {{ 36,  75}, { 76,  55}, { 72, 146}},
                {{142, 110}, {192, 243}, {459, 401}}};

    osd_device.Initialize(in_img_shape[0], in_img_shape[1]);

    // auto start = std::chrono::high_resolution_clock::now();
    // auto end = std::chrono::high_resolution_clock::now();
}

void VISUALIZER::Postprocess(ssne_tensor_t output[],
  DetectionResult* result, float& conf_threshold) {
    // 数据类型转换
    std::vector<std::array<float, 4>> bboxes;
    std::vector<float> scores;
    std::array<float, 4> tmp_bbox;

    float *dst1_b = (float*)get_data(output[0]);
    float *dst2_b = (float*)get_data(output[1]);
    float *dst3_b = (float*)get_data(output[2]);
    float *dst1_s = (float*)get_data(output[3]);
    float *dst2_s = (float*)get_data(output[4]);
    float *dst3_s = (float*)get_data(output[5]);
    //std::cout << "finish inference!!!!!!!!!!!!!!!!!!!" << std::endl;

    int num_res = (2400 + 600 + 150) * 3;
    int res_count = 0;
    result->Clear();
    result->Reserve(num_res);

    int stride = 32;
    int classes = 1;

    //nhwc
    int idx_b = 0;
    int idx_s = 0;
    int step_b = 4;
    int step_s = classes + 1;
    for (int h=0; h<10; h++){
        for (int w=0; w<15; w++){
            for (int i=0; i<3; i++){
                //int idx = (classes + 5) * i + (w + h * 15) * 3 * (classes + 5);
                float score = dst3_s[idx_s + 0] * dst3_s[idx_s + 1];
                if (score >= conf_threshold){
                    std::array<float, 4> tmp_bbox;
                    float x1 = (dst3_b[idx_b] * 2.0 - 0.5 + w) * stride;
                    float y1 = (dst3_b[idx_b + 1] * 2.0 - 0.5 + h) * stride;
                    float w1 = dst3_b[idx_b + 2] * dst3_b[idx_b + 2] * 4.0 * min_sizes_yolo[2][i][0];
                    float h1 = dst3_b[idx_b + 3] * dst3_b[idx_b + 3] * 4.0 * min_sizes_yolo[2][i][1];

                    tmp_bbox[0] = x1 - w1 / 2;
                    tmp_bbox[1] = y1 - h1 / 2;
                    tmp_bbox[2] = x1 + w1 / 2;
                    tmp_bbox[3] = y1 + h1 / 2;
                    result->boxes.emplace_back(tmp_bbox);
                    result->scores.push_back(score);
                    res_count += 1;
                }
                idx_s += step_s;
                idx_b += step_b;
            }
        }
    }

    stride = 16;
    idx_b = 0;
    idx_s = 0;
    for (int h=0; h<20; h++){
        for (int w=0; w<30; w++){
            for (int i=0; i<3; i++){
                //int idx = (classes + 5) * i + (w + h * 15) * 3 * (classes + 5);
                float score = dst2_s[idx_s + 0] * dst2_s[idx_s + 1];
                if (score >= conf_threshold){
                    std::array<float, 4> tmp_bbox;
                    float x1 = (dst2_b[idx_b] * 2.0 - 0.5 + w) * stride;
                    float y1 = (dst2_b[idx_b + 1] * 2.0 - 0.5 + h) * stride;
                    float w1 = dst2_b[idx_b + 2] * dst2_b[idx_b + 2] * 4.0 * min_sizes_yolo[1][i][0];
                    float h1 = dst2_b[idx_b + 3] * dst2_b[idx_b + 3] * 4.0 * min_sizes_yolo[1][i][1];

                    tmp_bbox[0] = x1 - w1 / 2;
                    tmp_bbox[1] = y1 - h1 / 2;
                    tmp_bbox[2] = x1 + w1 / 2;
                    tmp_bbox[3] = y1 + h1 / 2;
                    result->boxes.emplace_back(tmp_bbox);
                    result->scores.push_back(score);
                    res_count += 1;
                }
                idx_s += step_s;
                idx_b += step_b;
            }
        }
    }

    stride = 8;
    idx_b = 0;
    idx_s = 0;
    for (int h=0; h<40; h++){
        for (int w=0; w<60; w++){
            for (int i=0; i<3; i++){
                //int idx = (classes + 5) * i + (w + h * 15) * 3 * (classes + 5);
                float score = dst1_s[idx_s + 0] * dst1_s[idx_s + 1];
                if (score >= conf_threshold){
                    std::array<float, 4> tmp_bbox;
                    float x1 = (dst1_b[idx_b] * 2.0 - 0.5 + w) * stride;
                    float y1 = (dst1_b[idx_b + 1] * 2.0 - 0.5 + h) * stride;
                    float w1 = dst1_b[idx_b + 2] * dst1_b[idx_b + 2] * 4.0 * min_sizes_yolo[0][i][0];
                    float h1 = dst1_b[idx_b + 3] * dst1_b[idx_b + 3] * 4.0 * min_sizes_yolo[0][i][1];

                    tmp_bbox[0] = x1 - w1 / 2;
                    tmp_bbox[1] = y1 - h1 / 2;
                    tmp_bbox[2] = x1 + w1 / 2;
                    tmp_bbox[3] = y1 + h1 / 2;
                    result->boxes.emplace_back(tmp_bbox);
                    result->scores.push_back(score);
                    res_count += 1;
                }
                idx_s += step_s;
                idx_b += step_b;
            }
        }
    }

    result->Resize(res_count);

    // std::cout << "finish decode!!!!!!!!!!!!!!!!!!!" << std::endl;

    // 执行NMS
    utils::NMS(result, nms_threshold, top_k);

    // 恢复尺度
    res_count = static_cast<int>(result->boxes.size());
    result->Resize(std::min(res_count, keep_top_k));

    for (unsigned int i = 0; i < result->boxes.size(); i++) {
        result->boxes[i][0] = result->boxes[i][0] * det_scale;
        result->boxes[i][1] = result->boxes[i][1] * det_scale;
        result->boxes[i][2] = result->boxes[i][2] * det_scale;
        result->boxes[i][3] = result->boxes[i][3] * det_scale;
    }
}

void VISUALIZER::Draw() {
    std::vector<sst::device::osd::OsdQuadRangle> quad_rangle_vec;

	sst::device::osd::OsdQuadRangle q;

	q.color = 0;
	q.box = {100, 100, 200, 200};
	q.border = 3;
	q.alpha = fdevice::TYPE_ALPHA75;
	q.type = fdevice::TYPE_HOLLOW;
	quad_rangle_vec.emplace_back(q);


    osd_device.Draw(quad_rangle_vec);
}

void VISUALIZER::Run(ssne_tensor_t output[], DetectionResult* result,
  float& conf_threshold) {
    //start = std::chrono::high_resolution_clock::now();
    Postprocess(output, result, conf_threshold);
    // Draw(result);

    // end = std::chrono::high_resolution_clock::now();
    // duration = std::chrono::duration<double, std::milli>(end - start);
    // durations_postprocess.push_back(duration);
}

// const OSD& VISUALIZER::getOSD() const {
//     return osd_device;
// }

void VISUALIZER::Release() {
    osd_device.Release();
}

int Calculator::ParseFile(std::vector<uint8_t>& data, std::string filename) {
    // 打开二进制文件
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return -1;
    }

    // 计算文件大小
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 检查文件大小是否符合预期
    if (size != filesize * sizeof(uint8_t)) {
        std::cerr << "文件大小不匹配: " << size << " bytes" << std::endl;
        return -1;
    }

    // 读取数据到vector中
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "读取文件失败" << std::endl;
        return -1;
    }
    return 0;
}

void Calculator::Initialize(std::string filename_x, std::string filename_z, bool flip,
  std::array<int, 2>& img_shape) {
  filesize = img_shape[0] * img_shape[1];
  img_flip = flip;
  width = img_shape[0];
  height = img_shape[1];
  x_max = 1;
  x_min = -1;
  z_max = 3;
  z_min = 0;
  camera_cx = 960;
  map_x.reserve(filesize);
  map_z.reserve(filesize);
  int ret = ParseFile(map_z, filename_z);
  ret = ParseFile(map_x, filename_x);

  // 串口设备
  fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY);
  if (fd == -1) {
    std::cerr << "Error opening serial port!" << std::endl;
  }

  // 获取并修改终端属性
  struct termios Opt;
  tcgetattr(fd, &Opt);
  Opt.c_oflag &= ~OPOST; // 禁用输出处理
  tcsetattr(fd, TCSANOW, &Opt); // 设置修改后的终端属性
}

void Calculator::write_serial(const std::array<char, 3>& bytes) {
  char serial_output[6];
  serial_output[0] = 0x66;
  serial_output[5] = '\0';
  serial_output[1] = bytes[0];
  serial_output[2] = bytes[1];
  serial_output[3] = bytes[2];
  serial_output[4] = serial_output[0] ^ bytes[0] ^ bytes[1] ^ bytes[2];
  // 发送数据，不包括字符串末尾的 '\0'
  int ret = write(fd, serial_output, sizeof(serial_output) - 1);
  // std::ios_base::fmtflags f(std::cout.flags());
  // std::cout << std::hex << std::setw(10) << std::setfill('0') << combined << std::endl;
}

const std::vector<DetPed>& Calculator::getPedestrians() const {
  return pedestrians;
}

void Calculator::match(DetectionResult* result, std::vector<int>& flag_matched) {
  size_t size_ped = pedestrians.size();
  size_t size_det = result->boxes.size();
  match_idxs.clear();
  match_idxs.resize(std::max(size_ped, size_det), -1);
  if (size_ped > 0) {
    if (size_det > 0) {
      // 遍历所有历史框
      for (size_t i = 0; i < size_ped; i++) {
        float iou_max = 0;
        float idx_max = -1;
        // 遍历所有检测结果
        for (size_t j = 0; j < size_det; j++) {
          if (flag_matched[j]) {
            continue;
          }
          float iou = utils::calculateIou(pedestrians[i].bbox, result->boxes[j]);
          if (iou > iou_max) {
            iou_max = iou;
            idx_max = j;
          }
        }
        if (idx_max != -1) {
          match_idxs[i] = idx_max;
          flag_matched[idx_max] = 1;
        }
      }
    }
  }
  else {
    // 从无人状态切换时无需匹配，取面积最大的前n个检测结果
    std::vector<int> indices(size_det);
    for (int i = 0; i < size_det; i++) {
      indices[i] = i;
    }
    std::sort(indices.begin(), indices.end(), [result](int a, int b) {
      return utils::calculateArea(result->boxes[a]) > utils::calculateArea(result->boxes[b]);
    });
    int num_peds = std::min(NUM_MAX_PED, static_cast<int>(size_det));
    for (int i = 0; i < num_peds; i++) {
      match_idxs[i] = i;
    }
  }
}

void Calculator::update(DetectionResult* result, std::vector<int>& flag_matched) {
  size_t size_match = match_idxs.size();
  size_t size_ped = pedestrians.size();

  std::vector<DetPed> backup(pedestrians);
  pedestrians.clear();
  std::array<float, 4> matched_det;
  for (size_t i = 0; i < size_ped; i++) {
    if (match_idxs[i] != -1) {
      // 匹配上的行人，更新检测框坐标，递归处理
      DetPed ped(backup[i]);
      matched_det = result->boxes[match_idxs[i]];
      utils::calculateRecurrent(matched_det, backup[i].bbox, WEIGHT_CURRENT, ped.bbox);
      pedestrians.push_back(ped);
    }
  }
  // size_match > size_ped 说明当前画面人数增加了，多出的部分为新增的人
  if (size_match > size_ped) {
    for (size_t i = 0; i < flag_matched.size(); i++) {
      if (flag_matched[i] == 0) {
        DetPed ped;
        ped.bbox = result->boxes[i];
        pedestrians.push_back(ped);
      }
    }
  }

  int x, y;
  bool found_inside = false;
  float cal_x, cal_z;
  float min_cal_dist = 100;
  float min_cal_z = 0;
  float min_cal_x = 0;
  // 更新世界坐标
  for (size_t i = 0; i < pedestrians.size(); i++) {
    float cx = 0.5 * (pedestrians[i].bbox[0] + pedestrians[i].bbox[2]);
    float cy = pedestrians[i].bbox[3];
    x = std::min(width - 1, std::max(0, int(cx)));
    y = std::min(height - 1, std::max(0, int(cy)));
    int idx_search = img_flip ? y * width + width - x : y * width + x;
    cal_x = float(map_x[idx_search]) / 255. * (x_max - x_min) + x_min;
    cal_z = float(map_z[idx_search]) / 255. * (z_max - z_min) + z_min;

    bool inside = box_inside(pedestrians[i].bbox);
    if (inside) {
      found_inside = true;
      if (std::sqrt(cal_x * cal_x + cal_z * cal_z) < min_cal_dist) {
        min_cal_dist = std::sqrt(cal_x * cal_x + cal_z * cal_z);
        min_cal_z = cal_z * 100;
        min_cal_x = cal_x * 100;
      }

      if (cy > ped_line1[1]) {
        // 1m线内
        pedestrians[i].color_judge = RED;
        pedestrians[i].fcnt_red = FRAMES_RED;
        // 如果是红色，则立刻画成红色，优先级低的状态置为0
        pedestrians[i].color_draw = RED;
        pedestrians[i].fcnt_red -= 1;
        pedestrians[i].fcnt_yellow = 0;
        pedestrians[i].fcnt_green = 0;
        pedestrians[i].fcnt_blue = 0;
      }
      else if (cy > ped_line2[1]) {
        // 2m线内
        pedestrians[i].color_judge = YELLOW;
        pedestrians[i].fcnt_yellow = FRAMES_YELLOW;
        // 如果是黄色，则等红色状态结束后变为黄色，否则仍为红色
        if (pedestrians[i].fcnt_red > 0) {
          pedestrians[i].color_draw = RED;
          pedestrians[i].fcnt_red -= 1;
        }
        else {
          pedestrians[i].color_draw = YELLOW;
          pedestrians[i].fcnt_yellow -= 1;
        }
        pedestrians[i].fcnt_green = 0;
        pedestrians[i].fcnt_blue = 0;
      }
      else {
        // 3m线内
        pedestrians[i].color_judge = GREEN;
        pedestrians[i].fcnt_green = FRAMES_GREEN;
        // 如果是绿色，则等红色状态和黄色状态结束后变为绿色，否则仍为优先级高的颜色
        if (pedestrians[i].fcnt_red > 0) {
          pedestrians[i].color_draw = RED;
          pedestrians[i].fcnt_red -= 1;
        }
        else if (pedestrians[i].fcnt_yellow > 0) {
          pedestrians[i].color_draw = YELLOW;
          pedestrians[i].fcnt_yellow -= 1;
        }
        else {
          pedestrians[i].color_draw = GREEN;
          pedestrians[i].fcnt_green -= 1;
        }
        pedestrians[i].fcnt_blue = 0;
      }
    }
    else {
      // 标定区域外
      pedestrians[i].color_judge = BLUE;
      pedestrians[i].fcnt_blue = FRAMES_BLUE;
      // 如果是蓝色，则等红黄绿色状态结束后变为蓝色，否则仍为优先级高的颜色
      if (pedestrians[i].fcnt_red > 0) {
        pedestrians[i].color_draw = RED;
        pedestrians[i].fcnt_red -= 1;
      }
      else if (pedestrians[i].fcnt_yellow > 0) {
        pedestrians[i].color_draw = YELLOW;
        pedestrians[i].fcnt_yellow -= 1;
      }
      else if (pedestrians[i].fcnt_green > 0) {
        pedestrians[i].color_draw = GREEN;
        pedestrians[i].fcnt_green -= 1;
      }
      else {
        pedestrians[i].color_draw = BLUE;
        pedestrians[i].fcnt_blue -= 1;
      }
    }
  }

  if (found_inside) {
    distance_result.update(min_cal_x, min_cal_z, INSIDE);
  }
  else {
    if (pedestrians.size() > 0) {
      distance_result.update(-1, -1, OUTSIDE);
    }
    else {
      distance_result.update(-1, -1, NO_PERSON);
    }
  }
}

void Calculator::Run(DetectionResult* result, bool flag_dirty, bool out_uart) {
  // 用于debug检测模型的效果
  bool debug_det = false;
  // debug检测模型时，不存储任何历史信息
  if (debug_det) {
    pedestrians.clear();
  }

  size_t size_det = result->boxes.size();
  // 判断检测框是否被match的flag
  std::vector<int> flag_matched(size_det);
  for (int i = 0; i < size_det; i++) {
    flag_matched[i] = 0;
  }
  // 匹配历史框与当前目标框
  match(result, flag_matched);
  // 更新行人检测的信息
  update(result, flag_matched);

  bool flag_outside = distance_result.outside_cnt >= FRAMES_OUTSIDE ? true : false;
  bool flag_empty = distance_result.empty_cnt >= FRAMES_EMPTY ? true : false;
  char b1;
  unsigned char b2, b3;
  if (flag_dirty) {
      // 有脏污
      b1 = 0x00;
      b2 = 0x02;
      b3 = 0x58;
  }
  else if (flag_empty) {
      // 无人
      b1 = 0x00;
      b2 = 0x01;
      b3 = 0xF4;
  }
  else if (pedestrians.size() > 0) {
    if (flag_outside) {
      // 坐标区域外有人
      b1 = 0x00;
      b2 = 0x01;
      b3 = 0x90;
    }
    else {
      float avg_x = distance_result.avgX();
      float avg_z = distance_result.avgZ();
      // 输出坐标
      b1 = static_cast<char>(static_cast<int>(avg_x));
      uint32_t cal_dist = static_cast<uint32_t>(avg_z);
      b2 = static_cast<unsigned char>(cal_dist >> 8);
      b3 = static_cast<unsigned char>(cal_dist & 0xFF);
    }
  }

  if (out_uart) {
      // 发送串口数据
      std::array<char, 3> bytes;
      bytes[0] = b1;
      bytes[1] = static_cast<char>(b2);
      bytes[2] = static_cast<char>(b3);
      write_serial(bytes);
  }
}

// 计算两个向量的叉积
float Calculator::crossProduct(const Point& A, const Point& B, const Point& C) {
    return (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
}

// 判断点P是否在三角形ABC内
bool Calculator::isPointInTriangle(const Point& A, const Point& B, const Point& C, const Point& P) {
    float cross1 = crossProduct(A, B, P);
    float cross2 = crossProduct(B, C, P);
    float cross3 = crossProduct(C, A, P);
    bool has_neg = (cross1 < 0) || (cross2 < 0) || (cross3 < 0);
    bool has_pos = (cross1 > 0) || (cross2 > 0) || (cross3 > 0);
    return !(has_neg && has_pos);
}

// 判断点P是否在平行四边形ABCD内
bool Calculator::isPointInParallelogram(const Point& A, const Point& B, const Point& C, const Point& D, const Point& P) {
    // 将平行四边形分成两个三角形ABC和CDA
    return isPointInTriangle(A, B, C, P) || isPointInTriangle(C, D, A, P);
}

bool Calculator::point_inside(Point p) {
    Point A(ped_line3[0], ped_line3[1]);
    Point B(ped_line3[2], ped_line3[3]);
    Point C(ped_line1[2], ped_line1[3]);
    Point D(ped_line1[0], ped_line1[1]);
    Point E(ped_trapezoid[4], ped_trapezoid[5]);
    Point F(ped_trapezoid[6], ped_trapezoid[7]);
    if (img_flip) {
        A.x = width - ped_line3[2];
        A.y = ped_line3[3];
        B.x = width - ped_line3[0];
        B.y = ped_line3[1];
        C.x = width - ped_line1[0];
        C.y = ped_line1[1];
        D.x = width - ped_line1[2];
        D.y = ped_line1[3];
        E.x = width - ped_trapezoid[6];
        E.y = ped_trapezoid[7];
        F.x = width - ped_trapezoid[4];
        F.y = ped_trapezoid[5];
    }

    bool flag1 = isPointInParallelogram(A, B, C, D, p);
    bool flag2 = isPointInParallelogram(C, D, E, F, p);
    return flag1 || flag2;
}

bool Calculator::box_inside(const std::array<float, 4>& det) {
    bool flag = false;
    // flag = flag || point_inside(det.at(0), det.at(1));
    // flag = flag || point_inside(det.at(2), det.at(1));
    // flag = flag || point_inside(det.at(0), det.at(3));
    // flag = flag || point_inside(det.at(2), det.at(3));
    float x1 = fmax(0, det[0]);
    float x2 = fmin(width, det[2]);
    float y1 = fmax(0, det[1]);
    float y2 = fmin(height, det[3]);

    Point p1(x1, y1);
    Point p2(x2, y1);
    Point p3(x2, y2);
    Point p4(x1, y2);
    Point p5((x1 + x2) * 0.5, y2);
    // flag = flag || point_inside(p1);
    // flag = flag || point_inside(p2);
    // flag = flag || point_inside(p3);
    // flag = flag || point_inside(p4);
    flag = flag || point_inside(p5);
    return flag;
}


void Calculator::Release() {
  map_x.clear();
  map_z.clear();
  pedestrians.clear();
  match_idxs.clear();
  close(fd);
}
