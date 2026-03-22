cmake_minimum_required(VERSION 3.0.0)

project(ssne_ai_demo)

# 引入路径配置文件
include(cmake_config/Paths.cmake)

set(RPLIDAR_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rplidar_sdk/sdk)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
# 核心：禁用ABI变更提示
add_compile_options(-Wno-psabi)

# 添加头文件目录
include_directories(${M1_SDK_INC_DIR})
include_directories(${M1_SDK_INC_DIR}/smartsoc)
include_directories(${RPLIDAR_SDK_DIR}/include)
include_directories(${RPLIDAR_SDK_DIR}/src)

# 定义调用目录来源
set(SRC_DIR "src")
set(TARGET $ENV{BASE_DIR}/target)
include_directories(${CMAKE_SOURCE_DIR}/include)
message([<mode>] "message to display" ${TARGET})

# RPLidar SDK 源文件 (共享)
file(GLOB RPLIDAR_SDK_SRC
    "${RPLIDAR_SDK_DIR}/src/arch/linux/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/dataunpacker/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/dataunpacker/unpacker/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/hal/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/*.cpp"
)

# 共用链接库
set(COMMON_LIBS
    ${M1_SSNE_LIB}
    ${M1_CMABUFFER_LIB}
    ${M1_OSD_LIB}
    ${M1_SSZLOG_LIB}
    ${M1_ZLOG_LIB}
    ${M1_EMB_LIB}
    pthread
    dl
)

# ==================== Target 1: 人脸检测 demo (原有) ====================
add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/demo_face.cpp)
file(GLOB FACE_SRC_FILES "${CMAKE_SOURCE_DIR}/${SRC_DIR}/*.cpp")
# 排除 vision_app / osd_visualizer / debug_data_interface 避免重复符号
list(FILTER FACE_SRC_FILES EXCLUDE REGEX "vision_app\\.cpp$")
list(FILTER FACE_SRC_FILES EXCLUDE REGEX "osd_visualizer\\.cpp$")
list(FILTER FACE_SRC_FILES EXCLUDE REGEX "debug_data_interface\\.cpp$")
target_sources(ssne_ai_demo PRIVATE ${FACE_SRC_FILES} ${RPLIDAR_SDK_SRC})
target_link_libraries(ssne_ai_demo ${COMMON_LIBS})
install(TARGETS ssne_ai_demo DESTINATION bin)

# ==================== Target 2: 视觉综合 demo (YOLOv8+人脸+雷达+OSD) ====================
add_executable(ssne_vision_demo ${PROJECT_SOURCE_DIR}/demo_vision.cpp)
file(GLOB VISION_SRC_FILES "${CMAKE_SOURCE_DIR}/${SRC_DIR}/*.cpp")
# 排除旧 project_flow 避免符号冲突
list(FILTER VISION_SRC_FILES EXCLUDE REGEX "project_flow\\.cpp$")
target_sources(ssne_vision_demo PRIVATE ${VISION_SRC_FILES} ${RPLIDAR_SDK_SRC})
target_link_libraries(ssne_vision_demo ${COMMON_LIBS})
install(TARGETS ssne_vision_demo DESTINATION bin)
