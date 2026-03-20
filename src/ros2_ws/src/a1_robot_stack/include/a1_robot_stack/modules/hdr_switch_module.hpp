#pragma once

namespace a1_robot_stack::modules {

class HdrSwitchModule {
 public:
  void Init();
  void UpdateByIllumination(float lux);
};

}  // 命名空间 a1_robot_stack::modules
