#include "encoder.h"

//通用编码器初始化,将传入的定时器以及对应引脚初始化为编码器模式3
static void Encoder_TI12_ModeInit(GPIO_TypeDef* GPIOx_1,uint16_t GPIO_PIN_1,GPIO_TypeDef* GPIOx_2,uint16_t GPIO_PIN_2,TIM_TypeDef* TIMx)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
   
	//引脚配置
    GPIO_InitStructure.GPIO_Pin = GPIO_PIN_1;  
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOx_1, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_PIN_2; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOx_2, &GPIO_InitStructure);
	
	//确认复用的定时器
	//除TIM6、7无通道外,其他均有通道,对应使用复用功能
	uint8_t GPIO_AF;
	      if( TIMx == TIM1 || TIMx == TIM2 )                                      GPIO_AF = 0x01;
	else if( TIMx == TIM3  || TIMx == TIM4  || TIMx == TIM5 )                     GPIO_AF = 0x02;
	else if( TIMx == TIM8  || TIMx == TIM9  || TIMx == TIM10 || TIMx == TIM11 )   GPIO_AF = 0x03;
	else if( TIMx == TIM12 || TIMx == TIM13 || TIMx == TIM14)                     GPIO_AF = 0x09;
	
	//确认1号引脚复用的引脚号
	uint8_t PinSource=0;
	for(PinSource=0;PinSource<16;PinSource++)
	{
		if( ( (GPIO_PIN_1>>PinSource)&0x01 )==1 ) break;
	}
	//复用配置
	GPIO_PinAFConfig(GPIOx_1,PinSource,GPIO_AF);
	
	//确认2号引脚复用的引脚号
	for(PinSource=0;PinSource<16;PinSource++)
	{
		if( ( (GPIO_PIN_2>>PinSource)&0x01 )==1 ) break;
	}
	//复用配置
	GPIO_PinAFConfig(GPIOx_2,PinSource,GPIO_AF);
	
	
	//定时器配置
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler = 0x0; 				    //不分频
    TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD;      //设定计数器自动重装值
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //选择时钟分频：不分频
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数
    TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);    //初始化定时器

	//使用编码器模式3
    TIM_EncoderInterfaceConfig(TIMx, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
	
	//滤波系数配置为0
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICFilter = 0;
    TIM_ICInit(TIMx, &TIM_ICInitStructure);
	
	//清除TIM的更新标志位
    TIM_ClearFlag(TIMx, TIM_FLAG_Update);
	//清空计数值
    TIM_SetCounter(TIMx,0);
	//启动定时器
    TIM_Cmd(TIMx, ENABLE);
}

/**************************************************************************
Function: Encoder interface A initialization
Input   : none
Output  : none
函数功能：编码器接口A初始化
入口参数：无
返 回 值：无
**************************************************************************/
void EncoderA_Init(void)
{
    ENABLE_ENCODER_A_TIM_CLOCK;   //使能定时器
	ENBALE_ENCODER_A1_PORT_CLOCK; //使能1号引脚对应的端口
	ENBALE_ENCODER_A2_PORT_CLOCK; //使能2号引脚对应的端口
	
	//配置编码器A工作在编码器3模式
	Encoder_TI12_ModeInit(ENCODER_A1_PORT,ENCODER_A1_PIN,ENCODER_A2_PORT,ENCODER_A2_PIN,ENCODER_A_TIM);
}


void EncoderB_Init(void)
{
    ENABLE_ENCODER_B_TIM_CLOCK;   //使能定时器
	ENBALE_ENCODER_B1_PORT_CLOCK; //使能1号引脚对应的端口
	ENBALE_ENCODER_B2_PORT_CLOCK; //使能2号引脚对应的端口
	
	//配置编码器B工作在编码器3模式
	Encoder_TI12_ModeInit(ENCODER_B1_PORT,ENCODER_B1_PIN,ENCODER_B2_PORT,ENCODER_B2_PIN,ENCODER_B_TIM);
}


void EncoderC_Init(void)
{
    ENABLE_ENCODER_C_TIM_CLOCK;   //使能定时器
	ENBALE_ENCODER_C1_PORT_CLOCK; //使能1号引脚对应的端口
	ENBALE_ENCODER_C2_PORT_CLOCK; //使能2号引脚对应的端口
   
	//配置编码器C工作在编码器3模式
	Encoder_TI12_ModeInit(ENCODER_C1_PORT,ENCODER_C1_PIN,ENCODER_C2_PORT,ENCODER_C2_PIN,ENCODER_C_TIM);
}


void EncoderD_Init(void)
{
    ENABLE_ENCODER_D_TIM_CLOCK;   //使能定时器
	ENBALE_ENCODER_D1_PORT_CLOCK; //使能1号引脚对应的端口
	ENBALE_ENCODER_D2_PORT_CLOCK; //使能2号引脚对应的端口
   
	//配置编码器D工作在编码器3模式
	Encoder_TI12_ModeInit(ENCODER_D1_PORT,ENCODER_D1_PIN,ENCODER_D2_PORT,ENCODER_D2_PIN,ENCODER_D_TIM);
}


/**************************************************************************
Function: Read the encoder count
Input   : The timer
Output  : Encoder value (representing speed)
函数功能：读取编码器计数
入口参数：定时器
返回  值：编码器数值(代表速度)
**************************************************************************/
short Read_Encoder(ENCODER_t e)
{
    short Encoder_TIM;
    switch(e)
    {
		case Encoder_A:
			Encoder_TIM = (short)ENCODER_A_TIM -> CNT;
			ENCODER_A_TIM -> CNT=0;
			break;
		case Encoder_B:
			Encoder_TIM = (short)ENCODER_B_TIM -> CNT;
			ENCODER_B_TIM -> CNT=0;
			break;
		case Encoder_C:
			Encoder_TIM = (short)ENCODER_C_TIM -> CNT;
			ENCODER_C_TIM -> CNT=0;
			break;
		case Encoder_D:
			Encoder_TIM = (short)ENCODER_D_TIM -> CNT;
			ENCODER_D_TIM -> CNT=0;
			break;
		default:
			Encoder_TIM=0;
    }
    return Encoder_TIM;
}

