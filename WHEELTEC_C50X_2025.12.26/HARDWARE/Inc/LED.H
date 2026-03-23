#ifndef __LED_H
#define __LED_H
#include "sys.h"

//LED颜色定义
typedef enum{
	LED_Red     , 
	LED_Purple  ,
	LED_Blue    ,
	LED_Cyan    ,
	LED_Green   ,
	LED_Yellow  ,
	LED_White   ,
}LED_COLOR;

//LED开关状态定义
enum{
	LED_ON  = 0 ,
	LED_OFF = 1
};

//RGB灯带定义
enum{
	RGB_FULL_ON  = 255,
	RGB_FULL_OFF = 0
};

/*--------LED Interface Fun --------*/
void LED_Init(void);  
void V1_0_LED_Init(void);
void RGB_LightStrip_Init(void);

void LED_SetColor(LED_COLOR color);
void LED_SetState(uint8_t mode);
void LightStrip_SetColor( uint8_t r,uint8_t g,uint8_t b );
/*----------------------------------*/

/*--------LED config--------*/
//LED灯珠
#define ENABLE_LED_R_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE)
#define ENABLE_LED_G_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE)
#define ENABLE_LED_B_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE)

#define LED_R_PORT GPIOB
#define LED_G_PORT GPIOE
#define LED_B_PORT GPIOB

#define LED_R_PIN GPIO_Pin_15
#define LED_G_PIN GPIO_Pin_6
#define LED_B_PIN GPIO_Pin_14

#define LED_R PBout(15) 
#define LED_G PEout(6) 
#define LED_B PBout(14) 
/*----------------------------------*/


/*--------LED V1.0 config--------*/
//LED灯珠
#define V1_0_ENABLE_LED_R_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE)
#define V1_0_ENABLE_LED_G_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE)
#define V1_0_ENABLE_LED_B_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE)

#define V1_0_LED_R_PORT GPIOD
#define V1_0_LED_G_PORT GPIOD
#define V1_0_LED_B_PORT GPIOD

#define V1_0_LED_R_PIN GPIO_Pin_14
#define V1_0_LED_G_PIN GPIO_Pin_13
#define V1_0_LED_B_PIN GPIO_Pin_15

#define V1_0_LED_R PDout(14) 
#define V1_0_LED_G PDout(13) 
#define V1_0_LED_B PDout(15) 
/*----------------------------------*/

/*--------LightStrip config--------*/
/*-------- RGB灯带配置 --------*/
#define ENABLE_LightStrip_R_TIM_CLOCK   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM12,ENABLE) //定时器
#define ENABLE_LightStrip_G_TIM_CLOCK   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9,ENABLE) 
#define ENABLE_LightStrip_B_TIM_CLOCK   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM12,ENABLE) 

#define ENABLE_LightStrip_R_PORT_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE) //引脚端口
#define ENABLE_LightStrip_G_PORT_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE) 
#define ENABLE_LightStrip_B_PORT_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
	
#define LightStrip_R_PORT      GPIOB            //引脚端口
#define LightStrip_R_PIN       GPIO_Pin_15      //引脚
#define LightStrip_R_TIM       TIM12            //使用的定时器
#define LightStrip_R_Channel   2                //使用了定时器的哪一个通道CCRx(1~4)

#define LightStrip_G_PORT      GPIOE            //引脚端口 （此处注意,高配阿克曼舵机pwm信号也使用了TIM9定时器）
#define LightStrip_G_PIN       GPIO_Pin_6       //引脚
#define LightStrip_G_TIM       TIM9             //使用的定时器
#define LightStrip_G_Channel   2                //使用了定时器的哪一个通道CCRx(1~4)

#define LightStrip_B_PORT      GPIOB            //引脚端口
#define LightStrip_B_PIN       GPIO_Pin_14      //引脚
#define LightStrip_B_TIM       TIM12            //使用的定时器
#define LightStrip_B_Channel   1                //使用了定时器的哪一个通道CCRx(1~4)
/*--------------------------------------------*/

//定时器通道宏：根据用户选择的通道自动判断,用户无需修改以下内容
#if LightStrip_R_Channel==1
	#define LightStrip_R           LightStrip_R_TIM->CCR1
#elif LightStrip_R_Channel == 2
	#define LightStrip_R           LightStrip_R_TIM->CCR2
#elif LightStrip_R_Channel == 3
	#define LightStrip_R           LightStrip_R_TIM->CCR3
#elif LightStrip_R_Channel == 4
	#define LightStrip_R           LightStrip_R_TIM->CCR4
#else
	#error "LightStrip_R Channel configuration error"
#endif

#if LightStrip_G_Channel==1
	#define LightStrip_G           LightStrip_G_TIM->CCR1
#elif LightStrip_G_Channel == 2
	#define LightStrip_G           LightStrip_G_TIM->CCR2
#elif LightStrip_G_Channel == 3
	#define LightStrip_G           LightStrip_G_TIM->CCR3
#elif LightStrip_G_Channel == 4
	#define LightStrip_G           LightStrip_G_TIM->CCR4
#else
	#error "LightStrip_G Channel configuration error"
#endif

#if LightStrip_B_Channel==1
	#define LightStrip_B           LightStrip_B_TIM->CCR1
#elif LightStrip_B_Channel == 2
	#define LightStrip_B           LightStrip_B_TIM->CCR2
#elif LightStrip_B_Channel == 3
	#define LightStrip_B           LightStrip_B_TIM->CCR3
#elif LightStrip_B_Channel == 4
	#define LightStrip_B           LightStrip_B_TIM->CCR4
#else
	#error "LightStrip_B Channel configuration error"
#endif
/*----------------------------------*/


void Led_Flash(u16 time);

extern int Led_Count;
#endif
