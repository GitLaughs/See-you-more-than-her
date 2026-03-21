#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "rplidar.h"

namespace ssne_demo {

struct LidarSample {
  float angle_deg{0.0f};
  float distance_m{0.0f};
  float quality{0.0f};
};

class RplidarSdkAdapter {
 public:
  bool Configure(const std::string& serial_port, int baudrate);
  bool Start();
  std::vector<LidarSample> ScanOnce();
  void Stop();

  bool configured() const { return configured_; }
  bool ConnectDriver();
  void ReleaseDriver();


 private:
  std::string serial_port_;
  int baudrate_{0};
  rp::standalone::rplidar::RPlidarDriver* driver_{nullptr};
  bool configured_{false};
  bool running_{false};
};

}  // namespace ssne_demo