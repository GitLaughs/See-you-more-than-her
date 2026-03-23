#include "Flydigi_gamepad.h"
#include "FreeRTOS.h"
#include "task.h"

#include "system.h"

//内部函数声明
static void Flydigi_GamePad_SetVibration(uint8_t m1,uint8_t m2);
static GamePadKeyStateType_t Flydigi_GetKeyState(uint8_t bit);

//飞智沙漠狐游戏手柄对象
GamePadType_t flydigiGamepad = {
	.LX = 127,
	.LY = 127,
	.RX = 127,
	.RY = 127,
	.LT = 0,
	.RT = 0,
	.StartFlag = 0,
	.Vib_EN = 1,
	.SetVibration = Flydigi_GamePad_SetVibration,
	.getKeyState = Flydigi_GetKeyState
};


//按键回调函数
__weak void FlydigiGamePad_KeyEvent_Callback(uint8_t keyid,GamePadKeyEventType_t event)
{
//	printf("keyid:%d\tevent:%d\r\n",keyid,event);
//	
//	if( keyid == FlydigiKEY_SELECT && event == GamePadKeyEvent_LONGCLICK )
//		flydigiGamepad.SetVibration(255,0);
}

//按键值
static uint16_t Flydigi_GamePad_KeyOriginalVal = 0;

//震动数值
volatile static uint8_t VibrationVal[2] = {0,0};

//震动时间
volatile static uint32_t VibrationTimes = 0;

//启动震动标志位
volatile static uint8_t vibrationFlag = 0;

//获取手柄键值状态函数
static GamePadKeyStateType_t Flydigi_GetKeyState(uint8_t bit)
{
	if( (Flydigi_GamePad_KeyOriginalVal>>bit)&0x01 )
		return GamePadKeyState_Press;
	else
		return GamePadKeyState_Release;
}

//设置手柄震动接口
static void Flydigi_GamePad_SetVibration(uint8_t m1,uint8_t m2)
{
	if( flydigiGamepad.Vib_EN == 0 ) return;
		
	vibrationFlag = 1;
	VibrationVal[0] = m1;
	VibrationVal[1] = m2;
	
	//更新震动时间
	VibrationTimes = xTaskGetTickCount();
}


//摇杆值映射
static uint8_t map_joystick(uint8_t joystick) {
    if (joystick == 0) {
        return 127;
    } else if (joystick <= 127) {
        return 127 + joystick;
    } else {
        return joystick - 128;
    }
}

//16个手柄按键
#define Flydigi_KEY_NUM 16 

//定义按键检测事件的各个按键值
static GamePad_CheckEventType_t Flydigi_GamePadKeyCheckEvent[Flydigi_KEY_NUM] = { 0 };

//对外提供一个震动接口
static void FlydigiGamePad_Vibration(USBH_HandleTypeDef *phost)
{
	if( vibrationFlag==0 ) return;
	
	static uint8_t lastvib[2] = { 0 };
	
	//生成震动数据
	uint8_t vibration_data[8] = {0x00, 0x08, 0x00, VibrationVal[0], VibrationVal[1], 0x00, 0x00, 0x00};
	
	//停止震动数据固定值
	uint8_t stop_data[8] = {0x00, 0x08, 0x00, 0, 0, 0x00, 0x00, 0x00};
	
    HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;
	
	//开启震动
	USBH_InterruptSendData(phost, vibration_data, 8, HID_Handle->OutPipe);
	
	//单次震动持续300ms,除非任务一直设置震动
	if( xTaskGetTickCount() - VibrationTimes > 200 )
	{
		vibrationFlag = 0;
		vibration_data[3] = 0,vibration_data[4] = 0;
		for(uint8_t i=0;i<3;i++)
		{
			USBH_InterruptSendData(phost, stop_data, 8, HID_Handle->OutPipe);
			vTaskDelay(20);
		}
		lastvib[0]=0;
		lastvib[1]=0;
	}
	else
	{
		//数据改变时才刷新
		if( lastvib[0]!=VibrationVal[0] || lastvib[1]!=VibrationVal[1]  )
		{
			USBH_InterruptSendData(phost, vibration_data, 8, HID_Handle->OutPipe);
			vTaskDelay(20);
		}
		lastvib[0] = VibrationVal[0];
		lastvib[1] = VibrationVal[1];
	}
		
}

//飞智沙漠狐游戏手柄数据解码
void Flydigi_gamepad_Decode(USBH_HandleTypeDef *phost,uint8_t* buffer,uint8_t datalen)
{
	HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;
	
//	if( datalen!=FLYDIGI_GamePad_DataLen ) return;
	
	//启动时发送停止震动数据,避免在设置震动的时候刚好被复位或者其他意外,导致手柄一直震动
	static uint8_t times = 0;
	if( times<2 )
	{
		times++;
		uint8_t vibration_data[8] = {0x00, 0x08, 0x00, 0, 0, 0x00, 0x00, 0x00};
		USBH_InterruptSendData(phost, vibration_data, 8, HID_Handle->OutPipe);
	}
	
	//模拟量行程
	flydigiGamepad.LX = map_joystick(buffer[7]);
	flydigiGamepad.LY = map_joystick(buffer[9]);
	flydigiGamepad.RX = map_joystick(buffer[11]);
	flydigiGamepad.RY = map_joystick(buffer[13]);
	flydigiGamepad.LT = buffer[4];
	flydigiGamepad.RT = buffer[5];
	
	/* 摇杆值范围
	      Y
	     254
	      ^
	      |
	0 <---------> 254 X
	      |
	      v
	      0
	*/
	
	//15个按键值
	Flydigi_GamePad_KeyOriginalVal = (uint16_t)buffer[3]<<8 | buffer[2];
	
	//按键回调函数触发
	for (uint8_t key = FlydigiKEY_UP; key <= FlydigiKEY_Y; key++) 
	{
		if (key == Flydigi_PaddingBit) continue;
		GamePadKeyEventType_t event = GamePadKey_CheckEvent(Flydigi_GamePad_KeyOriginalVal,
                                		&Flydigi_GamePadKeyCheckEvent[key], key);
		
		if (event == GamePadKeyEvent_NONE) continue;
		
		//触发回调函数
		FlydigiGamePad_KeyEvent_Callback(key,event);
	}
	
	//设置震动函数
	FlydigiGamePad_Vibration(phost);
}

