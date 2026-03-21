#include "../include/lidar_sdk_adapter.hpp"

#include <cmath>

namespace ssne_demo {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

bool RplidarSdkAdapter::Configure(const std::string& serial_port, int baudrate) {
  serial_port_ = serial_port;
  baudrate_ = baudrate;
  configured_ = !serial_port_.empty() && baudrate_ > 0;
  return configured_;
}

bool RplidarSdkAdapter::ConnectDriver() {
  if (driver_ != nullptr) {
    return true;
  }

  driver_ = rp::standalone::rplidar::RPlidarDriver::CreateDriver();
  if (driver_ == nullptr) {
    return false;
  }

  const auto connect_result = driver_->connect(serial_port_.c_str(), static_cast<sl_u32>(baudrate_), 0);
  if (!IS_OK(connect_result)) {
    ReleaseDriver();
    return false;
  }

  return true;
}

bool RplidarSdkAdapter::Start() {
  if (!configured_ || !ConnectDriver()) {
    running_ = false;
    return false;
  }

  if (!IS_OK(driver_->startMotor())) {
    ReleaseDriver();
    running_ = false;
    return false;
  }

  if (!IS_OK(driver_->startScan(false, true, 0, nullptr))) {
    ReleaseDriver();
    running_ = false;
    return false;
  }

  running_ = true;
  return running_;
}

std::vector<LidarSample> RplidarSdkAdapter::ScanOnce() {
  if (!running_) {
    return {};
  }

  constexpr size_t kMaxNodes = rp::standalone::rplidar::RPlidarDriver::MAX_SCAN_NODES;
  sl_lidar_response_measurement_node_hq_t nodes[kMaxNodes];
  size_t count = kMaxNodes;

  const auto result = driver_->grabScanDataHq(nodes, count, 0);
  if (!IS_OK(result) || count == 0) {
    return {};
  }

  driver_->ascendScanData(nodes, count);

  std::vector<LidarSample> samples;
  samples.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    const float angle_deg = static_cast<float>(nodes[index].angle_z_q14) * 90.0f / 16384.0f;
    const float distance_m = static_cast<float>(nodes[index].dist_mm_q2) / 4000.0f;
    LidarSample sample;
    sample.angle_deg = angle_deg * kPi / 180.0f;
    sample.distance_m = distance_m;
    sample.quality = static_cast<float>(nodes[index].quality);
    samples.push_back(sample);
  }

  return samples;
}

void RplidarSdkAdapter::ReleaseDriver() {
  if (driver_ == nullptr) {
    return;
  }

  driver_->stop();
  driver_->stopMotor();
  driver_->disconnect();
  rp::standalone::rplidar::RPlidarDriver::DisposeDriver(driver_);
  driver_ = nullptr;
}

void RplidarSdkAdapter::Stop() {
  running_ = false;
  ReleaseDriver();
}

}  // namespace ssne_demo