#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>
#include <numeric>
#include <vector>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"

class PerformanceMonitorNode : public rclcpp::Node {
 public:
  PerformanceMonitorNode() : Node("performance_monitor_node") {
    target_fps_ = this->declare_parameter<double>("target_fps", 25.0);

    sub_fps_ = this->create_subscription<std_msgs::msg::Float32>(
      "/metrics/vision_fps", 20, std::bind(&PerformanceMonitorNode::on_fps, this, std::placeholders::_1));
    pub_diag_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/system/perf_diag", 10);
  }

 private:
  void on_fps(const std_msgs::msg::Float32::SharedPtr msg) {
    if (!std::isfinite(msg->data) || msg->data <= 0.0f) {
      dropped_++;
      return;
    }

    total_++;
    fps_window_.push_back(msg->data);
    if (fps_window_.size() > kWindow) {
      fps_window_.pop_front();
    }

    if (fps_window_.size() < 10) {
      return;
    }

    std::vector<float> sorted(fps_window_.begin(), fps_window_.end());
    std::sort(sorted.begin(), sorted.end());

    const float mean = std::accumulate(sorted.begin(), sorted.end(), 0.0f) / static_cast<float>(sorted.size());
    const float p95 = sorted[static_cast<size_t>(0.95 * (sorted.size() - 1))];
    const float drop_rate = (total_ + dropped_) > 0 ? static_cast<float>(dropped_) / static_cast<float>(total_ + dropped_) : 0.0f;
    const float fluctuation = mean > 1e-6f ? std::fabs(p95 - mean) / mean : 1.0f;

    diagnostic_msgs::msg::DiagnosticStatus st;
    st.name = this->get_name();
    st.hardware_id = "flying_a1";
    st.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
    st.message = "perf_ok";

    if (drop_rate > 0.05f || fluctuation > 0.20f || mean < static_cast<float>(0.85 * target_fps_)) {
      st.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
      st.message = "perf_degraded";
    }

    diagnostic_msgs::msg::KeyValue kv1;
    kv1.key = "fps_mean";
    kv1.value = std::to_string(mean);
    st.values.push_back(kv1);

    diagnostic_msgs::msg::KeyValue kv2;
    kv2.key = "fps_p95";
    kv2.value = std::to_string(p95);
    st.values.push_back(kv2);

    diagnostic_msgs::msg::KeyValue kv3;
    kv3.key = "drop_rate";
    kv3.value = std::to_string(drop_rate);
    st.values.push_back(kv3);

    diagnostic_msgs::msg::KeyValue kv4;
    kv4.key = "fluctuation_ratio";
    kv4.value = std::to_string(fluctuation);
    st.values.push_back(kv4);

    pub_diag_->publish(st);
  }

  static constexpr size_t kWindow = 200;

  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr sub_fps_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr pub_diag_;

  std::deque<float> fps_window_;
  uint64_t total_{0};
  uint64_t dropped_{0};
  double target_fps_{25.0};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PerformanceMonitorNode>());
  rclcpp::shutdown();
  return 0;
}
