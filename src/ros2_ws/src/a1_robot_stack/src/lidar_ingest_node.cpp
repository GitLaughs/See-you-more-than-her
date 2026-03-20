#include <algorithm>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>

#ifdef RPLIDAR_SDK_PRESENT
#include "rplidar.h"
#endif

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
    use_scan_topic_ = this->declare_parameter<bool>("use_scan_topic", true);
    use_rplidar_sdk_ = this->declare_parameter<bool>("use_rplidar_sdk", false);
    scan_topic_ = this->declare_parameter<std::string>("scan_topic", "/scan");
    serial_port_ = this->declare_parameter<std::string>("serial_port", "/dev/ttyUSB0");
    serial_baud_ = this->declare_parameter<int>("serial_baud", 230400);
    serial_poll_ms_ = this->declare_parameter<int>("serial_poll_ms", 20);

    if (use_scan_topic_) {
      sub_scan_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
        scan_topic_, rclcpp::SensorDataQoS(),
        std::bind(&LidarIngestNode::on_scan, this, std::placeholders::_1));
    } else {
      open_serial_or_throw();
      poll_timer_ = this->create_wall_timer(
        std::chrono::milliseconds(serial_poll_ms_), std::bind(&LidarIngestNode::poll_serial, this));
    }

    pub_closest_ = this->create_publisher<std_msgs::msg::Float32>("/perception/lidar_closest_m", 10);
    pub_obstacle_ = this->create_publisher<std_msgs::msg::Bool>("/perception/lidar_obstacle", 10);
    pub_fault_ = this->create_publisher<diagnostic_msgs::msg::DiagnosticStatus>("/system/fault", 20);
    pub_heartbeat_ = this->create_publisher<std_msgs::msg::UInt64>("/system/heartbeat/lidar", 10);
  }

  ~LidarIngestNode() override {
    if (serial_fd_ >= 0) {
      close(serial_fd_);
      serial_fd_ = -1;
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

  void open_serial_or_throw() {
    serial_fd_ = open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd_ < 0) {
      throw std::runtime_error("failed to open lidar serial: " + serial_port_ + " err=" + std::strerror(errno));
    }

    termios tty {};
    if (tcgetattr(serial_fd_, &tty) != 0) {
      throw std::runtime_error("tcgetattr failed for lidar serial");
    }

    cfsetospeed(&tty, baud_to_termios(serial_baud_));
    cfsetispeed(&tty, baud_to_termios(serial_baud_));
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
      throw std::runtime_error("tcsetattr failed for lidar serial");
    }
  }

  void publish_distance(float closest) {
    std_msgs::msg::Float32 closest_msg;
    closest_msg.data = closest;
    pub_closest_->publish(closest_msg);

    std_msgs::msg::Bool obs_msg;
    obs_msg.data = closest < obstacle_threshold_;
    pub_obstacle_->publish(obs_msg);

    std_msgs::msg::UInt64 hb;
    hb.data = ++seq_;
    pub_heartbeat_->publish(hb);
  }

  void poll_serial() {
    if (serial_fd_ < 0) {
      return;
    }

    char buf[128] = {0};
    const ssize_t n = read(serial_fd_, buf, sizeof(buf) - 1);
    if (n <= 0) {
      return;
    }
    serial_buffer_.append(buf, static_cast<size_t>(n));

    size_t pos = 0;
    while ((pos = serial_buffer_.find('\n')) != std::string::npos) {
      std::string line = serial_buffer_.substr(0, pos);
      serial_buffer_.erase(0, pos + 1);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }

      std::istringstream iss(line);
      float value = 0.0f;
      if (iss >> value && std::isfinite(value) && value > 0.0f) {
        publish_distance(value);
      }
    }
  }

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
      publish_distance(closest);
    } catch (const std::exception & e) {
      pub_fault_->publish(a1_robot_stack::make_fault_status(
        this->get_name(), a1_robot_stack::FaultDomain::CameraData, e.what(),
        diagnostic_msgs::msg::DiagnosticStatus::WARN));
    }
  }

  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr sub_scan_;
  rclcpp::TimerBase::SharedPtr poll_timer_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr pub_closest_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pub_obstacle_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticStatus>::SharedPtr pub_fault_;
  rclcpp::Publisher<std_msgs::msg::UInt64>::SharedPtr pub_heartbeat_;

  double obstacle_threshold_{0.7};
  bool use_scan_topic_{true};
  bool use_rplidar_sdk_{false};
  std::string scan_topic_;
  std::string serial_port_;
  int serial_baud_{230400};
  int serial_poll_ms_{20};
  int serial_fd_{-1};
  std::string serial_buffer_;
  uint64_t seq_{0};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LidarIngestNode>());
  rclcpp::shutdown();
  return 0;
}
