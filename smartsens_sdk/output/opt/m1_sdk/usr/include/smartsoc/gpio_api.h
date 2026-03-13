/**
 * @file gpio_api.h
 * @brief GPIO API头文件
 * @version 3.0
 * @date 2026
 * 
 * 本文件提供GPIO的基础操作API
 * 
 * 主要特性：
 * - GPIO引脚定义：GPIO_PIN_0 ~ GPIO_PIN_10
 * - 支持位掩码输入：可以使用 GPIO_PIN_0 | GPIO_PIN_1 同时操作多个引脚
 * - GPIO模式定义：GPIO_MODE_INPUT, GPIO_MODE_OUTPUT
 * - GPIO状态定义：GPIO_PIN_SET, GPIO_PIN_RESET
 * - GPIO复用功能定义：GPIO_AF_xxx
 * 
 */

#ifndef GPIO_API_H
#define GPIO_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 返回值定义
 * ============================================================================
 */
#define GPIO_SUCCESS     0
#define GPIO_ERROR      -1

/* ============================================================================
 * GPIO引脚定义（GPIO_PIN_0 ~ GPIO_PIN_10）
 *
 * 当前驱动只支持以下引脚：
 * - GPIO_PIN_0
 * - GPIO_PIN_2
 * - GPIO_PIN_8
 * - GPIO_PIN_9
 * - GPIO_PIN_10
 *
 * 其他引脚（1,3,4,5,6,7）在API中会被判定为非法输入并返回错误。
 * 这里保留宏定义仅用于文档和完整性说明，不建议在应用中使用被标记为不支持的引脚。
 * ============================================================================
 */
#define GPIO_PIN_0       ((uint16_t)0x0001)  /* GPIO引脚0 (1<<0) - 支持 */
/* #define GPIO_PIN_1    ((uint16_t)0x0002)     GPIO引脚1 (1<<1) - 不支持，仅保留定义注释 */
#define GPIO_PIN_2       ((uint16_t)0x0004)  /* GPIO引脚2 (1<<2) - 支持 */
/* #define GPIO_PIN_3    ((uint16_t)0x0008)     GPIO引脚3 (1<<3) - 不支持，仅保留定义注释 */
/* #define GPIO_PIN_4    ((uint16_t)0x0010)     GPIO引脚4 (1<<4) - 不支持，仅保留定义注释 */
/* #define GPIO_PIN_5    ((uint16_t)0x0020)     GPIO引脚5 (1<<5) - 不支持，仅保留定义注释 */
/* #define GPIO_PIN_6    ((uint16_t)0x0040)     GPIO引脚6 (1<<6) - 不支持，仅保留定义注释 */
/* #define GPIO_PIN_7    ((uint16_t)0x0080)     GPIO引脚7 (1<<7) - 不支持，仅保留定义注释 */
#define GPIO_PIN_8       ((uint16_t)0x0100)  /* GPIO引脚8 (1<<8) - 支持 */
#define GPIO_PIN_9       ((uint16_t)0x0200)  /* GPIO引脚9 (1<<9) - 支持 */
#define GPIO_PIN_10      ((uint16_t)0x0400)  /* GPIO引脚10 (1<<10) - 支持 */

/* ============================================================================
 * GPIO模式定义
 * ============================================================================
 */
typedef enum {
    GPIO_MODE_INPUT  = 0,  /* 输入模式 */
    GPIO_MODE_OUTPUT = 1   /* 输出模式 */
} gpio_mode_t;

/* ============================================================================
 * GPIO状态定义
 * ============================================================================
 */
typedef enum {
    GPIO_PIN_RESET = 0,  /* 低电平（复位状态） */
    GPIO_PIN_SET   = 1   /* 高电平（置位状态） */
} gpio_pin_state_t;

/* ============================================================================
 * GPIO复用功能定义
 * ============================================================================
 * 
 * @note 默认配置说明：
 *       - 调用 gpio_init() 之后，默认配置如下：
 *         * PIN0：配置为 UART TX0（输出复用功能）
 *         * PIN2：配置为 UART RX0（输入复用功能）
 *       - 如果要将PIN0或PIN2用作通用GPIO输入/输出，需要使用 gpio_set_alternate() API
 *         将复用功能设置为双NONE（input_sel=GPIO_AF_INPUT_NONE, output_sel=GPIO_AF_OUTPUT_NONE）
 *       - 示例：将PIN0恢复为通用GPIO
 *         gpio_set_alternate(gpio_handle, GPIO_PIN_0, GPIO_AF_INPUT_NONE, GPIO_AF_OUTPUT_NONE);
 */

/* 输入复用功能选择（UART_RX0） */
/* 注意：UART RX0输入复用功能硬件限制，支持PIN0和PIN2，推荐使用PIN2 */
#define GPIO_AF_INPUT_UART_RX0       0x6   /* uart rx0 */
#define GPIO_AF_INPUT_NONE            0xFF /* 保持原值（不配置） */

/* 输出复用功能选择（UART_TX0） */
/* 注意：UART TX0输出复用功能硬件限制，支持PIN0和PIN2，推荐使用PIN0 */
#define GPIO_AF_OUTPUT_UART_TX0      0x6   /* uart tx0 */
#define GPIO_AF_OUTPUT_NONE           0xFF /* 保持原值（不配置） */

/* ============================================================================
 * GPIO句柄类型
 * ============================================================================
 */
typedef struct gpio_handle_s *gpio_handle_t;

/* ============================================================================
 * GPIO设备管理API
 * ============================================================================
 */

/**
 * @brief 初始化GPIO设备并获取句柄
 * @return 成功返回GPIO句柄，失败返回NULL
 * @note 使用完毕后必须调用gpio_close()释放句柄
 */
gpio_handle_t gpio_init(void);

/**
 * @brief 关闭GPIO设备并释放句柄
 * @param handle GPIO句柄（由gpio_init()返回）
 * @return 成功返回GPIO_SUCCESS，失败返回GPIO_ERROR
 */
int gpio_close(gpio_handle_t handle);

/* ============================================================================
 * 基础GPIO操作API
 * ============================================================================
 */

/**
 * @brief 使能GPIO引脚（设置为GPIO模式）
 * @param handle GPIO句柄
 * @param pin GPIO引脚位掩码（如GPIO_PIN_5或GPIO_PIN_0|GPIO_PIN_1）
 * @param enable true=使能GPIO，false=禁用GPIO（默认状态所有GPIO都使能，该API非必要调用）
 * @return 成功返回GPIO_SUCCESS，失败返回GPIO_ERROR
 * @note 支持位掩码输入，可同时操作多个引脚（如GPIO_PIN_0|GPIO_PIN_1）
 */
int gpio_set_enable(gpio_handle_t handle, uint16_t pin, bool enable);

/**
 * @brief 设置GPIO引脚模式（输入/输出）
 * @param handle GPIO句柄
 * @param pin GPIO引脚位掩码（如GPIO_PIN_5或GPIO_PIN_0|GPIO_PIN_1）
 * @param mode GPIO模式（GPIO_MODE_INPUT或GPIO_MODE_OUTPUT）
 * @return 成功返回GPIO_SUCCESS，失败返回GPIO_ERROR
 * @note GPIO_MODE_OUTPUT=输出模式，GPIO_MODE_INPUT=输入模式
 * @note 支持位掩码输入，可同时操作多个引脚（如GPIO_PIN_0|GPIO_PIN_1）
 */
int gpio_set_mode(gpio_handle_t handle, uint16_t pin, gpio_mode_t mode);

/**
 * @brief 设置GPIO引脚状态（高电平/低电平）
 * @param handle GPIO句柄
 * @param pin GPIO引脚位掩码（如GPIO_PIN_5或GPIO_PIN_0|GPIO_PIN_1）
 * @param state GPIO状态（GPIO_PIN_SET或GPIO_PIN_RESET）
 * @return 成功返回GPIO_SUCCESS，失败返回GPIO_ERROR
 * @note 使用前需要先调用gpio_set_enable()和gpio_set_mode()配置GPIO
 * @note 支持位掩码输入，可同时操作多个引脚（如GPIO_PIN_0|GPIO_PIN_1）
 */
int gpio_write_pin(gpio_handle_t handle, uint16_t pin, gpio_pin_state_t state);

/**
 * @brief 读取GPIO引脚状态
 * @param handle GPIO句柄
 * @param pin GPIO引脚位掩码（如GPIO_PIN_5或GPIO_PIN_0|GPIO_PIN_1）
 * @param state_mask 输出参数，返回位掩码（对应位=1表示SET，对应位=0表示RESET）
 * @return 成功返回GPIO_SUCCESS，失败返回GPIO_ERROR
 * @note 使用前需要先调用gpio_set_enable()和gpio_set_mode()配置GPIO为输入模式
 * @note 支持位掩码输入，可同时读取多个引脚（如GPIO_PIN_0|GPIO_PIN_1）
 * @note 返回值是位掩码，例如：如果PIN0=SET, PIN1=RESET，返回0x0001
 *       使用方式：if (*state_mask & GPIO_PIN_0) { // PIN0是SET }
 */
int gpio_read_pin(gpio_handle_t handle, uint16_t pin, uint16_t *state_mask);

/**
 * @brief 翻转GPIO引脚电平
 * @param handle GPIO句柄
 * @param pin GPIO引脚位掩码（如GPIO_PIN_5或GPIO_PIN_0|GPIO_PIN_1）
 * @return 成功返回GPIO_SUCCESS，失败返回GPIO_ERROR
 * @note 使用前需要先调用gpio_set_enable()和gpio_set_mode()配置GPIO为输出模式
 * @note 支持位掩码输入，可同时翻转多个引脚（如GPIO_PIN_0|GPIO_PIN_1）
 */
int gpio_toggle_pin(gpio_handle_t handle, uint16_t pin);

/**
 * @brief 配置GPIO复用功能
 * @param handle GPIO句柄
 * @param pin GPIO引脚位掩码（如GPIO_PIN_5，注意：复用功能只支持单个引脚）
 * @param af_input 输入复用功能（GPIO_AF_INPUT_xxx或GPIO_AF_INPUT_NONE保持原值）
 * @param af_output 输出复用功能（GPIO_AF_OUTPUT_xxx或GPIO_AF_OUTPUT_NONE保持原值）
 * @return 成功返回GPIO_SUCCESS，失败返回GPIO_ERROR
 * @note 复用功能配置只支持单个引脚，不支持位掩码
 * @note 默认配置说明：
 *       - 调用 gpio_init() 之后，默认配置如下：
 *         * PIN0：配置为 UART TX0（输出复用功能）
 *         * PIN2：配置为 UART RX0（输入复用功能）
 *       - 如果要将PIN0或PIN2用作通用GPIO输入/输出，需要调用本函数
 *         将复用功能设置为双NONE（af_input=GPIO_AF_INPUT_NONE, af_output=GPIO_AF_OUTPUT_NONE）
 *       - 示例：将PIN0恢复为通用GPIO
 *         gpio_set_alternate(gpio_handle, GPIO_PIN_0, GPIO_AF_INPUT_NONE, GPIO_AF_OUTPUT_NONE);
 */
int gpio_set_alternate(gpio_handle_t handle, uint16_t pin, uint8_t af_input, uint8_t af_output);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_API_H */
