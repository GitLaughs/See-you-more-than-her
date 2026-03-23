#ifndef __CAN_H
#define __CAN_H	     
#include "sys.h"

//CAN1 receives RX0 interrupt enablement
//CAN1接收中断使能位
#define CAN1_RX0_INT_ENABLE	1	

//CAN2接收中断使能位
#define CAN2_RX1_INT_ENABLE	0	

/*--------CAN1 config--------*/
#define ENABLE_CAN1_CLOCK  RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1,ENABLE)
#if 0 //PA11、PA12
	#define ENABLE_CAN1_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
	#define ENABLE_CAN1_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
	#define CAN1_TX_PORT       GPIOA
	#define CAN1_RX_PORT       GPIOA
	#define CAN1_TX_PIN        GPIO_Pin_12
	#define CAN1_RX_PIN        GPIO_Pin_11
	#define CAN1_TX_Soure      GPIO_PinSource12
	#define CAN1_RX_Soure      GPIO_PinSource11
#elif 0 //PB8、PB9
	#define ENABLE_CAN1_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
	#define ENABLE_CAN1_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
	#define CAN1_TX_PORT       GPIOB
	#define CAN1_RX_PORT       GPIOB
	#define CAN1_TX_PIN        GPIO_Pin_9
	#define CAN1_RX_PIN        GPIO_Pin_8
	#define CAN1_TX_Soure      GPIO_PinSource9
	#define CAN1_RX_Soure      GPIO_PinSource8
#else  //PD0、PD1
	#define ENABLE_CAN1_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE)
	#define ENABLE_CAN1_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE)
	#define CAN1_TX_PORT       GPIOD
	#define CAN1_RX_PORT       GPIOD
	#define CAN1_TX_PIN        GPIO_Pin_1
	#define CAN1_RX_PIN        GPIO_Pin_0
	#define CAN1_TX_Soure      GPIO_PinSource1
	#define CAN1_RX_Soure      GPIO_PinSource0
#endif
/*----------------------------------*/

/*------- V1.00 CAN1 config--------*/
#define V1_0_ENABLE_CAN1_TX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define V1_0_ENABLE_CAN1_RX_PIN_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define V1_0_CAN1_TX_PORT       GPIOA
#define V1_0_CAN1_RX_PORT       GPIOA
#define V1_0_CAN1_TX_PIN        GPIO_Pin_12
#define V1_0_CAN1_RX_PIN        GPIO_Pin_11
#define V1_0_CAN1_TX_Soure      GPIO_PinSource12
#define V1_0_CAN1_RX_Soure      GPIO_PinSource11
/*----------------------------------*/

/*--------CAN1 Interface Fun --------*/
u8 CAN1_Mode_Init(u8 tsjw,u8 tbs2,u8 tbs1,u16 brp,u8 mode);
u8 V1_0_CAN1_Mode_Init(u8 tsjw,u8 tbs2,u8 tbs1,u16 brp,u8 mode);

u8 CAN1_Send_Num(u32 id,u8 *data);
u8 CAN1_Send_EXTid_Num(u32 id,u8 *data);
/*----------------------------------*/

							    
#endif

















