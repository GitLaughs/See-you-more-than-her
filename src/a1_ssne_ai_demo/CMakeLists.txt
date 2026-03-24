cmake_minimum_required(VERSION 3.0.0)

project(ssne_face_drive_demo)

# 引入路径配置文件
include(cmake_config/Paths.cmake)

set(RPLIDAR_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rplidar_sdk/sdk)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
add_compile_options(-Wno-psabi)

# 添加头文件目录
include_directories(${M1_SDK_INC_DIR})
include_directories(${M1_SDK_INC_DIR}/smartsoc)
include_directories(${RPLIDAR_SDK_DIR}/include)
include_directories(${RPLIDAR_SDK_DIR}/src)

set(SRC_DIR "src")
set(TARGET $ENV{BASE_DIR}/target)
include_directories(${CMAKE_SOURCE_DIR}/include)
message([<mode>] "message to display" ${TARGET})

# RPLidar SDK 源文件
file(GLOB RPLIDAR_SDK_SRC
    "${RPLIDAR_SDK_DIR}/src/arch/linux/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/dataunpacker/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/dataunpacker/unpacker/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/hal/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/*.cpp"
)

# 链接库
set(COMMON_LIBS
    ${M1_SSNE_LIB}
    ${M1_CMABUFFER_LIB}
    ${M1_OSD_LIB}
    ${M1_GPIO_LIB}
    ${M1_UART_LIB}
    ${M1_SSZLOG_LIB}
    ${M1_ZLOG_LIB}
    ${M1_EMB_LIB}
    pthread
    dl
)

# ==================== 人脸检测 + 底盘控制 ====================
add_executable(ssne_face_drive_demo ${PROJECT_SOURCE_DIR}/demo_face_drive.cpp)
file(GLOB FACE_DRIVE_SRC_FILES "${CMAKE_SOURCE_DIR}/${SRC_DIR}/*.cpp")
# 排除不需要的模块
list(FILTER FACE_DRIVE_SRC_FILES EXCLUDE REGEX "vision_app\\.cpp$")
list(FILTER FACE_DRIVE_SRC_FILES EXCLUDE REGEX "osd_visualizer\\.cpp$")
list(FILTER FACE_DRIVE_SRC_FILES EXCLUDE REGEX "debug_data_interface\\.cpp$")
list(FILTER FACE_DRIVE_SRC_FILES EXCLUDE REGEX "project_flow\\.cpp$")
list(FILTER FACE_DRIVE_SRC_FILES EXCLUDE REGEX "yolov8_detector\\.cpp$")
target_sources(ssne_face_drive_demo PRIVATE ${FACE_DRIVE_SRC_FILES} ${RPLIDAR_SDK_SRC})
target_link_libraries(ssne_face_drive_demo ${COMMON_LIBS})
install(TARGETS ssne_face_drive_demo DESTINATION bin)
