#pragma once

namespace a1_robot_stack::modules {

class YoloV8Trainer {
 public:
  void PlanDataset();
  void ExportOnnx();
};

}  // namespace a1_robot_stack::modules
