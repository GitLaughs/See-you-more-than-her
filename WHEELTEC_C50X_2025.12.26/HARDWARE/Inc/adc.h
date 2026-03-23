#ifndef __ADC_H
#define __ADC_H	
#include "sys.h"

//阿克曼转向结构 数值结构体
typedef struct{
	int Bias;                      //前轮转角零点值,可作为转向结构的纠正补偿调节
	int Mid;                       //舵机中值,用于矫正小车复位重启时舵机的方向
	int Max;                       //舵机转动的最大值
	int Min;                       //舵机转动的最小值
}AKM_SERVO_ADC;


/*--------ADC Interface Fun--------*/
//对外接口
void ADC1_Init(void);
void ADC2_Init(void);
void Akm_ServoParam_Init(AKM_SERVO_ADC* p);
int get_DMA_SlideRes(void);
int get_DMA_ServoBias(void);
u16 Get_ADC1(u8 ch);
u16 Get_ADC1_Average(u8 chn, u8 times);
float Get_battery_volt(void);
extern AKM_SERVO_ADC Akm_Servo;
/*----------------------------------*/

/*--------Battery_PIN config--------*/
//电池电压采集的引脚
#define ENABLE_BATTERY_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE)
#define Battery_PORT  GPIOB
#define Battery_PIN   GPIO_Pin_0
#define Battery_Ch    ADC_Channel_8
/*----------------------------------*/

/*--------CarMode_PIN config--------*/
//用于检测车型调节电位器的引脚
#define ENABLE_CarMode_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE)
#define CarMode_PORT  GPIOC
#define CarMode_PIN   GPIO_Pin_2
#define CarMode_Ch    ADC_Channel_12
/*----------------------------------*/

/*--------TurnAdjust_PIN config--------*/
//阿克曼转向结构补偿电位器采集引脚
#define ENABLE_ServoAdj_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE)
#define ServoAdj_PORT  GPIOC
#define ServoAdj_PIN   GPIO_Pin_3
#define ServoAdj_Ch    ADC_Channel_13
/*----------------------------------*/

/*--------Slide_PIN config--------*/
//转向滑轨数据采集引脚
#define ENABLE_SLIDE_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE)
#define SLIDE_PORT  GPIOC
#define SLIDE_PIN   GPIO_Pin_1
#define SLIDE_Ch    ADC_Channel_11

//滑轨数据引脚对应DMA配置
#define ENABLE_DMAx_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE)
#define DMA_REG_ADDR      (uint32_t)(&ADC2->DR)
#define DMAx_Streamx      DMA2_Stream3
#define DMA_ChannelX      DMA_Channel_1
/*----------------------------------*/


#endif 


