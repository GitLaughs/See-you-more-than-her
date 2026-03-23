#ifndef __WiredPS2_GAMEPAD_H
#define __WiredPS2_GAMEPAD_H

#include <stdint.h>

#include "usbh_hid.h"
#include "usbh_hid_GamePad.h"
#include "bsp_gamepad.h"

//有线手柄产品标识符
#define Wired_PS2_VID 0x0810
#define Wired_PS2_PID 0x0001

//第二代有线
#define WiredV2_PS2_VID 0x0079
#define WiredV2_PS2_PID 0x0006

//PS2按键位置枚举(bit0~bit15分别为下面的0~15)
enum 
{
	PS2KEY_SELECT	   = 0, //选择按键
	PS2KEY_LROCKER      , //左右摇杆按下键值
	PS2KEY_RROCKER      ,
	PS2KEY_START        , //开始按键
	PS2KEY_UP           , //左按键区域
	PS2KEY_RIGHT        ,
	PS2KEY_DOWN         ,
	PS2KEY_LEFT         ,
	PS2KEY_L2           ,	//左右扳机按键值
	PS2KEY_R2           ,
	PS2KEY_L1           ,  
	PS2KEY_R1           ,
	PS2KEY_1GREEN       , //右按键区域
	PS2KEY_2RED         , 
	PS2KEY_3BLUE        , 
	PS2KEY_4PINK           
};


//手柄解码函数
void Wired_USB_PS2gamepad_Decode(USBH_HandleTypeDef *phost,uint8_t* buffer,uint8_t datalen);
void Wired_USB_V2_PS2gamepad_Decode(USBH_HandleTypeDef *phost,uint8_t* buffer,uint8_t datalen);
//对外提供手柄对象
extern GamePadType_t Wired_USB_PS2Gamepad;

#endif /* __Xbox360_GAMEPAD_H */
