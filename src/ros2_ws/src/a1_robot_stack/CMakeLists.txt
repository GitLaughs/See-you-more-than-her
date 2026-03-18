cmake_minimum_required(VERSION 3.8)
project(a1_robot_stack)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)

include_directories(include)

add_executable(perception_node src/perception_node.cpp)
ament_target_dependencies(perception_node rclcpp std_msgs sensor_msgs geometry_msgs diagnostic_msgs)

add_executable(lidar_ingest_node src/lidar_ingest_node.cpp)
ament_target_dependencies(lidar_ingest_node rclcpp std_msgs sensor_msgs geometry_msgs diagnostic_msgs)

add_executable(chassis_controller_node src/chassis_controller_node.cpp)
ament_target_dependencies(chassis_controller_node rclcpp std_msgs sensor_msgs geometry_msgs diagnostic_msgs)

add_executable(safety_supervisor_node src/safety_supervisor_node.cpp)
ament_target_dependencies(safety_supervisor_node rclcpp std_msgs sensor_msgs geometry_msgs diagnostic_msgs)

add_executable(performance_monitor_node src/performance_monitor_node.cpp)
ament_target_dependencies(performance_monitor_node rclcpp std_msgs sensor_msgs geometry_msgs diagnostic_msgs)

install(TARGETS
  perception_node
  lidar_ingest_node
  chassis_controller_node
  safety_supervisor_node
  performance_monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
