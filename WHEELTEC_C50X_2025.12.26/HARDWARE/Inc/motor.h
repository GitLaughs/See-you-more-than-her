#ifndef __MOTOR_H
#define __MOTOR_H

#include "sys.h"

/*--------moto Interface Fun --------*/
//对外开放的接口
#define SERVO_INIT     1500                             //Servo zero point //舵机零点
#define FULL_DUTYCYCLE 16800                            //轮子电机的满占空比数值,与初始化设置相关.
void MiniBalance_PWM_Init(u16 arr,u16 psc);            //直流电机初始化
void V1_0_MiniBalance_PWM_Init(u16 arr,u16 psc);      //直流电机初始化
void Servo_Senior_Init(u16 arr,u16 psc,int servomid); //高配阿克曼车型舵机初始化
void Servo_Top_Init(u16 arr,u16 psc,int servomid);    //顶配阿克曼车型舵机初始化
/*--------------------------------------------*/


//用户配置User config,当不涉及功能修改时,若发生引脚更换,仅修改该文件下的引脚参数即可
/*--------Motor_A User config--------*/
//PWM引脚配置
#define ENABLE_PWMA_TIM_CLOCK   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8,ENABLE)  //PWMA时钟使能
#define ENABLE_PWMA_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE) //PWMA对应的引脚端口使能
	
#define PWMA_PORT      GPIOC            //PWMA引脚端口
#define PWMA_PIN       GPIO_Pin_9       //PWMA引脚
#define PWMA_TIM       TIM8             //PWMA使用的定时器
#define PWMA_Channel   4                //使用了定时器的哪一个通道CCRx(1~4)

//电机方向引脚配置
#define ENABLE_AIN1_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)
#define ENABLE_AIN2_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)

#define AIN1 	            PBout(13)//AIN1
#define AIN1_PORT           GPIOB
#define AIN1_PIN            GPIO_Pin_13

#define AIN2 	            PBout(12)//AIN2
#define AIN2_PORT           GPIOB
#define AIN2_PIN            GPIO_Pin_12
/*----------------------------------*/

/*--------Motor_B User config--------*/
//PWM引脚配置
#define ENABLE_PWMB_TIM_CLOCK   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8,ENABLE)  //PWMB时钟使能
#define ENABLE_PWMB_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE) //PWMB对应的引脚端口使能
	
#define PWMB_PORT      GPIOC            //PWMB引脚端口
#define PWMB_PIN       GPIO_Pin_8       //PWMB引脚
#define PWMB_TIM       TIM8             //PWMB使用的定时器
#define PWMB_Channel   3                //使用了定时器的哪一个通道CCRx(1~4)

//电机方向引脚配置
#define ENABLE_BIN1_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE)
#define ENABLE_BIN2_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE)

#define BIN1 	            PCout(0)//BIN1
#define BIN1_PORT           GPIOC
#define BIN1_PIN            GPIO_Pin_0

#define BIN2 	            PEout(15)//BIN2
#define BIN2_PORT           GPIOE
#define BIN2_PIN            GPIO_Pin_15
/*----------------------------------*/

/*--------Motor_C User config--------*/
//PWM引脚配置
#define ENABLE_PWMC_TIM_CLOCK   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8,ENABLE) //PWMC时钟使能
#define ENABLE_PWMC_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE) //PWMC对应的引脚端口使能
	
#define PWMC_PORT      GPIOC            //PWMC引脚端口
#define PWMC_PIN       GPIO_Pin_7       //PWMC引脚
#define PWMC_TIM       TIM8             //PWMC使用的定时器
#define PWMC_Channel   2                //使用了定时器的哪一个通道CCRx(1~4)

//电机方向引脚配置
#define ENABLE_CIN1_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE)
#define ENABLE_CIN2_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE)

#define CIN1 	        PDout(10)//CIN1
#define CIN1_PORT       GPIOD
#define CIN1_PIN        GPIO_Pin_10

#define CIN2 	        PDout(12)//CIN2
#define CIN2_PORT       GPIOD
#define CIN2_PIN        GPIO_Pin_12
/*----------------------------------*/

/*--------Motor_D User config--------*/
//PWM引脚配置
#define ENABLE_PWMD_TIM_CLOCK   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8,ENABLE) //PWMD时钟使能
#define ENABLE_PWMD_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE) //PWMD对应的引脚端口使能
	
#define PWMD_PORT      GPIOC            //PWMD引脚端口
#define PWMD_PIN       GPIO_Pin_6       //PWMD引脚
#define PWMD_TIM       TIM8             //PWMD使用的定时器
#define PWMD_Channel   1                //使用了定时器的哪一个通道CCRx(1~4)

//电机方向引脚配置
#define ENABLE_DIN1_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE)
#define ENABLE_DIN2_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE)

#define DIN1 	        PCout(12)//DIN1
#define DIN1_PORT       GPIOC
#define DIN1_PIN        GPIO_Pin_12

#define DIN2 	        PAout(8) //DIN2
#define DIN2_PORT       GPIOA
#define DIN2_PIN        GPIO_Pin_8
/*----------------------------------*/

/*--------Senior akm Servo User config--------*/
//高配阿克曼车型舵机配置
//PWM引脚配置
#define ENABLE_SERVO_SENIOR_TIM_CLOCK   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9,ENABLE)  //舵机定时器时钟使能
#define ENABLE_SERVO_SENIOR_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE) //舵机对应的引脚端口使能
	
#define SERVO_SENIOR_PORT      GPIOA            //舵机引脚端口
#define SERVO_SENIOR_PIN       GPIO_Pin_2       //舵机引脚
#define SERVO_SENIOR_TIM       TIM9             //舵机使用的定时器
#define SERVO_SENIOR_Channel   1                //使用了定时器的哪一个通道CCRx(1~4)
/*--------------------------------------------*/

/*--------Top akm Servo User config--------*/
//顶配阿克曼车型舵机配置
#define ENABLE_SERVO_TOP_TIM_CLOCK   RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4,ENABLE) //舵机时钟使能
#define ENABLE_SERVO_TOP_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE) //舵机对应的引脚端口使能
	
#define SERVO_TOP_PORT      GPIOB            //舵机引脚端口
#define SERVO_TOP_PIN       GPIO_Pin_7       //舵机引脚
#define SERVO_TOP_TIM       TIM4             //舵机使用的定时器
#define SERVO_TOP_Channel   2                //使用了定时器的哪一个通道CCRx(1~4)
/*--------------------------------------------*/

/*-------- V1.0 Motor_B User config--------*/
//V1.00版本硬件的电机方向引脚配置
#define V1_0_ENABLE_BIN1_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE)
#define V1_0_ENABLE_BIN2_CLOCK   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE)

#define V1_0_BIN1 	              PCout(0)//BIN1
#define V1_0_BIN1_PORT            GPIOC
#define V1_0_BIN1_PIN             GPIO_Pin_0

#define V1_0_BIN2 	              PBout(14)//BIN2
#define V1_0_BIN2_PORT            GPIOB
#define V1_0_BIN2_PIN             GPIO_Pin_14
/*----------------------------------*/



//根据用户配置生成的配置,不需要用户修改
//The configuration generated based on user configuration does not require user modification
#if PWMA_Channel==1
	#define PWMA           PWMA_TIM->CCR1
#elif PWMA_Channel == 2
	#define PWMA           PWMA_TIM->CCR2
#elif PWMA_Channel == 3
	#define PWMA           PWMA_TIM->CCR3
#elif PWMA_Channel == 4
	#define PWMA           PWMA_TIM->CCR4
#else
	#error "PWM_A Channel configuration error"
#endif

#if PWMB_Channel==1
	#define PWMB           PWMB_TIM->CCR1
#elif PWMB_Channel == 2
	#define PWMB           PWMB_TIM->CCR2
#elif PWMB_Channel == 3
	#define PWMB           PWMB_TIM->CCR3
#elif PWMB_Channel == 4
	#define PWMB           PWMB_TIM->CCR4
#else
	#error "PWM_B Channel configuration error"
#endif

#if PWMC_Channel==1
	#define PWMC           PWMC_TIM->CCR1
#elif PWMC_Channel == 2
	#define PWMC           PWMC_TIM->CCR2
#elif PWMC_Channel == 3
	#define PWMC           PWMC_TIM->CCR3
#elif PWMC_Channel == 4
	#define PWMC           PWMC_TIM->CCR4
#else
	#error "PWM_C Channel configuration error"
#endif

#if PWMD_Channel==1
	#define PWMD           PWMD_TIM->CCR1
#elif PWMD_Channel == 2
	#define PWMD           PWMD_TIM->CCR2
#elif PWMD_Channel == 3
	#define PWMD           PWMD_TIM->CCR3
#elif PWMD_Channel == 4
	#define PWMD           PWMD_TIM->CCR4
#else
	#error "PWM_D Channel configuration error"
#endif

#if SERVO_SENIOR_Channel==1
	#define SERVO_SENIOR           SERVO_SENIOR_TIM->CCR1
#elif SERVO_SENIOR_Channel == 2
	#define SERVO_SENIOR           SERVO_SENIOR_TIM->CCR2
#elif SERVO_SENIOR_Channel == 3
	#define SERVO_SENIOR           SERVO_SENIOR_TIM->CCR3
#elif SERVO_SENIOR_Channel == 4
	#define SERVO_SENIOR           SERVO_SENIOR_TIM->CCR4
#else
	#error "SERVO_SENIOR_Channel Channel configuration error"
#endif

#if SERVO_TOP_Channel==1
	#define SERVO_TOP           SERVO_TOP_TIM->CCR1
#elif SERVO_TOP_Channel == 2
	#define SERVO_TOP           SERVO_TOP_TIM->CCR2
#elif SERVO_TOP_Channel == 3
	#define SERVO_TOP           SERVO_TOP_TIM->CCR3
#elif SERVO_TOP_Channel == 4
	#define SERVO_TOP           SERVO_TOP_TIM->CCR4
#else
	#error "SERVO_TOP_Channel Channel configuration error"
#endif


/*---------------------------------------------------------------*/

#endif
