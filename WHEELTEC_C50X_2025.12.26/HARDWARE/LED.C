#include "led.h"

/**************************************************************************
Function: LED interface initialization
Input   : none
Output  : none
函数功能：LED接口初始化
入口参数：无 
返回  值：无
**************************************************************************/
void LED_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	ENABLE_LED_R_PIN_CLOCK;
	ENABLE_LED_G_PIN_CLOCK;
	ENABLE_LED_B_PIN_CLOCK;
	
	GPIO_InitStructure.GPIO_Pin =  LED_R_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(LED_R_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin =  LED_G_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(LED_G_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin =  LED_B_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(LED_B_PORT, &GPIO_InitStructure);
	
	GPIO_SetBits(LED_R_PORT,LED_R_PIN);
	GPIO_SetBits(LED_G_PORT,LED_G_PIN);
	GPIO_SetBits(LED_B_PORT,LED_B_PIN);
	
	LED_SetColor(LED_Red);//设置LED颜色,默认为红色
}

void V1_0_LED_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	V1_0_ENABLE_LED_R_PIN_CLOCK;
	V1_0_ENABLE_LED_G_PIN_CLOCK;
	V1_0_ENABLE_LED_B_PIN_CLOCK;
	
	GPIO_InitStructure.GPIO_Pin =  V1_0_LED_R_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(V1_0_LED_R_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin =  V1_0_LED_G_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(V1_0_LED_G_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin =  V1_0_LED_B_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(V1_0_LED_B_PORT, &GPIO_InitStructure);
	
	GPIO_SetBits(V1_0_LED_R_PORT,V1_0_LED_R_PIN);
	GPIO_SetBits(V1_0_LED_G_PORT,V1_0_LED_G_PIN);
	GPIO_SetBits(V1_0_LED_B_PORT,V1_0_LED_B_PIN);
	
	LED_SetColor(LED_Red);//设置LED颜色,默认为红色
}

//将用户配置的亮度值 0~255 映射到实际控制PWM的值 0~10000.
static int16_t rgb_map_value(uint8_t user_val)
{
    // 计算输入范围的比例
    float scale = (float)(user_val) / 255.0f ;
    // 使用比例计算输出值
    int16_t result = (int16_t)(scale * 10000);
	
	if( result >=10000  ) result = 10000;
	
	return result;
}

//灯带颜色设置函数
void LightStrip_SetColor( uint8_t r,uint8_t g,uint8_t b )
{
	LightStrip_R = rgb_map_value(r);
	LightStrip_G = rgb_map_value(g);
	LightStrip_B = rgb_map_value(b);
}

//内部函数,初始化pwm的引脚
static void GPIO_PWM_Init(GPIO_TypeDef* GPIOx,uint16_t PINx,TIM_TypeDef* TIMx)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	volatile uint8_t GPIO_PinSourceX;
	for(GPIO_PinSourceX=0;GPIO_PinSourceX<16;GPIO_PinSourceX++)
	{
		if( ( (PINx>>GPIO_PinSourceX)&0x01 )==1 ) break;
	}
	
	uint8_t GPIO_AF = 0;
	      if( TIMx == TIM1 || TIMx == TIM2 )                                       GPIO_AF = 0x01;
	else if( TIMx == TIM3  || TIMx == TIM4  || TIMx == TIM5 )                     GPIO_AF = 0x02;
	else if( TIMx == TIM8  || TIMx == TIM9  || TIMx == TIM10 || TIMx == TIM11 )   GPIO_AF = 0x03;
	else if( TIMx == TIM12 || TIMx == TIM13 || TIMx == TIM14)                     GPIO_AF = 0x09;
	GPIO_PinAFConfig(GPIOx,GPIO_PinSourceX,GPIO_AF);  
	
	GPIO_InitStructure.GPIO_Pin = PINx; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF; 
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOx, &GPIO_InitStructure);
}

//内部函数,pwm的定时器初始化
static void TIMx_PWM_Init(TIM_TypeDef* TIMx,uint8_t TIM_Channel,u16 arr,u16 psc,u16 init_val)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	
	TIM_TimeBaseStructure.TIM_Period = arr; //驱动灯带使用500Hz PWM信号，避免高频导致芯片发热
	TIM_TimeBaseStructure.TIM_Prescaler =psc; //PSC分频
	TIM_TimeBaseStructure.TIM_ClockDivision = 1; //时钟分频系数
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //计数模式
	TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure); //写入配置
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1; //使用PWM1模式
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; //禁用互补输出
	TIM_OCInitStructure.TIM_Pulse = 0; //不分频
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;  //输出极性为高
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Set;//输出极性为高
	
	if( TIM_Channel==1 )
	{
		TIM_OC1Init(TIMx, &TIM_OCInitStructure);
		TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Enable);//使能重装载
		TIM_SetCompare1(TIMx,init_val);
	}
	else if( TIM_Channel==2 )
	{
		TIM_OC2Init(TIMx, &TIM_OCInitStructure);
		TIM_OC2PreloadConfig(TIMx, TIM_OCPreload_Enable);//使能重装载
		TIM_SetCompare2(TIMx,init_val);
	}
	else if( TIM_Channel==3 )
	{
		TIM_OC3Init(TIMx, &TIM_OCInitStructure);
		TIM_OC3PreloadConfig(TIMx, TIM_OCPreload_Enable);//使能重装载
		TIM_SetCompare3(TIMx,init_val);
	}
	else if( TIM_Channel==4 )
	{
		TIM_OC4Init(TIMx, &TIM_OCInitStructure);
		TIM_OC4PreloadConfig(TIMx, TIM_OCPreload_Enable);//使能重装载
		TIM_SetCompare4(TIMx,init_val);
	}
	
	TIM_ARRPreloadConfig(TIMx, ENABLE); //使能TIMx在ARR上的预装载寄存器
	TIM_Cmd(TIMx, ENABLE);  
}

//用户pwm通用初始化函数
static void User_PWM_Init(GPIO_TypeDef* GPIOx,uint16_t PINx,TIM_TypeDef* TIMx,uint8_t TIM_Channel,u16 arr,u16 psc,u16 init_val)
{
	GPIO_PWM_Init(GPIOx,PINx,TIMx);
	TIMx_PWM_Init(TIMx,TIM_Channel,arr,psc,init_val);
}

//RGB灯带初始化
void RGB_LightStrip_Init(void)
{
	ENABLE_LightStrip_R_PORT_CLOCK;
	ENABLE_LightStrip_R_TIM_CLOCK;
	
	ENABLE_LightStrip_G_PORT_CLOCK;
	ENABLE_LightStrip_G_TIM_CLOCK;
	
	ENABLE_LightStrip_B_PORT_CLOCK;
	ENABLE_LightStrip_B_TIM_CLOCK;

	User_PWM_Init(LightStrip_R_PORT,LightStrip_R_PIN,LightStrip_R_TIM,LightStrip_R_Channel,10000-1,84-1 ,rgb_map_value(255));
	User_PWM_Init(LightStrip_G_PORT,LightStrip_G_PIN,LightStrip_G_TIM,LightStrip_G_Channel,10000-1,168-1,rgb_map_value(255));
	User_PWM_Init(LightStrip_B_PORT,LightStrip_B_PIN,LightStrip_B_TIM,LightStrip_B_Channel,10000-1,84-1 ,rgb_map_value(255));
	
	LED_SetColor(LED_Red);//设置LED颜色,默认为红色
}

static u8 led_buf[3]={0,0,0};

//设置LED颜色
void LED_SetColor(LED_COLOR color)
{
		  if(color==LED_Red)       led_buf[0] = LED_ON , led_buf[1] = LED_OFF , led_buf[2] = LED_OFF;
	else if( color==LED_Purple )  led_buf[0] = LED_ON , led_buf[1] = LED_OFF , led_buf[2] = LED_ON;
	else if( color==LED_Blue   )  led_buf[0] = LED_OFF , led_buf[1] = LED_OFF , led_buf[2] = LED_ON;
	else if( color==LED_Cyan   )  led_buf[0] = LED_OFF , led_buf[1] = LED_ON , led_buf[2] = LED_ON;
	else if( color==LED_Green  )  led_buf[0] = LED_OFF , led_buf[1] = LED_ON , led_buf[2] = LED_OFF;
	else if( color==LED_Yellow )  led_buf[0] = LED_ON , led_buf[1] = LED_ON , led_buf[2] = LED_OFF;
	else if( color==LED_White  )  led_buf[0] = LED_ON , led_buf[1] = LED_ON , led_buf[2] = LED_ON;
}

//设置LED亮灭
void LED_SetState(uint8_t mode)
{
	if( SysVal.HardWare_Ver == V1_1 )
	{
		if( mode==1 )
		{
			//pwm模式下的rgb灯带与rgb灯珠控制
			u8 tmp_r=0,tmp_g=0,tmp_b=0;
			tmp_r = (led_buf[0]==LED_ON)?RGB_FULL_ON:RGB_FULL_OFF;
			tmp_g = (led_buf[1]==LED_ON)?RGB_FULL_ON:RGB_FULL_OFF;
			tmp_b = (led_buf[2]==LED_ON)?RGB_FULL_ON:RGB_FULL_OFF;
			
			LightStrip_SetColor(tmp_r,tmp_g,tmp_b);
		}
		else
		{
			LightStrip_SetColor(RGB_FULL_OFF,RGB_FULL_OFF,RGB_FULL_OFF);
		}
	}
	else if( SysVal.HardWare_Ver == V1_0 )
	{
		if( mode==1 )
		{
			V1_0_LED_R = led_buf[0];
			V1_0_LED_G = led_buf[1];
			V1_0_LED_B = led_buf[2];
		}
		else
		{
			V1_0_LED_R = LED_OFF , V1_0_LED_G = LED_OFF , V1_0_LED_B = LED_OFF;
		}
	}
}

/**************************************************************************
Function: The LED flashing
Input   : none
Output  : blink time
函数功能：LED闪烁
入口参数：闪烁时间
返 回 值：无
**************************************************************************/
void Led_Flash(u16 time)
{
	  static int temp;
	  if(0==time) LED_R=0;
	  else		if(++temp==time)	LED_R=~LED_R,temp=0;
}

