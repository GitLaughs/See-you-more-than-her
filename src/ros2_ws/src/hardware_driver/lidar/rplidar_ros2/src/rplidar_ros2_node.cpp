#include <cmath>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

#include "rplidar.h"

using namespace rp::standalone::rplidar;

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

class RplidarRos2Node : public rclcpp::Node {
 public:
  RplidarRos2Node() : Node("rplidar_ros2_node") {
    serial_port_ = declare_parameter<std::string>("port", "/dev/ttyUSB0");
    baudrate_ = declare_parameter<int>("baudrate", 115200);
    frame_id_ = declare_parameter<std::string>("frame_id", "laser_frame");
    range_min_ = declare_parameter<double>("range_min", 0.1);
    range_max_ = declare_parameter<double>("range_max", 64.0);
    publish_rate_hz_ = declare_parameter<double>("publish_rate_hz", 10.0);

    publisher_ = create_publisher<sensor_msgs::msg::LaserScan>("scan", rclcpp::SensorDataQoS());

    driver_ = RPlidarDriver::CreateDriver();
    if (driver_ == nullptr) {
      throw std::runtime_error("Failed to create RPlidar driver");
    }

    const auto connect_result = driver_->connect(serial_port_.c_str(), static_cast<sl_u32>(baudrate_), 0);
    if (!IS_OK(connect_result)) {
      RPlidarDriver::DisposeDriver(driver_);
      driver_ = nullptr;
      throw std::runtime_error("Failed to connect to RPLidar device");
    }

    if (!IS_OK(driver_->startMotor())) {
      driver_->disconnect();
      RPlidarDriver::DisposeDriver(driver_);
      driver_ = nullptr;
      throw std::runtime_error("Failed to start RPLidar motor");
    }

    if (!IS_OK(driver_->startScan(false, true, 0, nullptr))) {
      driver_->stopMotor();
      driver_->disconnect();
      RPlidarDriver::DisposeDriver(driver_);
      driver_ = nullptr;
      throw std::runtime_error("Failed to start RPLidar scan");
    }

    if (publish_rate_hz_ <= 0.0) {
      publish_rate_hz_ = 10.0;
    }

    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(1.0 / publish_rate_hz_)),
      std::bind(&RplidarRos2Node::PublishScan, this));
  }

  ~RplidarRos2Node() override {
    if (driver_ != nullptr) {
      driver_->stop();
      driver_->stopMotor();
      driver_->disconnect();
      RPlidarDriver::DisposeDriver(driver_);
      driver_ = nullptr;
    }
  }

 private:
  void PublishScan() {
    if (driver_ == nullptr) {
      return;
    }

    constexpr size_t kMaxNodes = RPlidarDriver::MAX_SCAN_NODES;
    sl_lidar_response_measurement_node_hq_t nodes[kMaxNodes];
    size_t count = kMaxNodes;

    const auto result = driver_->grabScanDataHq(nodes, count, 0);
    if (!IS_OK(result) || count == 0) {
      return;
    }

    driver_->ascendScanData(nodes, count);

    auto scan_msg = std::make_shared<sensor_msgs::msg::LaserScan>();
    scan_msg->header.stamp = now();
    scan_msg->header.frame_id = frame_id_;
    scan_msg->angle_min = 0.0f;
    scan_msg->angle_max = 2.0f * kPi;
    scan_msg->angle_increment = (scan_msg->angle_max - scan_msg->angle_min) / 3600.0f;
    scan_msg->range_min = static_cast<float>(range_min_);
    scan_msg->range_max = static_cast<float>(range_max_);
    scan_msg->ranges.assign(3600, std::numeric_limits<float>::infinity());
    scan_msg->intensities.assign(3600, 0.0f);

    for (size_t index = 0; index < count; ++index) {
      const float angle_deg = static_cast<float>(nodes[index].angle_z_q14) * 90.0f / 16384.0f;
      const float angle_rad = angle_deg * kPi / 180.0f;
      const float distance_m = static_cast<float>(nodes[index].dist_mm_q2) / 4000.0f;
      if (distance_m < scan_msg->range_min || distance_m > scan_msg->range_max) {
        continue;
      }

      const auto slot = static_cast<size_t>(angle_rad / scan_msg->angle_increment);
      if (slot < scan_msg->ranges.size()) {
        scan_msg->ranges[slot] = distance_m;
        scan_msg->intensities[slot] = static_cast<float>(nodes[index].quality);
      }
    }

    publisher_->publish(*scan_msg);
  }

  std::string serial_port_;
  int baudrate_{0};
  std::string frame_id_;
  double range_min_{0.1};
  double range_max_{64.0};
  double publish_rate_hz_{10.0};

  RPlidarDriver* driver_{nullptr};
  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<RplidarRos2Node>();
    rclcpp::spin(node);
  } catch (const std::exception& exception) {
    RCLCPP_FATAL(rclcpp::get_logger("rplidar_ros2_node"), "%s", exception.what());
  }
  rclcpp::shutdown();
  return 0;
}
