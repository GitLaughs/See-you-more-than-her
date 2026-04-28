UART驱动说明
UART（Universal Asynchronous Receiver/Transmitter，通用异步收发传输器）是一种串行通信接口，用于在设备之间进行异步串行数据传输。它不需要时钟信号，而是通过起始位、数据位、停止位来同步数据。

注意点：

● A1平台支持 UART TX0（发送实例）和 UART RX0（接收实例）。

● UART 通信必须与 GPIO 复用功能配合使用：TX0 默认配置 GPIO 引脚为 UART_TX0 输出复用，RX0 默认配置 GPIO 引脚为 UART_RX0 输入复用。

● （其中，如引脚图所示，GPIO_PIN_0默认状态是UART TX0，GPIO_PIN_2默认状态是UART RX0）

硬件连接说明
UART 与外部设备连接时需注意以下事项：

1. 波特率配置

● A1 平台的 UART 波特率必须与外设设备的波特率完全一致，否则无法正常通信。

● 常用的波特率：9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 或者自定义波特率均可支持（推荐低于2000000）

uart使用需要注意时序，调试推荐使用示波器和串口调试工具

以921600波特率为例子，TX连续发送0x55（01010101），波形如下图所示

image
image
802×505 100 KB
2. TX 和 RX 交叉连接

● 即 A1 平台的 TX0 连接到外设的 RX，A1 平台的 RX0 连接到外设的 TX。

A1平台                外设设备
TX0         ────────>  RX
RX0         <────────  TX
GND         <───────>  GND
3. 电源连接

● GND（地线）：确保共地，这是 UART 通信的必要条件。

● 1.8V 电源：A1 平台的 UART 信号电平为 1.8V。一般情况下不需要单独连接 1.8V 电源线，除非需要为外设提供电源。

引用头文件（UART）
开发主要用到的头文件是 uart_api.h。

文件位置：

smartsens_sdk/output/opt/m1_sdk/usr/include/uart_api.h

（该文件包含 UART 相关 API 的详细注释）需要调用头文件：

#include "uart_api.h"         //必须引用，包含所有公共API接口的函数声明
UART功能依赖库列表
库文件路径链接方式的 cmake 等写法可以参考人脸识别 ssne_ai_demo 中的 Paths.cmake 和 CMakeLists.txt。

依赖库列表：libuart.so

1. Paths.cmake 中：

set(M1_UART_LIB        "${M1_SDK_LIB_DIR}/libuart.so"      CACHE STRING INTERNAL)
2. CMakeLists.txt 中的 target_link_libraries 函数增加 ${M1_UART_LIB}：

target_link_libraries(${PROJECT_NAME} 
                        ${M1_SSNE_LIB}
                        ${M1_CMABUFFER_LIB}
                        ${M1_OSD_LIB}
                        ${M1_GPIO_LIB}
                        ${M1_UART_LIB}  // 添加这一行
                        ${M1_SSZLOG_LIB}
                        ${M1_ZLOG_LIB}
                        ${M1_EMB_LIB}
)
3. 运行脚本 (run.sh)：运行程序前增加载 uart 驱动模块

insmod /lib/modules/$(uname -r)/extra/uart_kmod.ko
API使用示例和注意点
1. uart_init() - 初始化UART设备

示例：

uart_handle_t uart_handle;
uart_handle = uart_init();
if (uart_handle == NULL) {
    printf("UART初始化失败\n");
    return -1;
}
注意点：

● 必须在使用前调用：所有 UART 操作都需要先调用此函数获取句柄。

● 内核模块必须已加载：如果内核模块未加载或设备文件不存在，会返回 NULL。

● GPIO复用前置：使用 UART 前需要先通过 GPIO API 配置相应引脚的复用功能（A1已默认配置）

2. uart_close() - 关闭UART设备

示例：

uart_close(uart_handle);
注意点：

● 使用完 UART 后调用此函数释放资源，避免资源泄漏。

3. uart_set_baudrate() - 配置UART波特率

示例：

// 配置UART TX0波特率为115200
uart_set_baudrate(uart_handle, UART_TX0, 115200);
 
// 配置UART RX0波特率为115200
uart_set_baudrate(uart_handle, UART_RX0, 115200);
注意点：

● 必须一致：TX0 和 RX0 的波特率必须相同；与外部设备通信时，外设的波特率也必须与配置的一致。

● 配置时机：建议在 uart_init() 后立即配置。

● 默认值：如果不配置，默认波特率为 115200。

4. uart_set_parity() - 配置UART奇偶校验

示例：

// 配置为无校验（推荐）
uart_set_parity(uart_handle, UART_TX0, UART_PARITY_NONE);
uart_set_parity(uart_handle, UART_RX0, UART_PARITY_NONE);
注意点：

● 类型：支持 UART_PARITY_NONE（无校验，推荐）、UART_PARITY_EVEN（偶校验）、UART_PARITY_ODD（奇校验）。

● 一致性：通信双方的校验类型必须相同。

● 默认值：如果不调用此函数，默认使用无校验。

5. uart_send_data() - 发送多个字节数据

示例：

// 发送32字节数据（FIFO满载）
uint8_t data[32] = {0};
uart_send_data(uart_handle, UART_TX0, data, 32);
 
// 发送64字节数据（分包发送示例）
uint8_t big_data[64];
// 发送前32字节
uart_send_data(uart_handle, UART_TX0, &big_data[0], 32);
// 发送后32字节
uart_send_data(uart_handle, UART_TX0, &big_data[0], 32);
注意点：

● FIFO限制：单次发送建议不超过 32 字节。

● 大数据处理：超过 32 字节需分多次调用。

● 无需手动延时：在两次发送调用之间，无需额外延时等待。

6. uart_receive_data() - 接收多个字节数据

示例：

uint8_t buffer[32];
uint32_t actual_len = 0;
 
// 尝试接收数据（最多32字节）
uart_receive_data(uart_handle, UART_RX0, buffer, sizeof(buffer), &actual_len);
 
if (actual_len > 0) {
    printf("成功接收 %u 字节数据\n", actual_len);
    // 处理数据...
}
注意点：

● 实际接收量：actual_len 返回实际接收到的字节数，可能小于请求的长度（取决于 FIFO 中的数据量）。

● FIFO限制：单次最多读取 32 字节。

● 大数据处理：如果数据超过 32 字节，需要循环或多次调用读取。

● 数据存储：接收到的数据从 data[0] 开始存储。

完整使用示例
以下是一个完整的 UART 初始化、发送与接收流程示例（仅供参考）：

#include "gpio_api.h"
#include "uart_api.h"
#include <stdio.h>
 
int main(void)
{
    // 1. GPIO初始化
    gpio_handle_t gpio = gpio_init();
    if (gpio == NULL) return -1;
 
    // 2. 配置GPIO复用（以PIN0为TX, PIN2为RX默认状态开启，可以不设置）
    // 使能GPIO功能，准备复用（默认enable，实际可以不调用）
    //gpio_set_enable(gpio, GPIO_PIN_0 | GPIO_PIN_2, true);
    // 设置方向
    //gpio_set_mode(gpio, GPIO_PIN_0, GPIO_MODE_OUTPUT); // TX为输出
    //gpio_set_mode(gpio, GPIO_PIN_2, GPIO_MODE_INPUT);  // RX为输入
    // 配置复用功能
    //gpio_set_alternate(gpio, GPIO_PIN_0, GPIO_AF_INPUT_NONE, GPIO_AF_OUTPUT_UART_TX0);
    //gpio_set_alternate(gpio, GPIO_PIN_2, GPIO_AF_INPUT_UART_RX0, GPIO_AF_OUTPUT_NONE);
 
    // 3. UART初始化
    uart_handle_t uart = uart_init();
    if (uart == NULL) {
        gpio_close(gpio);
        return -1;
    }
 
    // 4. 配置通信参数
    uart_set_baudrate(uart, UART_TX0, 115200);
    uart_set_baudrate(uart, UART_RX0, 115200);
    uart_set_parity(uart, UART_TX0, UART_PARITY_NONE);
    uart_set_parity(uart, UART_RX0, UART_PARITY_NONE);
 
    // 5. 发送数据
    uint8_t tx_data[10] = "Hello";
    uart_send_data(uart, UART_TX0, tx_data, 10);
 
    // 6. 接收数据
    uint8_t rx_buffer[32];
    uint32_t rx_len = 0;
    uart_receive_data(uart, UART_RX0, rx_buffer, 32, &rx_len);
    if (rx_len > 0) {
        printf("Received: %.*s\n", rx_len, rx_buffer);
    }
 
    // 7. 释放资源
    uart_close(uart);
    gpio_close(gpio);
 
    return 0;
}