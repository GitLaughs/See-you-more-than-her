cmake_minimum_required(VERSION 3.0.0)

project(ssne_ai_demo)

# 引入路径配置文件
include(cmake_config/Paths.cmake)

set(RPLIDAR_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rplidar_sdk/sdk)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
# 核心：禁用ABI变更提示
add_compile_options(-Wno-psabi)

# 中文注释：已转写
# 中文注释：已转写
add_executable(ssne_ai_demo ${PROJECT_SOURCE_DIR}/demo_face.cpp)
# 中文注释：已转写

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


# 添加source文件夹下的所有cpp文件
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/${SRC_DIR}/*.cpp")
target_sources(ssne_ai_demo PRIVATE ${SRC_FILES})

file(GLOB RPLIDAR_SDK_SRC
    "${RPLIDAR_SDK_DIR}/src/arch/linux/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/dataunpacker/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/dataunpacker/unpacker/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/hal/*.cpp"
    "${RPLIDAR_SDK_DIR}/src/*.cpp"
)
target_sources(ssne_ai_demo PRIVATE ${RPLIDAR_SDK_SRC})

target_link_libraries(${PROJECT_NAME} 
                        ${M1_SSNE_LIB}
                        ${M1_CMABUFFER_LIB}
                        ${M1_OSD_LIB}
                        ${M1_SSZLOG_LIB}
                        ${M1_ZLOG_LIB}
                                                ${M1_EMB_LIB}
                                                pthread
                                                dl
)

install(TARGETS ssne_ai_demo DESTINATION bin)
