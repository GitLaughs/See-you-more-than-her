#ifndef __USRATX_H
#define __USRATX_H 
#include "sys.h"

//APP控制相关变量结构体
typedef struct{
	u8 TurnPage;      //APP进入转向页面标志位
	u8 DirectionFlag; //APP方向标志
	u8 ParamSaveFlag; //APP保存参数标志位
	u8 ParamSendflag; //APP发送数据标志位
	u8 TurnFlag;      //APP转向标志
}APP_CONTROL_t;


extern APP_CONTROL_t appkey;
void APPKey_Param_Init(APP_CONTROL_t *p);

/*--------UART1 config--------*/
#define ENABLE_UART1_CLOCK  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE)
#if 1 //PA9、PA10
#define ENABLE_UART1_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define ENABLE_UART1_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define UART1_TX_PORT       GPIOA
#define UART1_RX_PORT       GPIOA
#define UART1_TX_PIN        GPIO_Pin_9
#define UART1_RX_PIN        GPIO_Pin_10
#define UART1_TX_Soure      GPIO_PinSource9
#define UART1_RX_Soure      GPIO_PinSource10
#else //PB6、PB7
#define ENABLE_UART1_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
#define ENABLE_UART1_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
#define UART1_TX_PORT       GPIOB
#define UART1_RX_PORT       GPIOB
#define UART1_PIN_TX        GPIO_Pin_6
#define UART1_PIN_RX        GPIO_Pin_7
#define UART1_TX_Soure      GPIO_PinSource6
#define UART1_RX_Soure      GPIO_PinSource7
#endif
/*----------------------------------*/

/*--------UART1 Interface Fun --------*/
void UART1_Init(u32 bound);
void uart1_send(u8 data);
/*----------------------------------*/

/*--------UART3 config--------*/
#define ENABLE_UART3_CLOCK  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE)
#if 1 //PB10、PB11
#define ENABLE_UART3_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
#define ENABLE_UART3_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
#define UART3_TX_PORT       GPIOB
#define UART3_RX_PORT       GPIOB
#define UART3_TX_PIN        GPIO_Pin_10
#define UART3_RX_PIN        GPIO_Pin_11
#define UART3_TX_Soure      GPIO_PinSource10
#define UART3_RX_Soure      GPIO_PinSource11
#else //PD8、PD9
#define ENABLE_UART3_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE)
#define ENABLE_UART3_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE)
#define UART3_TX_PORT       GPIOD
#define UART3_RX_PORT       GPIOD
#define UART3_PIN_TX        GPIO_Pin_8
#define UART3_PIN_RX        GPIO_Pin_9
#define UART3_TX_Soure      GPIO_PinSource8
#define UART3_RX_Soure      GPIO_PinSource9
#endif
/*----------------------------------*/

/*--------UART3 Interface Fun --------*/
void UART3_Init(u32 bound);
void uart3_send(u8 data);
/*----------------------------------*/

/*--------UART4 config--------*/
#define ENABLE_UART4_CLOCK  RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4,ENABLE)
#if 1 //PC10、PC11
#define ENABLE_UART4_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE)
#define ENABLE_UART4_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE)
#define UART4_TX_PORT       GPIOC
#define UART4_RX_PORT       GPIOC
#define UART4_TX_PIN        GPIO_Pin_10
#define UART4_RX_PIN        GPIO_Pin_11
#define UART4_TX_Soure      GPIO_PinSource10
#define UART4_RX_Soure      GPIO_PinSource11
#else //PA0、PA1
#define ENABLE_UART4_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define ENABLE_UART4_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define UART4_TX_PORT       GPIOA
#define UART4_RX_PORT       GPIOA
#define UART4_PIN_TX        GPIO_Pin_0
#define UART4_PIN_RX        GPIO_Pin_1
#define UART4_TX_Soure      GPIO_PinSource0
#define UART4_RX_Soure      GPIO_PinSource1
#endif
/*----------------------------------*/

/*--------UART4 Interface Fun --------*/
void UART4_Init(u32 bound);
void uart4_send(u8 data);
void BlueTooth_UsartConfig(void);
/*----------------------------------*/


#endif

