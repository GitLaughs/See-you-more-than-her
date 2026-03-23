/***********************************************
公司：轮趣科技（东莞）有限公司
品牌：WHEELTEC
官网：wheeltec.net
淘宝店铺：shop114407458.taobao.com 
速卖通: https://minibalance.aliexpress.com/store/4455017
版本：V1.01
修改时间：2024-06-25

Company: WHEELTEC Co.Ltd
Brand: WHEELTEC
Website: wheeltec.net
Taobao shop: shop114407458.taobao.com 
Aliexpress: https://minibalance.aliexpress.com/store/4455017
Version: V1.01
Update：2024-06-25

All rights reserved
***********************************************/
#include "system.h"

//Task priority    //任务优先级
#define START_TASK_PRIO	1

//Task stack size //任务堆栈大小	
#define START_STK_SIZE 	512  

//Task handle     //任务句柄
TaskHandle_t StartTask_Handler;

//Task function   //任务函数
void start_task(void *pvParameters);

//Main function //主函数
int main(void)
{ 
	systemInit(); //Hardware initialization //硬件初始化

	//Create the start task //创建开始任务
	xTaskCreate((TaskFunction_t )start_task,            //Task function   //任务函数
							(const char*    )"start_task",          //Task name       //任务名称
							(uint16_t       )START_STK_SIZE,        //Task stack size //任务堆栈大小
							(void*          )NULL,                  //Arguments passed to the task function //传递给任务函数的参数
							(UBaseType_t    )START_TASK_PRIO,       //Task priority   //任务优先级
							(TaskHandle_t*  )&StartTask_Handler);   //Task handle     //任务句柄    					
	vTaskStartScheduler();  //Enables task scheduling //开启任务调度	
}
 
//Start task task function //开始任务任务函数
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL(); //Enter the critical area //进入临界区
	
    //Create the task //创建任务
	xTaskCreate(Balance_task,  "Balance_task",  BALANCE_STK_SIZE,  NULL, BALANCE_TASK_PRIO,  NULL);	//Vehicle motion control task //小车运动控制任务
	xTaskCreate(show_task,     "show_task",     SHOW_STK_SIZE,     NULL, SHOW_TASK_PRIO,     NULL);  //User interaction tasks related to data display //与数据显示相关的用户交互任务
	xTaskCreate(led_task,      "led_task",      LED_STK_SIZE,      NULL, LED_TASK_PRIO,      NULL);	 //LED light flashing task //LED灯闪烁任务
	xTaskCreate(data_task,     "DATA_task",     DATA_STK_SIZE,     NULL, DATA_TASK_PRIO,     &data_TaskHandle);	 //Send data to each interface task //将数据发送到各个接口任务
	if(SysVal.HardWare_Ver==V1_0) 	//IMU data read task //IMU数据读取任务,根据不同的硬件版本启动不同的任务.
	{
		xTaskCreate(MPU6050_task,  "IMU_task",  IMU_STK_SIZE,  NULL, IMU_TASK_PRIO,  NULL);
		xTaskCreate(pstwo_task,    "PSTWO_task",    PS2_STK_SIZE,      NULL, PS2_TASK_PRIO,      &show_TaskHandle);	 //Read the PS2 controller task //读取PS2手柄任务
	}
		
	else if( SysVal.HardWare_Ver==V1_1 )
	{
		xTaskCreate(ICM20948_task,  "IMU_task",  IMU_STK_SIZE,  NULL, IMU_TASK_PRIO,  NULL);
	}

	
    vTaskDelete(StartTask_Handler); //Delete the start task //删除开始任务

    taskEXIT_CRITICAL();            //Exit the critical section//退出临界区
}






