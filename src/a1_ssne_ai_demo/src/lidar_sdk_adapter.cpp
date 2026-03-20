#include "../include/lidar_sdk_adapter.hpp"

namespace ssne_demo {

bool RplidarSdkAdapter::Configure(const std::string& serial_port, int baudrate) {
  serial_port_ = serial_port;
  baudrate_ = baudrate;
  configured_ = !serial_port_.empty() && baudrate_ > 0;
  return configured_;
}

bool RplidarSdkAdapter::Start() {
  running_ = configured_;
  return running_;
}

std::vector<LidarSample> RplidarSdkAdapter::ScanOnce() {
  if (!running_) {
    return {};
  }
  return {};
}

void RplidarSdkAdapter::Stop() { running_ = false; }

}  // namespace ssne_demo