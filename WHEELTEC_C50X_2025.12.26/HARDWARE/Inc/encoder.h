#ifndef __ENCODER_H
#define __ENCODER_H 
#include "sys.h"

// No larger than 65535, because the timer of STM32F103 is 16 bit
//不可大于65535，因为STM32F103的定时器是16位的
#define ENCODER_TIM_PERIOD (u16)(65535)   

//编码器枚举
typedef enum{
	Encoder_A,
	Encoder_B,
	Encoder_C,
	Encoder_D
}ENCODER_t;

/*--------ENCODER Interface Fun --------*/
short Read_Encoder(ENCODER_t e);
/*----------------------------------*/


/*--------ENCODER_A config--------*/
#define ENABLE_ENCODER_A_TIM_CLOCK    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE)
#define ENBALE_ENCODER_A1_PORT_CLOCK  RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define ENBALE_ENCODER_A2_PORT_CLOCK  RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)

#define ENCODER_A_TIM     TIM2             //编码器A接口所使用的定时器

#define ENCODER_A1_PORT   GPIOA            //编码器A接口1号端口
#define ENCODER_A1_PIN    GPIO_Pin_15      //编码器A接口1号引脚

#define ENCODER_A2_PORT   GPIOB            //编码器A接口2号端口
#define ENCODER_A2_PIN    GPIO_Pin_3       //编码器A接口2号引脚
/*----------------------------------*/

/*--------ENCODER_A Interface Fun --------*/
void EncoderA_Init(void);
/*----------------------------------*/


/*--------ENCODER_B config--------*/
#define ENABLE_ENCODER_B_TIM_CLOCK    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE)
#define ENBALE_ENCODER_B1_PORT_CLOCK  RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define ENBALE_ENCODER_B2_PORT_CLOCK  RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)

#define ENCODER_B_TIM     TIM3             //编码器B接口所使用的定时器

#define ENCODER_B1_PORT   GPIOA            //编码器B接口1号端口
#define ENCODER_B1_PIN    GPIO_Pin_6       //编码器B接口1号引脚

#define ENCODER_B2_PORT   GPIOA            //编码器B接口2号端口
#define ENCODER_B2_PIN    GPIO_Pin_7       //编码器B接口2号引脚
/*----------------------------------*/

/*--------ENCODER_B Interface Fun --------*/
void EncoderB_Init(void);
/*----------------------------------*/

/*--------ENCODER_C config--------*/
#define ENABLE_ENCODER_C_TIM_CLOCK    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4,ENABLE)
#define ENBALE_ENCODER_C1_PORT_CLOCK  RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
#define ENBALE_ENCODER_C2_PORT_CLOCK  RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)

#define ENCODER_C_TIM     TIM4             //编码器C接口所使用的定时器

#define ENCODER_C1_PORT   GPIOB            //编码器C接口1号端口
#define ENCODER_C1_PIN    GPIO_Pin_6       //编码器C接口1号引脚

#define ENCODER_C2_PORT   GPIOB            //编码器C接口2号端口
#define ENCODER_C2_PIN    GPIO_Pin_7       //编码器C接口2号引脚
/*----------------------------------*/

/*--------ENCODER_C Interface Fun --------*/
void EncoderC_Init(void);
/*----------------------------------*/

/*--------ENCODER_D config--------*/
#define ENABLE_ENCODER_D_TIM_CLOCK    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5,ENABLE)
#define ENBALE_ENCODER_D1_PORT_CLOCK  RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)
#define ENBALE_ENCODER_D2_PORT_CLOCK  RCC_APB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)

#define ENCODER_D_TIM     TIM5             //编码器D接口所使用的定时器

#define ENCODER_D1_PORT   GPIOA            //编码器D接口1号端口
#define ENCODER_D1_PIN    GPIO_Pin_0       //编码器D接口1号引脚

#define ENCODER_D2_PORT   GPIOA            //编码器D接口2号端口
#define ENCODER_D2_PIN    GPIO_Pin_1       //编码器D接口2号引脚
/*----------------------------------*/

/*--------ENCODER_D Interface Fun --------*/
void EncoderD_Init(void);
/*----------------------------------*/


#endif
