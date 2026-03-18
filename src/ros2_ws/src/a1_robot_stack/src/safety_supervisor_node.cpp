#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/u_int64.hpp"

using namespace std::chrono_literals;

class SafetySupervisorNode : public rclcpp::Node {
 public:
  SafetySupervisorNode() : Node("safety_supervisor_node") {
    heartbeat_timeout_sec_ = this->declare_parameter<double>("heartbeat_timeout_sec", 1.0);

    sub_nav_cmd_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/navigation/desired_cmd", 20,
      std::bind(&SafetySupervisorNode::on_nav_cmd, this, std::placeholders::_1));
    sub_fault_ = this->create_subscription<diagnostic_msgs::msg::DiagnosticStatus>(
      "/system/fault", 50, std::bind(&SafetySupervisorNode::on_fault, this, std::placeholders::_1));

    sub_hb_vision_ = this->create_subscription<std_msgs::msg::UInt64>(
      "/system/heartbeat/vision", 10, [this](const std_msgs::msg::UInt64::SharedPtr) {
        heartbeats_["vision"] = this->now();
      });
    sub_hb_lidar_ = this->create_subscription<std_msgs::msg::UInt64>(
      "/system/heartbeat/lidar", 10, [this](const std_msgs::msg::UInt64::SharedPtr) {
        heartbeats_["lidar"] = this->now();
      });
    sub_hb_chassis_ = this->create_subscription<std_msgs::msg::UInt64>(
      "/system/heartbeat/chassis", 10, [this](const std_msgs::msg::UInt64::SharedPtr) {
        heartbeats_["chassis"] = this->now();
      });

    pub_safe_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel_safe", 20);
    pub_alarm_ = this->create_publisher<std_msgs::msg::String>("/system/alarm", 20);

    const auto now = this->now();
    heartbeats_["vision"] = now;
    heartbeats_["lidar"] = now;
    heartbeats_["chassis"] = now;

    timer_ = this->create_wall_timer(100ms, std::bind(&SafetySupervisorNode::watchdog_tick, this));
  }

 private:
  void on_nav_cmd(const geometry_msgs::msg::Twist::SharedPtr msg) {
    last_cmd_ = *msg;
  }

  void on_fault(const diagnostic_msgs::msg::DiagnosticStatus::SharedPtr msg) {
    if (msg->level >= diagnostic_msgs::msg::DiagnosticStatus::ERROR) {
      emergency_stop_ = true;
      last_alarm_ = "fault:" + msg->name + " " + msg->message;
    }
  }

  void watchdog_tick() {
    const auto now = this->now();
    for (const auto & kv : heartbeats_) {
      const auto age = (now - kv.second).seconds();
      if (age > heartbeat_timeout_sec_) {
        emergency_stop_ = true;
        last_alarm_ = "heartbeat timeout: " + kv.first;
      }
    }

    geometry_msgs::msg::Twist out;
    if (!emergency_stop_) {
      out = last_cmd_;
    }
    pub_safe_cmd_->publish(out);

    if (emergency_stop_) {
      std_msgs::msg::String alarm;
      alarm.data = last_alarm_;
      pub_alarm_->publish(alarm);
    }
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_nav_cmd_;
  rclcpp::Subscription<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr sub_fault_;
  rclcpp::Subscription<std_msgs::msg::UInt64>::SharedPtr sub_hb_vision_;
  rclcpp::Subscription<std_msgs::msg::UInt64>::SharedPtr sub_hb_lidar_;
  rclcpp::Subscription<std_msgs::msg::UInt64>::SharedPtr sub_hb_chassis_;

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_safe_cmd_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_alarm_;
  rclcpp::TimerBase::SharedPtr timer_;

  std::unordered_map<std::string, rclcpp::Time> heartbeats_;
  geometry_msgs::msg::Twist last_cmd_;
  bool emergency_stop_{false};
  double heartbeat_timeout_sec_{1.0};
  std::string last_alarm_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SafetySupervisorNode>());
  rclcpp::shutdown();
  return 0;
}
