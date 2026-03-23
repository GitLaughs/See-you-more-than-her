#ifndef __AUTORECHARGE_H
#define __AUTORECHARGE_H
#include "sys.h"

typedef struct{
	u8 L_A;          //四路红外信号情况. 1收到信号 0无信号
	u8 L_B;
	u8 R_A;
	u8 R_B;
	u8 RED_STATE;    //四路红外信号汇总 取值0~4
	u8 NavWalk;      //导航接管回充控制的标志位 1已接管 0无状态
	u8 AllowRecharge;//自动回充模式标志位 1回充模式 0正常模式
	u8 Charging;     //充电标志位 1充电 0未充电
	
	u8 RedRefalsh_Time; //红外信号识别时刷新的时间，单位 10ms.
	
	float ChargingCurrent;//充电电流大小
	
	float Dock_MoveX; //设置对接时速度的大小,此值直接赋值给Red_Move
	float Dock_MoveY;
	float Dock_MoveZ;
	
	float Red_MoveX; //识别到红外信号给出的小车3轴速度
	float Red_MoveY;
	float Red_MoveZ;
	
	float Up_MoveX;//导航接管时,给出的小车3轴速度
	float Up_MoveY;
	float Up_MoveZ;
	
	u8 OutLine_Check;//充电装备离线检测
	
}CHARGER_t;


//对外接口
extern CHARGER_t charger;
void auto_recharge_reset(void);
void CAN_Send_AutoRecharge(void);
void Find_Charging_HardWare(void);

//自动回充的处理逻辑函数
void Handle_Normal_AutoRecharge(uint8_t* temp_rxbuf);
void Handle_AKM_AutoRecharge(uint8_t* temp_rxbuf);

#if defined AKM_CAR
//以下是阿克曼小车自动回充专用.在阿克曼使用自动回充时,速度执行来源是自身,而不采用充电装备的速度逻辑.\
(原因是阿克曼对接逻辑不同,充电装备上的逻辑需要满足其他小车,不能只满足阿克曼.故阿克曼独立在控制板端)
#define cur_front_left do{                          \
			charger.Red_MoveX = -charger.Dock_MoveX ,\
			charger.Red_MoveZ =  0.1f ;\
}while(0)

#define cur_front_right do{                          \
			charger.Red_MoveX = -charger.Dock_MoveX ,\
			charger.Red_MoveZ = -0.1f ;\
}while(0)


#define front_left do{                          \
			charger.Red_MoveX = -charger.Dock_MoveX ,\
			charger.Red_MoveZ =  charger.Dock_MoveZ ;\
}while(0)

#define front_right do{                          \
			charger.Red_MoveX = -charger.Dock_MoveX ,\
			charger.Red_MoveZ = -charger.Dock_MoveZ ;\
}while(0)

#define back_left do{                          \
			charger.Red_MoveX = charger.Dock_MoveX ,\
			charger.Red_MoveZ = -charger.Dock_MoveZ ;\
}while(0)

#define back_right do{                          \
			charger.Red_MoveX = charger.Dock_MoveX ,\
			charger.Red_MoveZ = charger.Dock_MoveZ ;\
}while(0)

#define back do{                          \
			charger.Red_MoveX = charger.Dock_MoveX ,\
			charger.Red_MoveZ = 0 ;\
}while(0)

#define front do{                          \
			charger.Red_MoveX = -charger.Dock_MoveX ,\
			charger.Red_MoveZ = 0 ;\
}while(0)

#define stop do{                          \
			charger.Red_MoveX = 0 ,\
			charger.Red_MoveZ = 0 ;\
}while(0)
#endif //#if defined AKM_CAR


#endif
