cmake_minimum_required(VERSION 3.0.0)

project(ssne_ai_demo)

# 引入路径配置文件
include(cmake_config/Paths.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
# 核心：禁用ABI变更提示
add_compile_options(-Wno-psabi)

add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/demo_rps_game.cpp)

# 添加头文件目录
include_directories(${M1_SDK_INC_DIR})
include_directories(${M1_SDK_INC_DIR}/smartsoc)
include_directories(${CMAKE_SOURCE_DIR}/include)

# 明确列出需要编进来的源文件，避免把旧检测源一起打进目标
set(SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/chassis_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/osd-device.cpp
    ${CMAKE_SOURCE_DIR}/src/pipeline_image.cpp
    ${CMAKE_SOURCE_DIR}/src/rps_classifier.cpp
    ${CMAKE_SOURCE_DIR}/src/utils.cpp
)

target_sources(ssne_ai_demo PRIVATE ${SRC_FILES})

target_link_libraries(${PROJECT_NAME}
                        ${M1_SSNE_LIB}
                        ${M1_CMABUFFER_LIB}
                        ${M1_OSD_LIB}
                        ${M1_GPIO_LIB}
                        ${M1_UART_LIB}
                        ${M1_SSZLOG_LIB}
                        ${M1_ZLOG_LIB}
                        ${M1_EMB_LIB}
)

install(TARGETS ssne_ai_demo DESTINATION bin)
