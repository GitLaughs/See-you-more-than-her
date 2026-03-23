#include "motor.h"

/**************************************************************************
Function: Motor orientation pin initialization
Input   : none
Output  : none
函数功能：电机方向引脚初始化
入口参数：无
返回  值：无
**************************************************************************/
//引脚通用配置：将传入的引脚设置为推挽输出模式
static void GPIO_Output_Init(GPIO_TypeDef* GPIOx,uint16_t PINx)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   =  PINx;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOx, &GPIO_InitStructure);
}

//定时器通用配置：将输入的定时器、对应的引脚，配置为PWM1输出模式 
void TIMx_PWM1_Mode_Init(GPIO_TypeDef* GPIOx,uint16_t PINx,TIM_TypeDef* TIMx,uint8_t TIM_Channel,u16 arr,u16 psc,u16 init_val)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
	
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
	
	//配置对应引脚为复用功能
    GPIO_InitStructure.GPIO_Pin = PINx;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;       
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;	
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;      
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      
    GPIO_Init(GPIOx,&GPIO_InitStructure);           
	
    //Sets the value of the auto-reload register cycle for the next update event load activity
    //设置在下一个更新事件装入活动的自动重装载寄存器周期的值
    TIM_TimeBaseStructure.TIM_Period = arr;
    //Sets the pre-divider value used as the TIMX clock frequency divisor
    //设置用来作为TIMx时钟频率除数的预分频值
    TIM_TimeBaseStructure.TIM_Prescaler =psc;
    //Set the clock split :TDTS = Tck_tim
    //设置时钟分割:TDTS = Tck_tim
    TIM_TimeBaseStructure.TIM_ClockDivision = 1;
    //Up counting mode
    //向上计数模式
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    //Initializes the timebase unit for TIMX based on the parameter specified in TIM_TIMEBASEINITSTRUCT
    //根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位
    TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);

    //Select Timer mode :TIM Pulse Width Modulation mode 1
    //选择定时器模式:TIM脉冲宽度调制模式1
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    //Compare output enablement
    //比较输出使能
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    //Output polarity :TIM output polarity is higher
    //输出极性:TIM输出比较极性高
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	
    //Initialize the peripheral TIMX based on the parameter specified in TIM_OCINITSTRUCT
    //根据TIM_OCInitStruct中指定的参数初始化外设TIMx
	if( TIM_Channel==1 )
	{
		TIM_OC1Init(TIMx, &TIM_OCInitStructure);
		TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Enable);
		TIM_SetCompare1(TIMx,init_val);
	}
	else if( TIM_Channel==2 )
	{
		TIM_OC2Init(TIMx, &TIM_OCInitStructure);
		TIM_OC2PreloadConfig(TIMx, TIM_OCPreload_Enable);
		TIM_SetCompare2(TIMx,init_val);
	}
	else if( TIM_Channel==3 )
	{
		TIM_OC3Init(TIMx, &TIM_OCInitStructure);
		TIM_OC3PreloadConfig(TIMx, TIM_OCPreload_Enable);
		TIM_SetCompare3(TIMx,init_val);
	}
	else if( TIM_Channel==4 )
	{
		TIM_OC4Init(TIMx, &TIM_OCInitStructure);
		TIM_OC4PreloadConfig(TIMx, TIM_OCPreload_Enable);
		TIM_SetCompare4(TIMx,init_val);
	}
	
	
	if( TIMx==TIM1 || TIMx==TIM8  || TIMx==TIM9)
	{
		// Advanced timer output must be enabled
		//高级定时器输出必须使能这句
		TIM_CtrlPWMOutputs(TIMx,ENABLE);
	}

    // Enable the TIMX preloaded register on the ARR
    //使能TIMx在ARR上的预装载寄存器
    TIM_ARRPreloadConfig(TIMx, ENABLE);
	
    //Enable TIM
    //使能定时器
    TIM_Cmd(TIMx, ENABLE);
}

static void MiniBalance_Motor_Init(void)
{
	//开启对应引脚时钟
	ENABLE_AIN1_CLOCK;
	ENABLE_AIN2_CLOCK;
	ENABLE_BIN1_CLOCK;
	ENABLE_BIN2_CLOCK;
	ENABLE_CIN1_CLOCK;
	ENABLE_CIN2_CLOCK;
	ENABLE_DIN1_CLOCK;
	ENABLE_DIN2_CLOCK;
	
	//设置对应引脚推挽输出
	GPIO_Output_Init(AIN1_PORT,AIN1_PIN);
	GPIO_Output_Init(AIN2_PORT,AIN2_PIN);
	
	GPIO_Output_Init(BIN1_PORT,BIN1_PIN);
	GPIO_Output_Init(BIN2_PORT,BIN2_PIN);
	
	GPIO_Output_Init(CIN1_PORT,CIN1_PIN);
	GPIO_Output_Init(CIN2_PORT,CIN2_PIN);
	
	GPIO_Output_Init(DIN1_PORT,DIN1_PIN);
	GPIO_Output_Init(DIN2_PORT,DIN2_PIN);
	
	AIN1 = 0; AIN2 = 0;
	BIN1 = 0; BIN2 = 0;
	CIN1 = 0; CIN2 = 0;
	DIN1 = 0; DIN2 = 0;
}

static void V1_0_MiniBalance_Motor_Init(void)
{
	//开启对应引脚时钟
	ENABLE_AIN1_CLOCK;
	ENABLE_AIN2_CLOCK;
	V1_0_ENABLE_BIN1_CLOCK;
	V1_0_ENABLE_BIN2_CLOCK;
	ENABLE_CIN1_CLOCK;
	ENABLE_CIN2_CLOCK;
	ENABLE_DIN1_CLOCK;
	ENABLE_DIN2_CLOCK;
	
	//设置对应引脚推挽输出
	GPIO_Output_Init(AIN1_PORT,AIN1_PIN);
	GPIO_Output_Init(AIN2_PORT,AIN2_PIN);
	
	GPIO_Output_Init(V1_0_BIN1_PORT,V1_0_BIN1_PIN);
	GPIO_Output_Init(V1_0_BIN2_PORT,V1_0_BIN2_PIN);
	
	GPIO_Output_Init(CIN1_PORT,CIN1_PIN);
	GPIO_Output_Init(CIN2_PORT,CIN2_PIN);
	
	GPIO_Output_Init(DIN1_PORT,DIN1_PIN);
	GPIO_Output_Init(DIN2_PORT,DIN2_PIN);
	
	AIN1 = 0; AIN2 = 0;
	V1_0_BIN1 = 0; V1_0_BIN2 = 0;
	CIN1 = 0; CIN2 = 0;
	DIN1 = 0; DIN2 = 0;
}

/**************************************************************************
Function: The motor PWM initialization
Input   : arr: Automatic reload value, psc: clock preset frequency
Output  : none
函数功能：电机PWM引脚初始化
入口参数：arr：自动重装值  psc：时钟预分频数
返回  值：无
**************************************************************************/
void MiniBalance_PWM_Init(u16 arr,u16 psc)
{
	MiniBalance_Motor_Init();//电机方向引脚初始化
	
	//使能PWM对应的定时器与引脚端口时钟
	ENABLE_PWMA_TIM_CLOCK;
	ENABLE_PWMA_PORT_CLOCK;
	
	ENABLE_PWMB_TIM_CLOCK;
	ENABLE_PWMB_PORT_CLOCK;
	
	ENABLE_PWMC_TIM_CLOCK;
	ENABLE_PWMC_PORT_CLOCK;
	
	ENABLE_PWMD_TIM_CLOCK;
	ENABLE_PWMD_PORT_CLOCK;
	
	//将对应的定时器与对应的引脚配置为 PWM1 模式
	TIMx_PWM1_Mode_Init(PWMA_PORT,PWMA_PIN,PWMA_TIM,PWMA_Channel,arr,psc,0);
	TIMx_PWM1_Mode_Init(PWMB_PORT,PWMB_PIN,PWMB_TIM,PWMB_Channel,arr,psc,0);
	TIMx_PWM1_Mode_Init(PWMC_PORT,PWMC_PIN,PWMC_TIM,PWMC_Channel,arr,psc,0);
	TIMx_PWM1_Mode_Init(PWMD_PORT,PWMD_PIN,PWMD_TIM,PWMD_Channel,arr,psc,0);
	
}

void V1_0_MiniBalance_PWM_Init(u16 arr,u16 psc)
{
	V1_0_MiniBalance_Motor_Init();//电机方向引脚初始化
	
	//使能PWM对应的定时器与引脚端口时钟
	ENABLE_PWMA_TIM_CLOCK;
	ENABLE_PWMA_PORT_CLOCK;
	
	ENABLE_PWMB_TIM_CLOCK;
	ENABLE_PWMB_PORT_CLOCK;
	
	ENABLE_PWMC_TIM_CLOCK;
	ENABLE_PWMC_PORT_CLOCK;
	
	ENABLE_PWMD_TIM_CLOCK;
	ENABLE_PWMD_PORT_CLOCK;
	
	//将对应的定时器与对应的引脚配置为 PWM1 模式
	TIMx_PWM1_Mode_Init(PWMA_PORT,PWMA_PIN,PWMA_TIM,PWMA_Channel,arr,psc,0);
	TIMx_PWM1_Mode_Init(PWMB_PORT,PWMB_PIN,PWMB_TIM,PWMB_Channel,arr,psc,0);
	TIMx_PWM1_Mode_Init(PWMC_PORT,PWMC_PIN,PWMC_TIM,PWMC_Channel,arr,psc,0);
	TIMx_PWM1_Mode_Init(PWMD_PORT,PWMD_PIN,PWMD_TIM,PWMD_Channel,arr,psc,0);
}

/**************************************************************************
Function: Senior Ackerman model servo initialization
Input   : ARR: Automatic reload value  PSC: clock preset frequency
Output  : none
函数功能：高配阿克曼车型舵机初始化
入口参数：arr：自动重装值  psc：时钟预分频数
返回  值：无
**************************************************************************/
void Servo_Senior_Init(u16 arr,u16 psc,int servomid)
{
	ENABLE_SERVO_SENIOR_TIM_CLOCK;
	ENABLE_SERVO_SENIOR_PORT_CLOCK;
	TIMx_PWM1_Mode_Init(SERVO_SENIOR_PORT,SERVO_SENIOR_PIN,SERVO_SENIOR_TIM,SERVO_SENIOR_Channel,arr,psc,servomid);
}

void Servo_Top_Init(u16 arr,u16 psc,int servomid)
{
	ENABLE_SERVO_TOP_TIM_CLOCK;
	ENABLE_SERVO_TOP_PORT_CLOCK;
	TIMx_PWM1_Mode_Init(SERVO_TOP_PORT,SERVO_TOP_PIN,SERVO_TOP_TIM,SERVO_TOP_Channel,arr,psc,servomid);
}
