cmake_minimum_required(VERSION 3.0.0)

project(ssne_ai_demo)

# 引入路径配置文件
include(cmake_config/Paths.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
# 核心：禁用ABI变更提示
add_compile_options(-Wno-psabi)

# add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/online_ped_osd_thread.cpp)
# add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/online_ped_osd.cpp)
add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/demo_face.cpp)
# add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/test_isp_debug.cpp)

# 添加头文件目录
include_directories(${M1_SDK_INC_DIR})
include_directories(${M1_SDK_INC_DIR}/smartsoc)

# 定义调用目录来源
set(SRC_DIR "src")
set(TARGET $ENV{BASE_DIR}/target)
include_directories(${CMAKE_SOURCE_DIR}/include)
message([<mode>] "message to display" ${TARGET})


# 添加source文件夹下的所有cpp文件
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/${SRC_DIR}/*.cpp")
target_sources(ssne_ai_demo PRIVATE ${SRC_FILES})

target_link_libraries(${PROJECT_NAME} 
                        ${M1_SSNE_LIB}
                        ${M1_CMABUFFER_LIB}
                        ${M1_OSD_LIB}
                        ${M1_SSZLOG_LIB}
                        ${M1_ZLOG_LIB}
                        ${M1_EMB_LIB}
)

install(TARGETS ssne_ai_demo DESTINATION bin)
