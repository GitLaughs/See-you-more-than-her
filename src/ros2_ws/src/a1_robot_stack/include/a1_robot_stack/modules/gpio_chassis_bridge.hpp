#pragma once

namespace a1_robot_stack::modules {

class GpioChassisBridge {
 public:
  void Init();
  void WriteMotion(float linear, float angular);
};

}  // 命名空间 a1_robot_stack::modules
