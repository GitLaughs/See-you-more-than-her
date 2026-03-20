#pragma once

namespace a1_robot_stack::modules {

class YoloV8Trainer {
 public:
  void PlanDataset();
  void ExportOnnx();
};

}  // 命名空间 a1_robot_stack::modules
