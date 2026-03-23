cmake_minimum_required(VERSION 3.5)
project(wheeltec_multi)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
#find_package(actionlib REQUIRED)
#find_package(actionlib_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

find_package(nav2_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclpy REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(std_srvs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(turn_on_wheeltec_robot REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # uncomment the line when a copyright and license is not present in all source files
  #set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # uncomment the line when this package is not in a git repo
  #set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

include_directories(
  include
)

add_executable(multi_slave src/slave_tf_listener.cpp)
ament_target_dependencies(multi_slave tf2_ros tf2 tf2_geometry_msgs geometry_msgs rclcpp std_msgs nav_msgs std_srvs sensor_msgs)
install(TARGETS multi_slave DESTINATION  lib/${PROJECT_NAME})

add_executable(multi_avoidance src/multi_avoidance.cpp)
ament_target_dependencies(multi_avoidance tf2_ros tf2 tf2_geometry_msgs rclcpp geometry_msgs std_msgs nav_msgs std_srvs sensor_msgs turn_on_wheeltec_robot)
install(TARGETS multi_avoidance DESTINATION  lib/${PROJECT_NAME})

install(
  DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)
install(PROGRAMS scripts/listen_tfodom.py scripts/send_tfodom.py scripts/set_pose.py scripts/wheeltec_keyboard.py DESTINATION lib/${PROJECT_NAME})

ament_package()
