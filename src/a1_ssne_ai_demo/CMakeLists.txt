cmake_minimum_required(VERSION 3.0.0)

project(ssne_face_drive_demo)

include(cmake_config/Paths.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
add_compile_options(-Wno-psabi)

include_directories(${M1_SDK_INC_DIR})
include_directories(${M1_SDK_INC_DIR}/smartsoc)
include_directories(${CMAKE_SOURCE_DIR}/include)

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

# 仅编译 人脸检测 + 底盘控制 所需的源文件
set(FACE_DRIVE_SOURCES
    src/scrfd_gray.cpp
    src/pipeline_image.cpp
    src/osd-device.cpp
    src/utils.cpp
    src/chassis_controller.cpp
    src/face_drive_app.cpp
)

add_executable(ssne_face_drive_demo
    ${PROJECT_SOURCE_DIR}/demo_face_drive.cpp
    ${FACE_DRIVE_SOURCES}
)
target_link_libraries(ssne_face_drive_demo ${COMMON_LIBS})
install(TARGETS ssne_face_drive_demo DESTINATION bin)
