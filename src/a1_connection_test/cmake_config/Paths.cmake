
# ===========


set(SDK_ROOT "$ENV{BASE_DIR}/$ENV{EXPORT_LIB_M1_SDK_ROOT_PATH}" CACHE PATH "SDK root directory")

# 第三方库路径
set(THIRD_PARTY_DIR "${SDK_ROOT}/third_party" CACHE PATH "Third-party libraries directory")

set(M1_SDK_INC_DIR "${SDK_ROOT}/include" CACHE PATH "M1-SDK include directory")

set(M1_SDK_LIB_DIR "${SDK_ROOT}/lib" CACHE PATH "M1-SDK libraries directory")

# 自定义库搜索路径
set(CUSTOM_LIB_DIRS 
    "${SDK_ROOT}/lib"
    "${THIRD_PARTY_DIR}/lib"
    CACHE STRING "Custom library search paths"
)

# 自定义头文件搜索路径
set(CUSTOM_INCLUDE_DIRS 
    "${SDK_ROOT}/include"
    "${SDK_ROOT}/include/smartsoc"
    "${THIRD_PARTY_DIR}/include"
    CACHE STRING "Custom include search paths"
)


set(M1_GPIO_LIB        "${M1_SDK_LIB_DIR}/libgpio.so"       CACHE STRING INTERNAL)
set(M1_UART_LIB        "${M1_SDK_LIB_DIR}/libuart.so"       CACHE STRING INTERNAL)
