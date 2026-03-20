#pragma once

#include <string>
#include <vector>

#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"

namespace a1_robot_stack {

enum class FaultDomain {
  CameraData,
  Inference,
  Resource
};

inline std::string domain_to_string(FaultDomain domain) {
  switch (domain) {
    case FaultDomain::CameraData:
      return "camera_data";
    case FaultDomain::Inference:
      return "inference";
    case FaultDomain::Resource:
      return "resource";
    default:
      return "unknown";
  }
}

inline diagnostic_msgs::msg::DiagnosticStatus make_fault_status(
  const std::string & node,
  FaultDomain domain,
  const std::string & message,
  int8_t level,
  const std::vector<diagnostic_msgs::msg::KeyValue> & extras = {}) {
  diagnostic_msgs::msg::DiagnosticStatus status;
  status.name = node;
  status.hardware_id = "flying_a1";
  status.level = level;
  status.message = message;

  diagnostic_msgs::msg::KeyValue domain_kv;
  domain_kv.key = "domain";
  domain_kv.value = domain_to_string(domain);
  status.values.push_back(domain_kv);

  for (const auto & kv : extras) {
    status.values.push_back(kv);
  }

  return status;
}

}  // 命名空间 a1_robot_stack
