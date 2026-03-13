/**
 * @file uart_api.h
 * @brief UART API头文件
 * @version 1.0
 * @date 2026
 * 
 * 本文件提供UART的基础操作API
 * 
 * 主要特性：
 * - 支持UART TX0发送
 * - 支持UART RX0接收
 * - 支持波特率配置
 * - 支持奇偶校验配置
 * 
 */

#ifndef UART_API_H
#define UART_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 返回值定义
 * ============================================================================
 */
#define UART_SUCCESS     0
#define UART_ERROR      -1

/* ============================================================================
 * UART实例定义
 * ============================================================================
 */
typedef enum {
    UART_TX0 = 0,  /* UART TX0 */
    UART_RX0 = 1   /* UART RX0 */
} uart_id_t;

/* ============================================================================
 * 波特率定义
 * ============================================================================
 * @note 波特率支持任意数值，由用户自定义
 * @note 常用波特率参考值：
 *       - 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
 * @note 波特率范围建议：最小约100，最大约3000000（受硬件限制）
 */

/* ============================================================================
 * 奇偶校验定义
 * ============================================================================
 */
typedef enum {
    UART_PARITY_NONE = 0,  /* 无奇偶校验（推荐） */
    UART_PARITY_EVEN = 1,  /* 偶校验 */
    UART_PARITY_ODD  = 2   /* 奇校验 */
} uart_parity_t;

/* ============================================================================
 * UART句柄类型
 * ============================================================================
 */
typedef struct uart_handle_s *uart_handle_t;

/* ============================================================================
 * UART设备管理API
 * ============================================================================
 */

/**
 * @brief 初始化UART设备并获取句柄
 * @return 成功返回UART句柄，失败返回NULL
 * @note 使用完毕，程序末尾调用uart_close()释放句柄
 * @note 内核模块必须已加载：如果内核模块未加载或设备文件不存在，会返回NULL
 * @note 默认PIN0和PIN2复用为UART_TX0和UART_RX0
 */
uart_handle_t uart_init(void);

/**
 * @brief 关闭UART设备并释放句柄
 * @param handle UART句柄（由uart_init()返回）
 * @return 成功返回UART_SUCCESS，失败返回UART_ERROR
 */
int uart_close(uart_handle_t handle);

/* ============================================================================
 * UART配置API
 * ============================================================================
 */

/**
 * @brief 配置UART波特率
 * @param handle UART句柄
 * @param id UART实例ID（UART_TX0或UART_RX0）
 * @param baudrate 波特率值（支持任意数值，单位：bps）
 * @return 成功返回UART_SUCCESS，失败返回UART_ERROR
 * @note 使用前需要：1.配置GPIO复用为UART_TX0/UART_RX0 2.初始化UART
 * @note 波特率支持任意数值：用户可以根据需要设置任意波特率值
 * @note 系统时钟频率：300MHz，波特率计算公式：baud_cnt = (300000000 / baudrate) - 1
 * @note 波特率范围建议：最小约100 bps，最大约3000000 bps（受硬件限制）
 * @note 常用波特率：9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
 * @note TX0和RX0波特率必须一致：发送端和接收端的波特率必须相同，否则无法正常通信
 * @note 外设波特率需一致：与外部设备通信时，外部设备的波特率也必须与配置的波特率一致
 * @note 需要在发送/接收数据前配置：建议在uart_init()后立即配置波特率
 * @note 示例：uart_set_baudrate(handle, UART_TX0, 115200);  // 设置115200波特率
 * @note 示例：uart_set_baudrate(handle, UART_TX0, 125000);  // 设置自定义波特率125000
 */
int uart_set_baudrate(uart_handle_t handle, uart_id_t id, uint32_t baudrate);

/**
 * @brief 配置UART奇偶校验
 * @param handle UART句柄
 * @param id UART实例ID
 * @param parity 奇偶校验类型（UART_PARITY_NONE/UART_PARITY_EVEN/UART_PARITY_ODD）
 * @return 成功返回UART_SUCCESS，失败返回UART_ERROR
 * @note TX0和RX0校验必须一致：发送端和接收端的校验类型必须相同
 * @note 外设校验需一致：与外部设备通信时，外部设备的校验类型也必须与配置的一致
 * @note 推荐使用无校验：大多数应用场景使用无校验即可，简单可靠
 * @note 不调用默认无校验：如果不调用此函数，默认使用无校验
 */
int uart_set_parity(uart_handle_t handle, uart_id_t id, uart_parity_t parity);

/* ============================================================================
 * UART发送API
 * ============================================================================
 */

/**
 * @brief 发送多个字节数据
 * @param handle UART句柄
 * @param id UART实例ID（UART_TX0）
 * @param data 要发送的数据缓冲区
 * @param len 数据长度（建议不超过32字节，受FIFO大小限制）
 * @return 成功返回UART_SUCCESS，失败返回UART_ERROR
 * @note 使用前需要：1.配置GPIO复用为UART_TX0 2.初始化UART 3.配置波特率
 * @note FIFO限制说明：
 *       - UART TX FIFO大小为32字节（0x20）
 *       - 如果数据长度超过32字节，建议分多次调用本函数发送，需要注意时序问题
 *       - 例如：发送64字节数据时，先发送前32字节，等待发送完成后，再发送后32字节
 * @note 只支持UART_TX0：id参数必须为UART_TX0
 * @note 受硬件FIFO限制，建议单次不超过32字节
 */
int uart_send_data(uart_handle_t handle, uart_id_t id, const uint8_t *data, uint32_t len);


/* ============================================================================
 * UART接收API
 * ============================================================================
 */

/**
 * @brief 接收多个字节数据
 * @param handle UART句柄
 * @param id UART实例ID（UART_RX0）
 * @param data 输出缓冲区
 * @param len 要接收的数据长度（建议不超过32字节，受FIFO大小限制）
 * @param received 输出参数，实际接收到的数据长度
 * @return 成功返回UART_SUCCESS，失败返回UART_ERROR
 * @note 使用前需要：1.配置GPIO复用为UART_RX0 2.初始化UART 3.配置波特率
 * @note 接收数据存储位置：接收到的数据存储在传入的data缓冲区中，从data[0]开始存储，实际接收到的字节数通过received参数返回
 * @note FIFO限制说明：
 *       - UART RX FIFO大小为32字节（0x20）
 *       - 单次读取最多只能读取FIFO中剩余的数据量（最多32字节）
 *       - 如果数据超过32字节，需要分多次调用本函数读取，需要考虑时序问题，尽量32字节以内完成读取
 *       - 例如：接收64字节数据时，先读取前32字节，再读取后32字节
 * @note 实际接收数量：received参数返回实际接收到的字节数，可能小于请求的长度（如果FIFO中的数据少于请求的长度）
 * @note 只支持UART_RX0：id参数必须为UART_RX0
 * @note 一般使用场景：通常使用 `if (received > 0)` 判断是否接收到数据，然后检查data缓冲区中的特定指令内容。例如：
 *       if (received == 4 && data[0] == 0xAA && data[1] == 0x55) {
 *           // 匹配到特定指令，执行相应操作
 *       }
 */
int uart_receive_data(uart_handle_t handle, uart_id_t id, uint8_t *data, uint32_t len, uint32_t *received);

#ifdef __cplusplus
}
#endif

#endif /* UART_API_H */
