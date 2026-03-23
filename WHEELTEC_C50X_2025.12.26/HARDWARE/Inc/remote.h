#ifndef __REMOTE_H
#define __REMOTE_H
#include "sys.h"

//干扰判断的阈值.当航模的活性统计值超出该阈值，则认为当前是干扰信号或者是无航模信号,复位航模遥控的数值,防止小车出现失控行为.
//阈值越小检测越灵敏,阈值越大越缓慢.
#define Filter_Threshold 10

//遥控相关的变量
typedef struct  
{
	int ch1; //航模通道1~4读取到的数值,与控制相关
	int ch2; 
	int ch3;
	int ch4;
	uint16_t check_count; //航模遥控器活性统计,用于检查是否是真实的航模信号,过滤干扰
}REMOTER_t;

//对外接口
void Remoter_Init(void);
extern REMOTER_t remoter;
void Remoter_Param_Init(REMOTER_t *p);


/*--------Remote User config--------*/
//航模遥控配置
#define ENABLE_REMOTE_TIM_CLOCK   RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1,ENABLE)  //PWMA时钟使能

#define ENABLE_REMOTE_CH1_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE) //PWMA对应的引脚端口使能
#define ENABLE_REMOTE_CH2_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE) //PWMA对应的引脚端口使能
#define ENABLE_REMOTE_CH3_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE) //PWMA对应的引脚端口使能
#define ENABLE_REMOTE_CH4_PORT_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE) //PWMA对应的引脚端口使能
	
#define REMOTE_TIM            TIM1	
#define REMOTE_TIM_IRQn       TIM1_CC_IRQn        //航模数据采集对应的定时器中断
#define REMOTE_TIM_IRQHandler TIM1_CC_IRQHandler //航模数据采集对应的中断服务函数

#define REMOTE_CH1_PORT      GPIOE            
#define REMOTE_CH1_PIN       GPIO_Pin_9       

#define REMOTE_CH2_PORT      GPIOE            
#define REMOTE_CH2_PIN       GPIO_Pin_11

#define REMOTE_CH3_PORT      GPIOE           
#define REMOTE_CH3_PIN       GPIO_Pin_13 

#define REMOTE_CH4_PORT      GPIOE            
#define REMOTE_CH4_PIN       GPIO_Pin_14 

/*----------------------------------*/


//航模遥控接收 通道对应的定时器通道
#define Set_CH1_Rising  TIM_OC1PolarityConfig(REMOTE_TIM, TIM_ICPolarity_Rising)  //上升沿配置
#define Set_CH2_Rising  TIM_OC2PolarityConfig(REMOTE_TIM, TIM_ICPolarity_Rising)
#define Set_CH3_Rising  TIM_OC3PolarityConfig(REMOTE_TIM, TIM_ICPolarity_Rising)
#define Set_CH4_Rising  TIM_OC4PolarityConfig(REMOTE_TIM, TIM_ICPolarity_Rising)

#define Set_CH1_Falling TIM_OC1PolarityConfig(REMOTE_TIM, TIM_ICPolarity_Falling)  //下降沿配置
#define Set_CH2_Falling TIM_OC2PolarityConfig(REMOTE_TIM, TIM_ICPolarity_Falling)
#define Set_CH3_Falling TIM_OC3PolarityConfig(REMOTE_TIM, TIM_ICPolarity_Falling)
#define Set_CH4_Falling TIM_OC4PolarityConfig(REMOTE_TIM, TIM_ICPolarity_Falling)

#define Get_CH1_CNT TIM_GetCapture1(REMOTE_TIM) //获取计数器的值
#define Get_CH2_CNT TIM_GetCapture2(REMOTE_TIM)
#define Get_CH3_CNT TIM_GetCapture3(REMOTE_TIM)
#define Get_CH4_CNT TIM_GetCapture4(REMOTE_TIM)

#define Get_CH1_State TIM_GetITStatus(REMOTE_TIM, TIM_IT_CC1) //获取中断的状态
#define Get_CH2_State TIM_GetITStatus(REMOTE_TIM, TIM_IT_CC2)
#define Get_CH3_State TIM_GetITStatus(REMOTE_TIM, TIM_IT_CC3)
#define Get_CH4_State TIM_GetITStatus(REMOTE_TIM, TIM_IT_CC4)

#define Clear_CH1_State TIM_ClearITPendingBit(REMOTE_TIM, TIM_IT_CC1) //清除中断标志位
#define Clear_CH2_State TIM_ClearITPendingBit(REMOTE_TIM, TIM_IT_CC2)
#define Clear_CH3_State TIM_ClearITPendingBit(REMOTE_TIM, TIM_IT_CC3)
#define Clear_CH4_State TIM_ClearITPendingBit(REMOTE_TIM, TIM_IT_CC4)


void Remoter_Init(void);


#endif
