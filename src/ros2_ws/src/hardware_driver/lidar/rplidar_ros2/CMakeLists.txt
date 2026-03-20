cmake_minimum_required(VERSION 3.8)
project(rplidar_ros2)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

set(RPLIDAR_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../rplidar_ros_upstream/sdk" CACHE PATH "Path to the official Slamtec RPLIDAR SDK")

if(NOT EXISTS "${RPLIDAR_SDK_DIR}/include/rplidar.h")
  message(FATAL_ERROR "RPLIDAR SDK not found at ${RPLIDAR_SDK_DIR}")
endif()

file(GLOB RPLIDAR_SDK_SRC
  "${RPLIDAR_SDK_DIR}/src/arch/linux/*.cpp"
  "${RPLIDAR_SDK_DIR}/src/dataunpacker/*.cpp"
  "${RPLIDAR_SDK_DIR}/src/dataunpacker/unpacker/*.cpp"
  "${RPLIDAR_SDK_DIR}/src/hal/*.cpp"
  "${RPLIDAR_SDK_DIR}/src/*.cpp"
)

add_executable(rplidar_ros2_node src/rplidar_ros2_node.cpp ${RPLIDAR_SDK_SRC})

target_include_directories(rplidar_ros2_node PRIVATE
  ${RPLIDAR_SDK_DIR}/include
  ${RPLIDAR_SDK_DIR}/src
)

ament_target_dependencies(rplidar_ros2_node
  rclcpp
  sensor_msgs
)

target_link_libraries(rplidar_ros2_node pthread dl)

install(TARGETS rplidar_ros2_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

ament_package()
