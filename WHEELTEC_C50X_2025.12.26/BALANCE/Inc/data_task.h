#ifndef __DATA_TASK_H
#define __DATA_TASK_H

#include "system.h"

#define DATA_TASK_PRIO		4             //Task priority //任务优先级
#define DATA_STK_SIZE 		512           //Task stack size //任务堆栈大小
#define DATA_TASK_RATE      RATE_20_HZ   //任务频率


//基础24字节帧头、帧尾、数据长度
#define FRAME_HEADER      0X7B //Frame_header //帧头
#define FRAME_TAIL        0X7D //Frame_tail   //帧尾
#define SEND_DATA_SIZE    24

//自动回充帧头、帧尾、数据长度
#define AutoCharge_HEADER      0X7C //Frame_header //帧头
#define AutoCharge_TAIL        0X7F //Frame_tail   //帧尾
#define AutoCharge_DATA_SIZE    8

//IMU三轴数据
typedef struct __Mpu6050_Data_ 
{
	short X_data; //2 bytes //2个字节
	short Y_data; //2 bytes //2个字节
	short Z_data; //2 bytes //2个字节
}Mpu6050_Data;

//机器人三轴速度
typedef struct __Robot_Data_ 
{
	short X_speed; //2 bytes //2个字节
	short Y_speed; //2 bytes //2个字节
	short Z_speed; //2 bytes //2个字节
}Robot_Vel;

//发送数据结构体定义
typedef struct _SEND_DATA_  
{
	struct _Sensor_Str_
	{
		unsigned char Frame_Header; //1个字节
		Robot_Vel    Vel;            //6字节
		Mpu6050_Data Accelerometer;  //6 bytes //6个字节
		Mpu6050_Data Gyroscope;      //6 bytes //6个字节	
		short Power_Voltage;        //2 bytes //2个字节
		unsigned char Frame_Tail;   //1 bytes //1个字节
	}Sensor_Str;
	
	unsigned char buffer[SEND_DATA_SIZE];//实际用于发送数据的缓冲区
}SEND_DATA;

//发送数据结构体定义
typedef struct _SEND_AutoCharge_DATA_  
{
	unsigned char buffer[AutoCharge_DATA_SIZE];
	struct _AutoCharge_Str_
	{
		unsigned char Frame_Header; //1 bytes //1个字节
		short Charging_Current;	    //2 bytes //2个字节
		unsigned char RED;          //1 bytes //1个字节
		unsigned char Charging;     //1 bytes //1个字节
		unsigned char yuliu;		//1 bytes //1个字节
		unsigned char Frame_Tail;   //1 bytes //1个字节
	}AutoCharge_Str;
}SEND_AutoCharge_DATA;
//对外使用变量
uint8_t Check_BCC(const uint8_t *data, uint16_t length);
void data_task(void *pvParameters);

extern TaskHandle_t data_TaskHandle ;
extern u8 Debug_Flag;
extern u8 Received_IP[4];  // 存储IP地址的4个字节
//内部使用函数
static float* Kinematics_akm_diff(float motorA,float motorB);
static float* Kinematics_omni(float motorA,float motorB,float motorC);
static float* Kinematics_mec_4wd(float motorA,float motorB,float motorC,float motorD);
static void data_transition(void);
static void Usart1_SendTask(void);
static void Usart3_SendTask(void);
static void CAN1_SendTask(void);

#endif
