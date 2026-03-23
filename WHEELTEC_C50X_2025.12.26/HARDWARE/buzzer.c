#include "buzzer.h"

/**************************************************************************
Function: Buzzer interface initialized
Input   : none
Output  : none
函数功能：蜂鸣器接口初始化
入口参数：无 
返回  值：无
**************************************************************************/
void Buzzer_Init(void)
{	
	GPIO_InitTypeDef  GPIO_InitStructure;
	ENABLE_Buzzer_PIN_CLOCK;
	GPIO_InitStructure.GPIO_Pin =  Buzzer_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(Buzzer_PORT, &GPIO_InitStructure);
	
	Buzzer = BEEP_OFF;
}

/**************************************************************************
Function function: buzzer response function
Entry parameters: number of beeps, beep time interval, and frequency of executing the function
Return value: None
Author: WHEELTEC
函数功能：蜂鸣器响应函数
入口参数：蜂鸣次数,蜂鸣时间间隔,执行该函数的频率
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static uint8_t Buzzer_Response(uint8_t numt,uint8_t times,uint8_t rate)
{
	uint16_t real_times = times*10;       //用户要求的蜂鸣间隔时间,单位ms
	uint8_t taskover = 0;                 //用于统计任务是否执行完毕
	uint16_t time_ratio = (uint16_t)( (1.0f/(float)rate) * 1000 );//时间最小单位,与任务频率相关.单位ms
	static uint16_t buzzer_timecore = 0; //任务时间,从任务启动开始累计时间
	static uint8_t done_count = 0;       //完成蜂鸣的次数
	
	if( numt==0 && times == 0 ) //不接收全0参数
	{
		taskover = 1;
		buzzer_timecore = 0;
		done_count = 0;
		Buzzer = BEEP_OFF;
		return taskover;
	}
	
	//检查用户要求的时间间隔是否超出计时最小分辨率
	if( real_times < time_ratio ) 
	{
		//无法到达的时间间隔,不响应任务
		taskover = 1;
		buzzer_timecore = 0;
		done_count = 0;
		Buzzer = BEEP_OFF;
		return taskover;
	}

	//任务走时
	buzzer_timecore += (1.0f/(float)rate) * 1000 ; // 任务内时间,单位 ms
	
	//执行任务
	if( buzzer_timecore<real_times )
	{
		Buzzer = BEEP_ON;
	}
	else if( buzzer_timecore>=real_times && buzzer_timecore<real_times*2 )
	{
		Buzzer = BEEP_OFF;
	}
	else if( buzzer_timecore>=real_times*2 )
	{
		buzzer_timecore = 0;//重新计时
		done_count ++ ;     //记录完成一次蜂鸣
		if(done_count==numt)
		{
			Buzzer = BEEP_OFF;    //确保蜂鸣器不会发声
			done_count = 0;
			taskover = 1;
			return taskover;//完成任务
		}
	}
	return taskover;
}


/* ------------------ 蜂鸣器任务队列相关变量,不可直接修改. ------------------- */
#define BUZZER_QUEUE_LEN 10 //蜂鸣器任务队列的长度

//蜂鸣器的任务队列.共 BUZZER_QUEUE_LEN 组 ( 蜂鸣次数(u8) , 蜂鸣时间(u8)) 的任务
static uint8_t buzzer_task_buf[BUZZER_QUEUE_LEN][2];
static uint8_t buzzer_NowLen = 0;//蜂鸣器当前任务队列长度
static uint8_t buzzer_task_tail = 0;//队列尾部
static uint8_t buzzer_task_head = 0;//队列头
/* ----------------------------------------------------------------------- */

/**************************************************************************
Functionality: Add a task to the buzzer execution queue.
Input Parameters: Number of buzzes, time interval between each buzz (in units of *10ms).
Return Value: 0: Task addition failed, 1: Task addition succeeded.
Author: WHEELTEC
函数功能：加入任务到蜂鸣器执行任务中
入口参数：蜂鸣次数，每次蜂鸣的时间间隔（单位*10ms）
返回  值：0:任务添加失败 1:任务添加成功
作    者：WHEELTEC
**************************************************************************/
uint8_t Buzzer_AddTask(uint8_t num_time,uint8_t times )
{
	//检查任务队列是否已满
	if( buzzer_NowLen >= BUZZER_QUEUE_LEN )
	{
		return 0;
	}
	else
	{
		//加入任务到环形队列尾部
		buzzer_task_buf[buzzer_task_tail][0] = num_time;
		buzzer_task_buf[buzzer_task_tail][1] = times;
		
		//队列尾部环形自增
		buzzer_task_tail = (buzzer_task_tail + 1)%BUZZER_QUEUE_LEN;
		
		//统计队列内数据长度
		buzzer_NowLen++;
	}
	return 1;
}



/**************************************************************************
Functionality: Buzzer task execution, retrieving and executing tasks added by the
               user through the Buzzer_AddTask() function one by one from the task queue,
               with a fixed time interval of 1 second between each task.
Return Value: None.
Author: WHEELTEC
函数功能：蜂鸣器任务，将用户通过函数 Buzzer_AddTask() 添加的任务队取出并列逐个任务执行.其中每个任务之间时间间隔固定为0.5秒
入口参数：执行蜂鸣任务的频率
返回  值：无
作    者：WHEELTEC
**************************************************************************/
void Buzzer_task(uint8_t rate)
{
	static uint8_t task_over = 1;
	static uint8_t task_delay = 0;//任务与任务之间的时间间隔
	static uint8_t start_delay = 0;//任务间隔完成标志位. 0可执行任务,1等待中
	
	//任务具体内容存放处
	static uint8_t task[2]={0,0};
	
	if( start_delay==0 ) //任务间隔延时完毕
	{
		//队列非空且当前没有蜂鸣任务在执行,则取出任务队列中的任务按顺序执行
		if( buzzer_NowLen!=0 && task_over==1 ) 
		{
			//取出一组任务
			task[0] = buzzer_task_buf[buzzer_task_head][0];
			task[1] = buzzer_task_buf[buzzer_task_head][1];
			
			//环形队列头递增
			buzzer_task_head = (buzzer_task_head + 1)%BUZZER_QUEUE_LEN;
			
			//统计队列内数据长度
			buzzer_NowLen --;
		}
	}

	//任务取出后,开始执行
	task_over = Buzzer_Response(task[0],task[1],rate);
	
	//完成任务后清空任务值
	if( task_over==1 ) task[0]=0,task[1]=0,start_delay=1;
	
	//任务等待：任务与任务之间,等待0.5秒以作区分
	if(start_delay)
	{
		task_delay++;
		if( task_delay>=rate*0.5 ) task_delay = 0,start_delay=0;
	}
	
}
