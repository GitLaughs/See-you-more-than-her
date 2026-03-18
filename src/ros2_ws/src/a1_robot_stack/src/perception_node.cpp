#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/u_int64.hpp"

#include "a1_robot_stack/fault_utils.hpp"

using namespace std::chrono_literals;

class PerceptionNode : public rclcpp::Node {
 public:
  PerceptionNode() : Node("perception_node") {
    target_fps_ = this->declare_parameter<double>("target_fps", 25.0);
    model_path_ = this->declare_parameter<std::string>("model_path", "/app/models/yolo.onnx");
    use_mock_input_ = this->declare_parameter<bool>("use_mock_input", true);

    pub_nav_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("/navigation/desired_cmd", 10);
    pub_obstacle_ = this->create_publisher<std_msgs::msg::Bool>("/perception/obstacle", 10);
    pub_depth_ = this->create_publisher<std_msgs::msg::Float32>("/perception/depth_m", 10);
    pub_gesture_ = this->create_publisher<std_msgs::msg::String>("/perception/gesture", 10);
    pub_fps_ = this->create_publisher<std_msgs::msg::Float32>("/metrics/vision_fps", 10);
    pub_fault_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/system/fault", 20);
    pub_heartbeat_ = this->create_publisher<std_msgs::msg::UInt64>("/system/heartbeat/vision", 10);

    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(static_cast<int>(1000.0 / target_fps_)),
      std::bind(&PerceptionNode::on_timer, this));

    RCLCPP_INFO(this->get_logger(), "Perception node ready, model=%s", model_path_.c_str());
  }

 private:
  void on_timer() {
    const auto t0 = this->now();
    try {
      frame_id_++;
      if (!use_mock_input_) {
        throw std::runtime_error("A1 camera adapter not wired yet");
      }

      const float pseudo_depth = 0.4f + 1.6f * std::fabs(std::sin(frame_id_ * 0.02));
      const bool obstacle = pseudo_depth < 0.8f;
      const std::string gesture = ((frame_id_ / 40) % 2 == 0) ? "OPEN_PALM" : "FIST";

      geometry_msgs::msg::Twist desired;
      desired.linear.x = obstacle ? 0.0 : 0.25;
      desired.angular.z = obstacle ? 0.5 : 0.0;
      pub_nav_cmd_->publish(desired);

      std_msgs::msg::Bool obs_msg;
      obs_msg.data = obstacle;
      pub_obstacle_->publish(obs_msg);

      std_msgs::msg::Float32 depth_msg;
      depth_msg.data = pseudo_depth;
      pub_depth_->publish(depth_msg);

      std_msgs::msg::String gesture_msg;
      gesture_msg.data = gesture;
      pub_gesture_->publish(gesture_msg);

      const auto dt = (this->now() - t0).seconds();
      std_msgs::msg::Float32 fps_msg;
      fps_msg.data = dt > 1e-6 ? static_cast<float>(1.0 / dt) : 0.0f;
      pub_fps_->publish(fps_msg);

      std_msgs::msg::UInt64 hb;
      hb.data = frame_id_;
      pub_heartbeat_->publish(hb);
    } catch (const std::bad_alloc & e) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::Resource, e.what(),
        diagnostic_msgs::msg::DiagnosticStatus::ERROR));
    } catch (const std::runtime_error & e) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::CameraData, e.what(),
        diagnostic_msgs::msg::DiagnosticStatus::WARN));
    } catch (const std::exception & e) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::Inference, e.what(),
        diagnostic_msgs::msg::DiagnosticStatus::ERROR));
    }
  }

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_nav_cmd_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pub_obstacle_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr pub_depth_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_gesture_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr pub_fps_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr pub_fault_;
  rclcpp::Publisher<std_msgs::msg::UInt64>::SharedPtr pub_heartbeat_;

  uint64_t frame_id_{0};
  double target_fps_{25.0};
  std::string model_path_;
  bool use_mock_input_{true};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PerceptionNode>());
  rclcpp::shutdown();
  return 0;
}
