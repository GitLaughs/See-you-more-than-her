cmake_minimum_required(VERSION 3.0.0)

project(ssne_connection_test)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
add_compile_options(-Wno-psabi)

# SDK 路径 (由构建脚本通过 -D 传入)
set(M1_SDK_INC_DIR "" CACHE PATH "M1-SDK include directory")
set(M1_SDK_LIB_DIR "" CACHE PATH "M1-SDK libraries directory")

include_directories(${M1_SDK_INC_DIR})
include_directories(${M1_SDK_INC_DIR}/smartsoc)
include_directories(${CMAKE_SOURCE_DIR}/include)

# 仅链接 GPIO + UART 库（最小依赖）
set(M1_GPIO_LIB "${M1_SDK_LIB_DIR}/libgpio.so" CACHE STRING INTERNAL)
set(M1_UART_LIB "${M1_SDK_LIB_DIR}/libuart.so" CACHE STRING INTERNAL)

set(TEST_LIBS
    ${M1_GPIO_LIB}
    ${M1_UART_LIB}
    pthread
)

add_executable(ssne_connection_test
    connection_test.cpp
    src/chassis_controller.cpp
)
target_link_libraries(ssne_connection_test ${TEST_LIBS})
install(TARGETS ssne_connection_test DESTINATION bin)
