#include "led_task.h"

/**************************************************************************
Function: LED light flashing task
Input   : none
Output  : none
函数功能：LED灯闪烁任务
入口参数：无
返回  值：无
**************************************************************************/
void led_task(void *pvParameters)
{
    while(1)
    {
        static u8 led_state=0;//LED亮灭设置
		
		//检测自动回充状态,对LED灯颜色进行修改
		static u8 check_charger_mode;
		static u8 check_Charging;
		
		//进入回充状态,LED颜色为黄色
		if( 0 == check_charger_mode && 1 == charger.AllowRecharge ) LED_SetColor(LED_Yellow);
		
		//退出回充状态,LED颜色设置默认颜色
		if( 1 == check_charger_mode && 0 == charger.AllowRecharge ) LED_SetColor(LED_Red);
		check_charger_mode = charger.AllowRecharge;
		
		if( 1==charger.AllowRecharge ) 
		{	//自动回充状态下充电成功时将LED设置为紫色
			if( 0==check_Charging && 1 == charger.Charging ) LED_SetColor(LED_Purple);
			if( 1==check_Charging && 0 == charger.Charging ) LED_SetColor(LED_Yellow);
			check_Charging = charger.Charging; 
		}

        //LED闪烁任务
        led_state = !led_state;
        LED_SetState(led_state);
		
        //The LED flicker task is very simple, requires low frequency accuracy, and uses the relative delay function
        //LED闪烁任务非常简单，对频率精度要求低，使用相对延时函数
        vTaskDelay(SysVal.LED_delay);
    }
}


