#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/u_int64.hpp"

using namespace std::chrono_literals;

class DisplayBridgeNode : public rclcpp::Node {
 public:
  DisplayBridgeNode() : Node("display_bridge_node") {
    status_file_path_ = this->declare_parameter<std::string>("status_file_path", "/tmp/a1_display_status.txt");
    publish_period_ms_ = this->declare_parameter<int>("publish_period_ms", 200);

    sub_depth_ = this->create_subscription<std_msgs::msg::Float32>(
      "/perception/depth_m", 20, [this](const std_msgs::msg::Float32::SharedPtr msg) {
        depth_m_ = msg->data;
      });
    sub_obstacle_ = this->create_subscription<std_msgs::msg::Bool>(
      "/perception/obstacle", 20, [this](const std_msgs::msg::Bool::SharedPtr msg) {
        obstacle_ = msg->data;
      });
    sub_gesture_ = this->create_subscription<std_msgs::msg::String>(
      "/perception/gesture", 20, [this](const std_msgs::msg::String::SharedPtr msg) {
        gesture_ = msg->data;
      });
    sub_alarm_ = this->create_subscription<std_msgs::msg::String>(
      "/system/alarm", 20, [this](const std_msgs::msg::String::SharedPtr msg) {
        alarm_ = msg->data;
      });

    pub_overlay_text_ = this->create_publisher<std_msgs::msg::String>("/display/overlay_text", 10);
    pub_heartbeat_ = this->create_publisher<std_msgs::msg::UInt64>("/system/heartbeat/display", 10);

    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(publish_period_ms_), std::bind(&DisplayBridgeNode::on_timer, this));
  }

 private:
  void on_timer() {
    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss.precision(2);
    oss << "depth=" << depth_m_ << "m";
    oss << " | obstacle=" << (obstacle_ ? "Y" : "N");
    oss << " | gesture=" << gesture_;
    if (!alarm_.empty()) {
      oss << " | alarm=" << alarm_;
    }

    std_msgs::msg::String out;
    out.data = oss.str();
    pub_overlay_text_->publish(out);

    std::ofstream fout(status_file_path_, std::ios::out | std::ios::trunc);
    if (fout.is_open()) {
      fout << out.data << std::endl;
    }

    std_msgs::msg::UInt64 hb;
    hb.data = ++seq_;
    pub_heartbeat_->publish(hb);
  }

  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr sub_depth_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr sub_obstacle_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_gesture_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_alarm_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_overlay_text_;
  rclcpp::Publisher<std_msgs::msg::UInt64>::SharedPtr pub_heartbeat_;
  rclcpp::TimerBase::SharedPtr timer_;

  std::string status_file_path_;
  int publish_period_ms_{200};
  float depth_m_{0.0f};
  bool obstacle_{false};
  std::string gesture_{"UNKNOWN"};
  std::string alarm_;
  uint64_t seq_{0};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DisplayBridgeNode>());
  rclcpp::shutdown();
  return 0;
}
