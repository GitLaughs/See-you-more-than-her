#ifndef __UARTX_CALLBACK_H
#define __UARTX_CALLBACK_H 

#include "system.h"

//机器人接收控制命令的数据长度
#define RECEIVE_DATA_SIZE 11

//机器人接收控制命令的结构体
typedef struct _RECEIVE_DATA_  
{
	unsigned char buffer[RECEIVE_DATA_SIZE];
	struct _Control_Str_
	{
		unsigned char Frame_Header; //1 bytes //1个字节
		float X_speed;	            //4 bytes //4个字节
		float Y_speed;              //4 bytes //4个字节
		float Z_speed;              //4 bytes //4个字节
		unsigned char Frame_Tail;   //1 bytes //1个字节
	}Control_Str;
}RECEIVE_DATA;
// IP帧配置参数
#define IP_REQUEST_HEADER_1  0xAA   // IP请求帧头第一字节
#define IP_REQUEST_HEADER_2  0xBB   // IP请求帧头第二字节
#define IP_CONFIRM_COUNT     50     // 确认次数：收到50次有效IP后停止请求(约2.5秒@20Hz)
#define IP_REQUEST_TIMEOUT   6000    // 请求超时次数：300秒 @ 20Hz


//内部函数
static float XYZ_Target_Speed_transition(u8 High,u8 Low);
static u8 AT_Command_Capture(u8 uart_recv);
static void _System_Reset_(u8 uart_recv);
void RobotControl_SetDebugLevel(char uart_recv);

extern u8 Debug_Flag;
extern u8 IP_Frame_Valid;           
extern u8 IP_Request_Enable ;                  // IP请求使能标志（1=发送请求，0=停止请求）


extern u8 Received_IP[4];  // 存储IP地址的4个字节
#endif

