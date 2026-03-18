#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/u_int64.hpp"

#include "a1_robot_stack/fault_utils.hpp"

class LidarIngestNode : public rclcpp::Node {
 public:
  LidarIngestNode() : Node("lidar_ingest_node") {
    obstacle_threshold_ = this->declare_parameter<double>("obstacle_threshold_m", 0.7);

    sub_scan_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SensorDataQoS(),
      std::bind(&LidarIngestNode::on_scan, this, std::placeholders::_1));

    pub_closest_ = this->create_publisher<std_msgs::msg::Float32>("/perception/lidar_closest_m", 10);
    pub_obstacle_ = this->create_publisher<std_msgs::msg::Bool>("/perception/lidar_obstacle", 10);
    pub_fault_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/system/fault", 20);
    pub_heartbeat_ = this->create_publisher<std_msgs::msg::UInt64>("/system/heartbeat/lidar", 10);
  }

 private:
  void on_scan(const sensor_msgs::msg::LaserScan::SharedPtr msg) {
    try {
      if (msg->ranges.empty()) {
        throw std::runtime_error("empty laser scan");
      }

      float closest = std::numeric_limits<float>::infinity();
      for (float r : msg->ranges) {
        if (std::isfinite(r)) {
          closest = std::min(closest, r);
        }
      }

      if (!std::isfinite(closest)) {
        throw std::runtime_error("all lidar ranges invalid");
      }

      std_msgs::msg::Float32 closest_msg;
      closest_msg.data = closest;
      pub_closest_->publish(closest_msg);

      std_msgs::msg::Bool obs_msg;
      obs_msg.data = closest < obstacle_threshold_;
      pub_obstacle_->publish(obs_msg);

      std_msgs::msg::UInt64 hb;
      hb.data = ++seq_;
      pub_heartbeat_->publish(hb);
    } catch (const std::exception & e) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::CameraData, e.what(),
        diagnostic_msgs::msg::DiagnosticStatus::WARN));
    }
  }

  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr sub_scan_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr pub_closest_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pub_obstacle_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr pub_fault_;
  rclcpp::Publisher<std_msgs::msg::UInt64>::SharedPtr pub_heartbeat_;

  double obstacle_threshold_{0.7};
  uint64_t seq_{0};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LidarIngestNode>());
  rclcpp::shutdown();
  return 0;
}
