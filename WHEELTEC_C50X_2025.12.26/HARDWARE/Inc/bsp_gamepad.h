#ifndef __BSP_GAMEPAD_H
#define __BSP_GAMEPAD_H

#include <stdint.h>

//游戏手柄按键通用事件
typedef enum{
    GamePadKeyEvent_NONE,         //无按键事件
    GamePadKeyEvent_SINGLECLICK,  //单击事件
    GamePadKeyEvent_DOUBLECLICK,  //双击事件
    GamePadKeyEvent_LONGCLICK     //长按事件
}GamePadKeyEventType_t;

//游戏手柄通用按键状态
typedef enum{
	GamePadKeyState_Release = 0,  //松开
	GamePadKeyState_Press = 1     //按下
}GamePadKeyStateType_t;

//通用手柄按键检测状态机
typedef enum{
    WaitToPress = 0,
    WaitToRelease ,
    KEYPress    ,
    KEYUp       ,
    LONG_CLICK  ,
}GamePad_StateMachineType_t;

//通用手柄单个按键属性
typedef struct 
{
    uint8_t keystate;
    uint32_t timebase;
    uint32_t statetime;
    GamePad_StateMachineType_t statemachine;
}GamePad_CheckEventType_t;

//游戏手柄按键通用检测时间,单位ms
#define GamePad_LONGPRESS_TIEM 1000 //长按检测时间
#define GamePad_CLICK_TIME     400  //单双击检测时间
#define GamePad_KEYFILTER_TIME 50   //按键滤波时间

//通用游戏手柄对象
typedef struct{
	uint8_t LX;  //4个方向摇杆值,取值0~255
	uint8_t LY;
	uint8_t RX;
	uint8_t RY;
	uint8_t LT;  //左右两个油门扳机
	uint8_t RT;
	void (*SetVibration)(uint8_t m1,uint8_t m2);//震动接口
	GamePadKeyStateType_t (*getKeyState)(uint8_t keybit);
	uint8_t StartFlag;
	uint8_t Vib_EN;
}GamePadType_t;


//游戏手柄品牌
typedef enum{
	UnKnown_Dev,          //未知设备
	PS2_Classic,           //经典PS2手柄
	PS2_USB_Wired,         //有线ps2手柄
	PS2_USB_WiredV2,       //第二代有线ps2
	PS2_USB_Wiredless,    //无线ps2手柄
	Xbox360,    //xbox360标识符
	SwitchPro,
}GamePadBrandType_t;

//手柄调试结构体
enum{
	EnumNULL = 0,
	EnumWait ,
	EnumDone ,
};
typedef struct{
	uint8_t ready;
	uint8_t enmu_state;
	GamePadBrandType_t type;
}GamePadDebugValType_t;

//对游戏手柄提供按键检测的接口函数
GamePadKeyEventType_t GamePadKey_CheckEvent(uint16_t keysource,GamePad_CheckEventType_t* key,uint8_t bit);

//对外提供游戏手柄品牌参数
extern GamePadBrandType_t gamepad_brand;

//手柄设备插入与拔出回调函数
void USB_GamePad_InsertCallback(void);
void USB_GamePad_PullOutCallback(void);

//通用游戏手柄接口
extern GamePadType_t* GamePadInterface;
extern GamePadType_t GamePadDefalut;

//通用手柄调试接口
extern GamePadDebugValType_t GamePadDebug;

#endif /* __BSP_GAMEPAD_H */
