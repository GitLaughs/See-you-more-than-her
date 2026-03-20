#include <chrono>
#include <cmath>
#include <cstdint>
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
    camera_topic_ = this->declare_parameter<std::string>("camera_topic", "/a1/camera/mono");
    camera_timeout_sec_ = this->declare_parameter<double>("camera_timeout_sec", 0.5);

    pub_nav_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("/navigation/desired_cmd", 10);
    pub_obstacle_ = this->create_publisher<std_msgs::msg::Bool>("/perception/obstacle", 10);
    pub_depth_ = this->create_publisher<std_msgs::msg::Float32>("/perception/depth_m", 10);
    pub_gesture_ = this->create_publisher<std_msgs::msg::String>("/perception/gesture", 10);
    pub_fps_ = this->create_publisher<std_msgs::msg::Float32>("/metrics/vision_fps", 10);
    pub_fault_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/system/fault", 20);
    pub_heartbeat_ = this->create_publisher<std_msgs::msg::UInt64>("/system/heartbeat/vision", 10);

    if (use_mock_input_) {
      timer_ = this->create_wall_timer(
        std::chrono::milliseconds(static_cast<int>(1000.0 / target_fps_)),
        std::bind(&PerceptionNode::on_timer, this));
    } else {
      sub_camera_ = this->create_subscription<sensor_msgs::msg::Image>(
        camera_topic_, rclcpp::SensorDataQoS(),
        std::bind(&PerceptionNode::on_camera_image, this, std::placeholders::_1));
      timer_ = this->create_wall_timer(100ms, std::bind(&PerceptionNode::camera_watchdog, this));
      last_camera_frame_time_ = this->now();
      prev_camera_frame_time_ = last_camera_frame_time_;
    }

    RCLCPP_INFO(
      this->get_logger(),
      "Perception node ready, model=%s, mock=%s, camera_topic=%s",
      model_path_.c_str(), use_mock_input_ ? "true" : "false", camera_topic_.c_str());
  }

 private:
  void publish_perception_result(float pseudo_depth, bool obstacle, const std::string & gesture) {
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
  }

  void on_timer() {
    const auto t0 = this->now();
    try {
      frame_id_++;
      const float pseudo_depth = 0.4f + 1.6f * std::fabs(std::sin(frame_id_ * 0.02));
      const bool obstacle = pseudo_depth < 0.8f;
      const std::string gesture = ((frame_id_ / 40) % 2 == 0) ? "OPEN_PALM" : "FIST";

      publish_perception_result(pseudo_depth, obstacle, gesture);

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

  void on_camera_image(const sensor_msgs::msg::Image::SharedPtr msg) {
    try {
      if (msg->data.empty()) {
        throw std::runtime_error("camera frame is empty");
      }

      last_camera_frame_time_ = this->now();
      ++frame_id_;

      // 在板端模型全量接入前，先用灰度均值作为轻量代理信号。
      uint64_t sum = 0;
      for (uint8_t value : msg->data) {
        sum += value;
      }
      const float mean_gray = static_cast<float>(sum) / static_cast<float>(msg->data.size());
      const float pseudo_depth = 0.3f + (255.0f - mean_gray) / 255.0f * 1.8f;
      const bool obstacle = pseudo_depth < 0.8f;
      const std::string gesture = obstacle ? "STOP" : "GO";

      publish_perception_result(pseudo_depth, obstacle, gesture);

      std_msgs::msg::Float32 fps_msg;
      const auto now = this->now();
      const auto dt = (now - prev_camera_frame_time_).seconds();
      fps_msg.data = (dt > 1e-6) ? static_cast<float>(1.0 / dt) : 0.0f;
      prev_camera_frame_time_ = now;
      pub_fps_->publish(fps_msg);

      std_msgs::msg::UInt64 hb;
      hb.data = frame_id_;
      pub_heartbeat_->publish(hb);
    } catch (const std::exception & e) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::CameraData, e.what(),
        diagnostic_msgs::msg::DiagnosticStatus::WARN));
    }
  }

  void camera_watchdog() {
    if (use_mock_input_) {
      return;
    }

    const auto age = (this->now() - last_camera_frame_time_).seconds();
    if (age > camera_timeout_sec_) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::CameraData,
        "camera timeout on topic: " + camera_topic_, diagnostic_msgs::msg::DiagnosticStatus::WARN));
    }
  }

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_camera_;
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
  std::string camera_topic_;
  double camera_timeout_sec_{0.5};
  rclcpp::Time last_camera_frame_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time prev_camera_frame_time_{0, 0, RCL_ROS_TIME};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PerceptionNode>());
  rclcpp::shutdown();
  return 0;
}
