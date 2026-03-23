#include "key.h"

/**************************************************************************
Function: Key initialization
Input   : none
Output  : none
函数功能：按键初始化
入口参数：无
返回  值：无 
**************************************************************************/
void KEY_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	ENABLE_USER_KEY_PIN_CLOCK;
	GPIO_InitStructure.GPIO_Pin = USER_KEY_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	GPIO_Init(USER_KEY_PORT, &GPIO_InitStructure);
} 


/**************************************************************************
函数功能：按键扫描函数
入口参数：执行该函数的任务频率、延迟滤波的时间
返回  值：long_click、double_click、single_click、key_stateless（长按、双击、单击、无状态）
作    者：WHEELTEC
**************************************************************************/
u8 KEY_Scan(u16 Frequency,u16 filter_times)
{
    static u16 time_core;//走时核心
    static u16 long_press_time;//长按识别
    static u8 press_flag=0;//按键按下标记
    static u8 check_once=0;//是否已经识别1次标记
    static u16 delay_mini_1;
    static u16 delay_mini_2;
	
    float Count_time = (((float)(1.0f/(float)Frequency))*1000.0f);//算出计1需要多少个毫秒

    if(check_once)//完成了识别，则清空所有变量
    {
        press_flag=0;//完成了1次识别，标记清零
        time_core=0;//完成了1次识别，时间清零
        long_press_time=0;//完成了1次识别，时间清零
        delay_mini_1=0;
        delay_mini_2=0;
    }
    if(check_once&&KEY==1) check_once=0; //完成扫描后按键被弹起，则开启下一次扫描

    if(KEY==0&&check_once==0)//按键扫描
    {
        press_flag=1;//标记被按下1次
		
        if(++delay_mini_1>filter_times)
        {
            delay_mini_1=0;
            long_press_time++;		
        }
    }

    if(long_press_time>(u16)(500.0f/Count_time))// 长按1秒
    {	
        check_once=1;//标记已被识别
        return long_click; //长按
    }

    //按键被按下1次又弹起后，开启内核走时
    if(press_flag&&KEY==1)
    {
        if(++delay_mini_2>filter_times)
        {
            delay_mini_2=0;
            time_core++; 
        }
    }		
	
    if(press_flag&&(time_core>(u16)(50.0f/Count_time)&&time_core<(u16)(300.0f/Count_time)))//50~700ms内被再次按下
    {
        if(KEY==0) //如果再次按下
        {
            check_once=1;//标记已被识别
            return double_click; //标记为双击
        }
    }
    else if(press_flag&&time_core>(u16)(300.0f/Count_time))
    {
        check_once=1;//标记已被识别
        return single_click; //800ms后还没被按下，则是单击
    }

    return key_stateless;
}

