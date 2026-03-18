#include <memory>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/u_int64.hpp"

#include "a1_robot_stack/fault_utils.hpp"

class ChassisControllerNode : public rclcpp::Node {
 public:
  ChassisControllerNode() : Node("chassis_controller_node") {
    max_linear_ = this->declare_parameter<double>("max_linear", 0.35);
    max_angular_ = this->declare_parameter<double>("max_angular", 1.0);

    sub_safe_cmd_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel_safe", 20, std::bind(&ChassisControllerNode::on_cmd, this, std::placeholders::_1));

    pub_out_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("/chassis/cmd_out", 20);
    pub_fault_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/system/fault", 20);
    pub_heartbeat_ = this->create_publisher<std_msgs::msg::UInt64>("/system/heartbeat/chassis", 10);
  }

 private:
  void on_cmd(const geometry_msgs::msg::Twist::SharedPtr msg) {
    try {
      geometry_msgs::msg::Twist out = *msg;
      out.linear.x = std::max(-max_linear_, std::min(max_linear_, out.linear.x));
      out.angular.z = std::max(-max_angular_, std::min(max_angular_, out.angular.z));

      // TODO: replace this publish with UART/CAN command output on A1 board.
      pub_out_cmd_->publish(out);

      std_msgs::msg::UInt64 hb;
      hb.data = ++seq_;
      pub_heartbeat_->publish(hb);
    } catch (const std::bad_alloc & e) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::Resource, e.what(),
        diagnostic_msgs::msg::DiagnosticStatus::ERROR));
    } catch (const std::exception & e) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::Inference, e.what(),
        diagnostic_msgs::msg::DiagnosticStatus::ERROR));
    }
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_safe_cmd_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_out_cmd_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr pub_fault_;
  rclcpp::Publisher<std_msgs::msg::UInt64>::SharedPtr pub_heartbeat_;

  double max_linear_{0.35};
  double max_angular_{1.0};
  uint64_t seq_{0};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ChassisControllerNode>());
  rclcpp::shutdown();
  return 0;
}
