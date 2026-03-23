#ifndef __PS2_GAMEPAD_H
#define __PS2_GAMEPAD_H

#include <stdint.h>

#include "bsp_gamepad.h"

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

//数据解码函数
void PS2_Wired_Decode(const uint8_t *data,uint8_t datalen);
void PS2_Wiredless_Android_Decode(const uint8_t *data,uint8_t datalen);
void PS2_Wiredless_PC_Decode(const uint8_t *data,uint8_t datalen);

//对外提供的接口
extern GamePadType_t ps2_gamepad;

#endif /* __PS2_GAMEPAD_H */

