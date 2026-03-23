#include "adc.h"
#include "motor.h"

//阿克曼转向滑轨数据
AKM_SERVO_ADC Akm_Servo;

//使用DMA采集的数据量
#define akm_servo_bufsize 80 //每组ADC数据共采集的个数
static uint16_t adc_OriBuf[akm_servo_bufsize][2]; //滑轨位置 和 舵机调整电位器 的原始数值(ADC数值)

/**************************************************************************
Function: ADC initializes battery voltage detection
Input   : none
Output  : none
函数功能：ADC初始化电池电压检测
入口参数：无
返回  值：无
**************************************************************************/
static void ADC1_Pin_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	
	ENABLE_BATTERY_PIN_CLOCK;
    GPIO_InitStructure.GPIO_Pin = Battery_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;//模拟输入
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//不带上下拉
    GPIO_Init(Battery_PORT, &GPIO_InitStructure);
	
	ENABLE_CarMode_PIN_CLOCK;
    GPIO_InitStructure.GPIO_Pin = CarMode_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;//模拟输入
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//不带上下拉
    GPIO_Init(CarMode_PORT, &GPIO_InitStructure);
}


static void ADC2_Pin_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	ENABLE_SLIDE_PIN_CLOCK;
    GPIO_InitStructure.GPIO_Pin = SLIDE_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;//模拟输入
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//不带上下拉
    GPIO_Init(SLIDE_PORT, &GPIO_InitStructure);
	
	ENABLE_ServoAdj_PIN_CLOCK;
    GPIO_InitStructure.GPIO_Pin = ServoAdj_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;//模拟输入
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//不带上下拉
    GPIO_Init(ServoAdj_PORT, &GPIO_InitStructure);
}

static void ADCx_DMAConfig_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    ENABLE_DMAx_CLOCK;

    DMA_DeInit(DMAx_Streamx);
    while(DMA_GetCmdStatus(DMAx_Streamx)!=DISABLE); //等待DMA可配置
    DMA_InitStructure.DMA_Channel = DMA_ChannelX; //选择DMA通道
    DMA_InitStructure.DMA_PeripheralBaseAddr = DMA_REG_ADDR; //外设的地址
    DMA_InitStructure.DMA_Memory0BaseAddr = (u32)adc_OriBuf; //存储区的地址
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory; //外设 到 存储区 模式
    DMA_InitStructure.DMA_BufferSize = akm_servo_bufsize*2; //要传输的字节数
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; //外设地址不自增
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; //存储区地址自增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; //外设传输半字(16位)
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord; //存储区保存半字
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular; //循环模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_High; //高优先级

    //////////// FIFO配置Disable后无需配置一下参数 ////////////
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;//存储器突发单次传输
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;//外设突发单次传输
    //////////// *** ////////////
    DMA_Init(DMAx_Streamx, &DMA_InitStructure);//初始化DMA

    DMA_Cmd(DMAx_Streamx, ENABLE);  //开启DMA传输
}

void ADC1_Init(void)
{
	ADC1_Pin_Init();//使用ADC1引脚配置
	
    ADC_CommonInitTypeDef  ADC_CommonInitStructure;
    ADC_InitTypeDef        ADC_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); //使能ADC1时钟 
	
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1,ENABLE);	  //ADC1复位
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1,DISABLE);  //复位结束

    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;                     //独立模式
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles; //两个采样阶段之间的延迟5个时钟
    ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;      //DMA失能
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;                  //预分频4分频。ADCCLK=PCLK2/4=84/4=21Mhz,ADC时钟最好不要超过36Mhz
    ADC_CommonInit(&ADC_CommonInitStructure);                                    //初始化

    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;                       //12位模式
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;                                //非扫描模式
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;                          //关闭连续转换
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;  //禁止触发检测，使用软件触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;                       //右对齐
    ADC_InitStructure.ADC_NbrOfConversion = 1;                                   //1个转换在规则序列中 也就是只转换规则序列1

    ADC_Init(ADC1, &ADC_InitStructure);//ADC1初始化
    ADC_Cmd(ADC1, ENABLE);//开启AD转换器
}

void ADC2_Init(void)
{
	ADC2_Pin_Init();//ADC2使用者引脚配置
	
    ADC_CommonInitTypeDef ADC_CommonInitStructure;
    ADC_InitTypeDef       ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE); //使能ADC2时钟

    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC2,ENABLE);	   	 //ADC2复位
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC2,DISABLE);	 //复位结束

    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;                    //独立模式
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;//两个采样阶段之间的延迟5个时钟
    ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;     //DMA失能
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;                 //预分频4分频。ADCCLK=PCLK2/4=84/4=21Mhz,ADC时钟最好不要超过36Mhz
    ADC_CommonInit(&ADC_CommonInitStructure);                                   //初始化

    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;                      //12位模式
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;                                //多通道时,需要使能扫描模式
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;                          //连续转换
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None; //禁止触发检测，使用软件触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;                      //右对齐
    ADC_InitStructure.ADC_NbrOfConversion = 2;                                  //1个转换在规则序列中 也就是只转换规则序列1
    ADC_Init(ADC2, &ADC_InitStructure);                                         //ADC初始化

    //配置规则通道与DMA请求
    ADC_RegularChannelConfig(ADC2,SLIDE_Ch,1,ADC_SampleTime_480Cycles);    //第一采集滑轨数据,存放于        Akm_Servo.adc_OriBuf[x][0]
	ADC_RegularChannelConfig(ADC2,ServoAdj_Ch,2,ADC_SampleTime_480Cycles); //第二采集右上角电位器数据,存放于 Akm_Servo.adc_OriBuf[x][1]
	
	//有数据转换完，自动生成DMA请求
    ADC_DMARequestAfterLastTransferCmd(ADC2,ENABLE);
	
	//ADC2启动
    ADC_Cmd(ADC2, ENABLE);
	
	//DMA配置
	ADCx_DMAConfig_Init();
	
	//ADC2 DMA搬运请求使能
    ADC_DMACmd(ADC2,ENABLE);
	
    //触发转换,由DMA完成数据搬运
    ADC_SoftwareStartConv(ADC2);
}

//返回DMA采集的滑轨数据平均值
int get_DMA_SlideRes(void)
{
	int tmp = 0;
	for(u8 i=0;i<akm_servo_bufsize;i++)
	{
		tmp += adc_OriBuf[i][0];
	}
	tmp = ( tmp/(float)akm_servo_bufsize ) - 2048;
	return tmp;
}

//返回右上角电位器的数据平均值
int get_DMA_ServoBias(void)
{
	int tmp = 0;
	for(u8 i=0;i<akm_servo_bufsize;i++)
	{
		tmp += adc_OriBuf[i][1];
	}
	tmp = ( tmp/(float)akm_servo_bufsize ) - 2048;
	return tmp;
}

//阿克曼舵机参数初始化
void Akm_ServoParam_Init(AKM_SERVO_ADC* p)
{
	p->Max = 2000;
	p->Min = 1000;
	p->Mid = SERVO_INIT;
}

/**************************************************************************
Function: The AD sampling
Input   : The ADC channels
Output  : AD conversion results
函数功能：AD采样
入口参数：ADC的通道
返回  值：AD转换结果
**************************************************************************/
u16 Get_ADC1(volatile u8 ch)
{
    //Sets the specified ADC rule group channel, one sequence, and sampling time
    //设置指定ADC的规则组通道，一个序列，采样时间

    //ADC1,ADC通道,采样时间为480周期
    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_480Cycles );
	
    //Enable the specified ADC1 software transformation startup function
    //使能指定的ADC1的软件转换启动功能
    ADC_SoftwareStartConv(ADC1);
	
    //Wait for the conversion to finish
    //等待转换结束
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC ));
	
    //Returns the result of the last ADC1 rule group conversion
    //返回最近一次ADC1规则组的转换结果
    return ADC_GetConversionValue(ADC1);
}

/**************************************************************************
Function: Collect multiple ADC values to calculate the average function
Input   : ADC channels and collection times
Output  : AD conversion results
函数功能：采集多次ADC值求平均值函数
入口参数：ADC通道和采集次数
返 回 值：AD转换结果
**************************************************************************/
u16 Get_ADC1_Average(u8 chn, u8 times)
{
    u32 temp_val=0;
    u8 t;
    for(t=0; t<times; t++)
    {
        temp_val+=Get_ADC1(chn);
    }
    return temp_val/times;
}

/**************************************************************************
Function: Read the battery voltage
Input   : none
Output  : Battery voltage in V
函数功能：读取电池电压
入口参数：无
返回  值：电池电压，单位 v
**************************************************************************/
float Get_battery_volt(void)
{
    float Volt;
    //The resistance partial voltage can be obtained by simple analysis according to the schematic diagram
    //电阻分压，具体根据原理图简单分析可以得到
    Volt=Get_ADC1(Battery_Ch)*3.3*11.0/1.0/4096;
    return Volt;
}


