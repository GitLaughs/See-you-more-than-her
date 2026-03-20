#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>

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
    use_uart_output_ = this->declare_parameter<bool>("use_uart_output", false);
    uart_port_ = this->declare_parameter<std::string>("uart_port", "/dev/ttyS0");
    uart_baud_ = this->declare_parameter<int>("uart_baud", 115200);

    sub_safe_cmd_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel_safe", 20, std::bind(&ChassisControllerNode::on_cmd, this, std::placeholders::_1));

    pub_out_cmd_ = this->create_publisher<geometry_msgs::msg::Twist>("/chassis/cmd_out", 20);
    pub_fault_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/system/fault", 20);
    pub_heartbeat_ = this->create_publisher<std_msgs::msg::UInt64>("/system/heartbeat/chassis", 10);

    if (use_uart_output_) {
      open_uart_or_throw();
      RCLCPP_INFO(this->get_logger(), "chassis UART enabled: %s @ %d", uart_port_.c_str(), uart_baud_);
    }
  }

  ~ChassisControllerNode() override {
    if (uart_fd_ >= 0) {
      close(uart_fd_);
      uart_fd_ = -1;
    }
  }

 private:
  static speed_t baud_to_termios(int baud) {
    switch (baud) {
      case 9600: return B9600;
      case 19200: return B19200;
      case 38400: return B38400;
      case 57600: return B57600;
      case 115200: return B115200;
      case 230400: return B230400;
      default: return B115200;
    }
  }

  void open_uart_or_throw() {
    uart_fd_ = open(uart_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (uart_fd_ < 0) {
      throw std::runtime_error("failed to open chassis uart: " + uart_port_ + " err=" + std::strerror(errno));
    }

    termios tty {};
    if (tcgetattr(uart_fd_, &tty) != 0) {
      throw std::runtime_error("tcgetattr failed for chassis uart");
    }

    cfsetospeed(&tty, baud_to_termios(uart_baud_));
    cfsetispeed(&tty, baud_to_termios(uart_baud_));
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(uart_fd_, TCSANOW, &tty) != 0) {
      throw std::runtime_error("tcsetattr failed for chassis uart");
    }
  }

  static std::string make_uart_frame(double linear_x, double angular_z) {
    std::ostringstream oss;
    // 命令帧限制在 32 字节以内，避免超过 A1 UART FIFO 后出现分段写抖动。
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss.precision(2);
    oss << "VX" << linear_x << ",WZ" << angular_z << "\n";
    std::string frame = oss.str();
    if (frame.size() > 32) {
      frame.resize(32);
      frame.back() = '\n';
    }
    return frame;
  }

  void send_uart(double linear_x, double angular_z) {
    if (!use_uart_output_ || uart_fd_ < 0) {
      return;
    }

    const std::string frame = make_uart_frame(linear_x, angular_z);
    const ssize_t written = write(uart_fd_, frame.data(), frame.size());
    if (written < 0) {
      throw std::runtime_error("uart write failed: " + std::string(std::strerror(errno)));
    }
  }

  void on_cmd(const geometry_msgs::msg::Twist::SharedPtr msg) {
    try {
      geometry_msgs::msg::Twist out = *msg;
      out.linear.x = std::max(-max_linear_, std::min(max_linear_, out.linear.x));
      out.angular.z = std::max(-max_angular_, std::min(max_angular_, out.angular.z));

      send_uart(out.linear.x, out.angular.z);
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
  bool use_uart_output_{false};
  std::string uart_port_;
  int uart_baud_{115200};
  int uart_fd_{-1};
  uint64_t seq_{0};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ChassisControllerNode>());
  rclcpp::shutdown();
  return 0;
}
