#pragma once

namespace a1_robot_stack::modules {

class GpioChassisBridge {
 public:
  void Init();
  void WriteMotion(float linear, float angular);
};

}  // namespace a1_robot_stack::modules
