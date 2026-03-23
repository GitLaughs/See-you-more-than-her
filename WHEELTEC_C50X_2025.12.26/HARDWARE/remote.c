#include "remote.h"

REMOTER_t remoter;

/**************************************************************************
Function: Model aircraft remote control initialization function, timer 1 input capture initialization
Input   : arr: Automatic reload value, psc: clock preset frequency
Output  : none
函数功能：航模遥控初始化函数，定时器1输入捕获初始化
入口参数：arr：自动重装值，psc：时钟预分频数
返 回 值：无
**************************************************************************/
//引脚通用配置：将传入的引脚设置为复用推挽输出
static void GPIO_AFPP_Init(GPIO_TypeDef* GPIOx,uint16_t PINx)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   =  PINx;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOx, &GPIO_InitStructure);
}

//引脚复用配置
static void GPIOx_PIN_AFConfig(GPIO_TypeDef* GPIOx,uint16_t PINx,TIM_TypeDef* TIMx)
{
	//确认复用的引脚号
	uint8_t PinSource=0;
	for(PinSource=0;PinSource<16;PinSource++)
	{
		if( ( (PINx>>PinSource)&0x01 )==1 ) break;
	}
	
	//确认复用的定时器
	//除TIM6、7无通道外,其他均有通道,对应使用复用功能
	uint8_t GPIO_AF;
	      if( TIMx == TIM1 || TIMx == TIM2 )                                       GPIO_AF = 0x01;
	else if( TIMx == TIM3  || TIMx == TIM4  || TIMx == TIM5 )                     GPIO_AF = 0x02;
	else if( TIMx == TIM8  || TIMx == TIM9  || TIMx == TIM10 || TIMx == TIM11 )   GPIO_AF = 0x03;
	else if( TIMx == TIM12 || TIMx == TIM13 || TIMx == TIM14)                     GPIO_AF = 0x09;
	
	//复用配置
	GPIO_PinAFConfig(GPIOx,PinSource,GPIO_AF);
}

void Remoter_Param_Init(REMOTER_t *p)
{
	p->ch1 = 1500;
	p->ch2 = 1500;
	p->ch3 = 1000;
	p->ch4 = 1500;
	p->check_count = 0;
}

void Remoter_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
	
	//引脚与定时器时钟开启
	ENABLE_REMOTE_TIM_CLOCK;
	ENABLE_REMOTE_CH1_PORT_CLOCK;
	ENABLE_REMOTE_CH2_PORT_CLOCK;
	ENABLE_REMOTE_CH3_PORT_CLOCK;
	ENABLE_REMOTE_CH4_PORT_CLOCK;
	
	//将对应引脚设置为复用输出
	GPIO_AFPP_Init(REMOTE_CH1_PORT,REMOTE_CH1_PIN);
	GPIO_AFPP_Init(REMOTE_CH2_PORT,REMOTE_CH2_PIN);
	GPIO_AFPP_Init(REMOTE_CH3_PORT,REMOTE_CH3_PIN);
	GPIO_AFPP_Init(REMOTE_CH4_PORT,REMOTE_CH4_PIN);
	
	//引脚复用配置
	GPIOx_PIN_AFConfig(REMOTE_CH1_PORT,REMOTE_CH1_PIN,REMOTE_TIM);
	GPIOx_PIN_AFConfig(REMOTE_CH2_PORT,REMOTE_CH2_PIN,REMOTE_TIM);
	GPIOx_PIN_AFConfig(REMOTE_CH3_PORT,REMOTE_CH3_PIN,REMOTE_TIM);
	GPIOx_PIN_AFConfig(REMOTE_CH4_PORT,REMOTE_CH4_PIN,REMOTE_TIM);
	

    /*** Initialize timer 1 || 初始化定时器1 ***/
    //Set the counter to automatically reload //设定计数器自动重装值
    TIM_TimeBaseStructure.TIM_Period = 9999;     //重装载值,最大计数到10000就重新装载
    //Pre-divider //预分频器
    TIM_TimeBaseStructure.TIM_Prescaler = 167;   //预分频168，高级定时器 168M/168 = 1M ==> 1us可计数1
    //Set the clock split: TDTS = Tck_tim //设置时钟分割:TDTS = Tck_tim
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    //TIM up count mode //TIM向上计数模式
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    //Initializes the timebase unit for TIMX based on the parameter specified in TIM_TimeBaseInitStruct
    //根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位
    TIM_TimeBaseInit(REMOTE_TIM, &TIM_TimeBaseStructure);

    /*** 初始化TIM1输入捕获参数，通道1 || Initialize TIM1 for the capture parameter, channel 1 ***/
    //Select input //选择输入端
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
    //Rising edge capture //上升沿捕获
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    //Configure input frequency division, regardless of frequency //配置输入分频,不分频
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    //IC1F=0000 Configure input filter //配置输入滤波器
    TIM_ICInitStructure.TIM_ICFilter = 0x0F;
    TIM_ICInit(REMOTE_TIM, &TIM_ICInitStructure);

    /*** 初始化TIM1输入捕获参数，通道2 || Initialize TIM1 for the capture parameter, channel 2 ***/
    //CC1S=01 Select input //选择输入端
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    //Rising edge capture //上升沿捕获
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    //Configure input frequency division, regardless of frequency //配置输入分频,不分频
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x00;	  //IC1F=0000 配置输入滤波器
    TIM_ICInit(REMOTE_TIM, &TIM_ICInitStructure);

    /*** 初始化TIM1输入捕获参数，通道3 || Initialize TIM1 for the capture parameter, channel 3 ***/
    //Select input //选择输入端
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
    //Rising edge capture //上升沿捕获
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    //Configure input frequency division, regardless of frequency //配置输入分频,不分频
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    //IC1F=0000 Configure input filter //配置输入滤波器，不滤波
    TIM_ICInitStructure.TIM_ICFilter = 0x00;
    TIM_ICInit(REMOTE_TIM, &TIM_ICInitStructure);

    /*** 初始化TIM1输入捕获参数，通道4 || Initialize TIM1 for the capture parameter, channel 4 ***/
    //Select input //选择输入端
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_4;
    //Rising edge capture //上升沿捕获
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    //Configure input frequency division, regardless of frequency //配置输入分频,不分频
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    //IC1F=0000 Configure input filter //配置输入滤波器，不滤波
    TIM_ICInitStructure.TIM_ICFilter = 0x00;
    TIM_ICInit(REMOTE_TIM, &TIM_ICInitStructure);

    /*** interrupt packet initialization || 中断分组初始化 ***/
    //TIM1 interrupts //TIM1中断
    NVIC_InitStructure.NVIC_IRQChannel = REMOTE_TIM_IRQn ;
    //Preempt priority 0 //先占优先级0级
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    //Level 0 from priority //从优先级0级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    //IRQ channels are enabled //IRQ通道被使能
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    //Initializes the peripheral NVIC register according to the parameters specified in NVIC_InitStruct
    //根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器
    NVIC_Init(&NVIC_InitStructure);

    //Allow CC1IE,CC2IE,CC3IE,CC4IE to catch interrupts, not allowed update_interrupts
    //不允许更新中断，允许CC1IE,CC2IE,CC3IE,CC4IE捕获中断
    TIM_ITConfig(REMOTE_TIM, TIM_IT_CC1|TIM_IT_CC2|TIM_IT_CC3|TIM_IT_CC4,ENABLE);

    //Enable timer //使能定时器
    TIM_Cmd(REMOTE_TIM, ENABLE);
}
