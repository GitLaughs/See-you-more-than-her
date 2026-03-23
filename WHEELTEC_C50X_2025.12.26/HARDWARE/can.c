#include "can.h"
#include "system.h"

//CAN接口对应的引脚初始化
static void GPIO_CANPort_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure; 
	
	ENABLE_CAN1_TX_PIN_CLOCK;
	ENABLE_CAN1_RX_PIN_CLOCK;
	
	//初始化GPIO
	//CAN1
	GPIO_InitStructure.GPIO_Pin = CAN1_TX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;     
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;    
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      //上拉
	GPIO_Init(CAN1_TX_PORT, &GPIO_InitStructure);           
	
	GPIO_InitStructure.GPIO_Pin = CAN1_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;     
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;   
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      //上拉
	GPIO_Init(CAN1_RX_PORT, &GPIO_InitStructure);    
	
	//引脚复用映射配置
	GPIO_PinAFConfig(CAN1_TX_PORT,CAN1_TX_Soure,GPIO_AF_CAN1); 
	GPIO_PinAFConfig(CAN1_RX_PORT,CAN1_RX_Soure,GPIO_AF_CAN1);
}

//CAN接口对应的引脚初始化
static void V1_0_GPIO_CANPort_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure; 
	
	V1_0_ENABLE_CAN1_TX_PIN_CLOCK;
	V1_0_ENABLE_CAN1_RX_PIN_CLOCK;
	
	//初始化GPIO
	//CAN1
	GPIO_InitStructure.GPIO_Pin = V1_0_CAN1_TX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;     
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;    
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      //上拉
	GPIO_Init(V1_0_CAN1_TX_PORT, &GPIO_InitStructure);           
	
	GPIO_InitStructure.GPIO_Pin = V1_0_CAN1_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;     
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;   
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      //上拉
	GPIO_Init(V1_0_CAN1_RX_PORT, &GPIO_InitStructure);    
	
	//引脚复用映射配置
	GPIO_PinAFConfig(V1_0_CAN1_TX_PORT,V1_0_CAN1_TX_Soure,GPIO_AF_CAN1); 
	GPIO_PinAFConfig(V1_0_CAN1_RX_PORT,V1_0_CAN1_TX_Soure,GPIO_AF_CAN1);
}

/**************************************************************************
Function: CAN1 initialization
Input   : tsjw：Resynchronize the jump time unit, Scope: 1 ~ 3;
 			    tbs2：Time unit of time period 2, range :1~8;
 			    tbs1：Time unit of time period 1, range :1~16;
 			    brp ：Baud rate divider, range :1 to 1024;(We're actually going to add 1, which is 1 to 1024) tq=(brp)*tpclk1
 			    mode：0, normal mode;1. Loop mode;
Output  : 0- Initialization successful;Other - initialization failed
Note: none of the entry parameters (except mode) can be 0
函数功能：CAN1初始化
入口参数：tsjw：重新同步跳跃时间单元，范围:1~3;
 			    tbs2：时间段2的时间单元，范围:1~8;
 			    tbs1：时间段1的时间单元，范围:1~16;
 			    brp ：波特率分频器，范围:1~1024;(实际要加1,也就是1~1024) tq=(brp)*tpclk1
 			    mode：0,普通模式;1,回环模式;
返回  值：0-初始化成功; 其他-初始化失败
注意：入口参数(除了mode)均不能为0
波特率/Baud rate=Fpclk1/((tbs1+tbs2+1)*brp)，Fpclk1为36M
                =42M/((3+2+1)*6)
						    =1M
**************************************************************************/
u8 CAN1_Mode_Init(u8 tsjw,u8 tbs2,u8 tbs1,u16 brp,u8 mode)
{
	CAN_InitTypeDef        CAN_InitStructure;
	CAN_FilterInitTypeDef  CAN_FilterInitStructure;
	NVIC_InitTypeDef  NVIC_InitStructure;
	
	//-1是因为这些配置实际数值从0开始
	//brp不需要-1，因为其配置数值是从1开始
 	if(tsjw==0||tbs2==0||tbs1==0||brp==0) return 1;
	tsjw-=1; //Subtract 1 before setting //先减去1.再用于设置
	tbs2-=1;
	tbs1-=1;
	
	//CAN口对应的GPIO初始化
	GPIO_CANPort_Init();
	
	//使能相关时钟
	ENABLE_CAN1_CLOCK;
	
	//CAN单元设置
	CAN_InitStructure.CAN_TTCM=DISABLE;	//非时间触发通信模式   
	CAN_InitStructure.CAN_ABOM=DISABLE;	//软件自动离线管理	  
	CAN_InitStructure.CAN_AWUM=DISABLE; //睡眠模式通过软件唤醒(清除CAN->MCR的SLEEP位)
	CAN_InitStructure.CAN_NART=ENABLE;	//禁止报文自动传送 
	CAN_InitStructure.CAN_RFLM=DISABLE;	//报文不锁定,新的覆盖旧的  
	CAN_InitStructure.CAN_TXFP=DISABLE;	//优先级由报文标识符决定 
	CAN_InitStructure.CAN_Mode= mode;	//模式设置 
	
	//波特率相关配置
	CAN_InitStructure.CAN_SJW=tsjw;	    //重新同步跳跃宽度(Tsjw)为tsjw+1个时间单位 CAN_SJW_1tq~CAN_SJW_4tq
	CAN_InitStructure.CAN_BS1=tbs1;     //Tbs1范围CAN_BS1_1tq ~CAN_BS1_16tq
	CAN_InitStructure.CAN_BS2=tbs2;     //Tbs2范围CAN_BS2_1tq ~CAN_BS2_8tq
	CAN_InitStructure.CAN_Prescaler=brp;//分频系数(Fdiv)为brp+1	
	
	CAN_Init(CAN1, &CAN_InitStructure);   // 初始化CAN1 	
	
	//配置过滤器
	//CAN1
	CAN_FilterInitStructure.CAN_FilterNumber=0;	                   //过滤器0
	CAN_FilterInitStructure.CAN_FilterMode=CAN_FilterMode_IdMask;  //屏蔽模式
	CAN_FilterInitStructure.CAN_FilterScale=CAN_FilterScale_32bit; //32位 
	CAN_FilterInitStructure.CAN_FilterIdHigh=0x0000;               //32位ID
	CAN_FilterInitStructure.CAN_FilterIdLow=0x0000; 	

	CAN_FilterInitStructure.CAN_FilterMaskIdHigh=0x0000;//32位MASK
	CAN_FilterInitStructure.CAN_FilterMaskIdLow=0x0000;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment=CAN_Filter_FIFO0;//过滤器0关联到FIFO0
	CAN_FilterInitStructure.CAN_FilterActivation=ENABLE; //激活过滤器0
	CAN_FilterInit(&CAN_FilterInitStructure);            //滤波器初始化
	
	//CAN1 FIFO0 中断使能
	#if CAN1_RX0_INT_ENABLE
	CAN_ITConfig(CAN1,CAN_IT_FMP0,ENABLE);//FIFO0消息挂号中断允许.		    
	NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;// 主优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;       // 次优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	#endif
	
	//CAN2 FIFO1 中断使能
	#if CAN2_RX1_INT_ENABLE
	CAN_ITConfig(CAN2,CAN_IT_FMP1,ENABLE);//FIFO1消息挂号中断允许.		    
	NVIC_InitStructure.NVIC_IRQChannel = CAN2_RX1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 主优先级为1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        // 次优先级为0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	#endif
	
	return 0;
}   

//V1.00版本CAN口初始化函数
u8 V1_0_CAN1_Mode_Init(u8 tsjw,u8 tbs2,u8 tbs1,u16 brp,u8 mode)
{
	CAN_InitTypeDef        CAN_InitStructure;
	CAN_FilterInitTypeDef  CAN_FilterInitStructure;
	NVIC_InitTypeDef  NVIC_InitStructure;

	//-1是因为这些配置实际数值从0开始
	//brp不需要-1，因为其配置数值是从1开始
 	if(tsjw==0||tbs2==0||tbs1==0||brp==0) return 1;
	tsjw-=1; //Subtract 1 before setting //先减去1.再用于设置
	tbs2-=1;
	tbs1-=1;
	
	//CAN口对应的GPIO初始化
	V1_0_GPIO_CANPort_Init();
	
	//使能相关时钟
	ENABLE_CAN1_CLOCK;       
	
	//引脚复用映射配置
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource11,GPIO_AF_CAN1); 
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource12,GPIO_AF_CAN1);
	
	//CAN单元设置
	CAN_InitStructure.CAN_TTCM=DISABLE;	//非时间触发通信模式   
	CAN_InitStructure.CAN_ABOM=DISABLE;	//软件自动离线管理	  
	CAN_InitStructure.CAN_AWUM=DISABLE; //睡眠模式通过软件唤醒(清除CAN->MCR的SLEEP位)
	CAN_InitStructure.CAN_NART=ENABLE;	//禁止报文自动传送 
	CAN_InitStructure.CAN_RFLM=DISABLE;	//报文不锁定,新的覆盖旧的  
	CAN_InitStructure.CAN_TXFP=DISABLE;	//优先级由报文标识符决定 
	CAN_InitStructure.CAN_Mode= mode;	//模式设置 
	
	//波特率相关配置
	CAN_InitStructure.CAN_SJW=tsjw;	    //重新同步跳跃宽度(Tsjw)为tsjw+1个时间单位 CAN_SJW_1tq~CAN_SJW_4tq
	CAN_InitStructure.CAN_BS1=tbs1;     //Tbs1范围CAN_BS1_1tq ~CAN_BS1_16tq
	CAN_InitStructure.CAN_BS2=tbs2;     //Tbs2范围CAN_BS2_1tq ~CAN_BS2_8tq
	CAN_InitStructure.CAN_Prescaler=brp;//分频系数(Fdiv)为brp+1	
	
	CAN_Init(CAN1, &CAN_InitStructure);   // 初始化CAN1 	
	
	//配置过滤器
	//CAN1
	CAN_FilterInitStructure.CAN_FilterNumber=0;	                   //过滤器0
	CAN_FilterInitStructure.CAN_FilterMode=CAN_FilterMode_IdMask;  //屏蔽模式
	CAN_FilterInitStructure.CAN_FilterScale=CAN_FilterScale_32bit; //32位 
	CAN_FilterInitStructure.CAN_FilterIdHigh=0x0000;               //32位ID
	CAN_FilterInitStructure.CAN_FilterIdLow=0x0000; 	

	CAN_FilterInitStructure.CAN_FilterMaskIdHigh=0x0000;//32位MASK
	CAN_FilterInitStructure.CAN_FilterMaskIdLow=0x0000;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment=CAN_Filter_FIFO0;//过滤器0关联到FIFO0
	CAN_FilterInitStructure.CAN_FilterActivation=ENABLE; //激活过滤器0
	CAN_FilterInit(&CAN_FilterInitStructure);            //滤波器初始化
	
	//CAN1 FIFO0 中断使能
	#if CAN1_RX0_INT_ENABLE
	CAN_ITConfig(CAN1,CAN_IT_FMP0,ENABLE);//FIFO0消息挂号中断允许.		    
	NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;// 主优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;       // 次优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	#endif
	
	//CAN2 FIFO1 中断使能
	#if CAN2_RX1_INT_ENABLE
	CAN_ITConfig(CAN2,CAN_IT_FMP1,ENABLE);//FIFO1消息挂号中断允许.		    
	NVIC_InitStructure.NVIC_IRQChannel = CAN2_RX1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 主优先级为1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        // 次优先级为0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	#endif
	
	return 0;
}   

// CAN1发送函数
u8 CAN1_Send_Num(u32 id,u8 *data)
{
	//CAN发送数据结构体
	CanTxMsg msg;
	
	u16 i;   //超时时间
	u8 mbox; //
	
	#if 1 //配置标准帧与id
	msg.StdId = id;
	msg.IDE = CAN_Id_Standard;
	#else //配置扩展帧与id
	msg.ExtId = id;
	msg.IDE = CAN_Id_Extended;
	#endif
	
	//发送的报文属于数据帧
	//数据帧 CAN_RTR_DATA
	//遥控帧 CAN_RTR_REMOTE
	msg.RTR = CAN_RTR_DATA;
	
	//发送的数据长度
	// 1~8
	msg.DLC = 8;
	
	//将要发送的数据复制入 msg.Data
	memcpy(msg.Data,data,8);
	
	//CAN1 发送
	CAN_Transmit(CAN1,&msg);
	
	//等待CAN1发送完成
	while( CAN_TransmitStatus(CAN1,mbox)== CAN_TxStatus_Failed && i<0xffff ) i++;
	
	//发送超时
	if(i>=0xffff) return 1;
	
	return 0;
}

u8 CAN1_Send_EXTid_Num(u32 id,u8 *data)
{
	//CAN发送数据结构体
	CanTxMsg msg;
	
	u16 i;   //超时时间
	u8 mbox; //
	
	#if 0 //配置标准帧与id
	msg.StdId = id;
	msg.IDE = CAN_Id_Standard;
	#else //配置扩展帧与id
	msg.ExtId = id;
	msg.IDE = CAN_Id_Extended;
	#endif
	
	//发送的报文属于数据帧
	//数据帧 CAN_RTR_DATA
	//遥控帧 CAN_RTR_REMOTE
	msg.RTR = CAN_RTR_DATA;
	
	//发送的数据长度
	// 1~8
	msg.DLC = 8;
	
	//将要发送的数据复制入 msg.Data
	memcpy(msg.Data,data,8);
	
	//CAN1 发送
	CAN_Transmit(CAN1,&msg);
	
	//等待CAN1发送完成
	while( CAN_TransmitStatus(CAN1,mbox)== CAN_TxStatus_Failed && i<0xffff ) i++;
	
	//发送超时
	if(i>=0xffff) return 1;
	
	return 0;
}
