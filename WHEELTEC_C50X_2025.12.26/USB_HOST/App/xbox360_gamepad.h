#ifndef __XBOX360_GAMEPAD_H
#define __XBOX360_GAMEPAD_H

#include <stdint.h>

#include "usbh_hid.h"
#include "usbh_hid_GamePad.h"
#include "bsp_gamepad.h"

//手柄产品ID以及序列号
#define Xbox360_Manufacturer "Xbox360"
#define Xbox360_SerialNum "5E904C0C"

//手柄产品PID、VID
#define Xbox360_GamePad_VID 0x045E
#define Xbox360_GamePad_PID 0x028E

//手柄的数据长度信息
#define Xbox360_GamePad_DataLen 64

//15个按键组合
enum{
	Xbox360KEY_UP = 0, //转轴十字-上
	Xbox360KEY_DOWN,   //转轴十字-下
	Xbox360KEY_LEFT,   //转轴十字-左
	Xbox360KEY_RIGHT,  //转轴十字-右
	Xbox360KEY_Menu,   // 菜单/star按键
	Xbox360KEY_SELECT, // 窗口/select按键
	Xbox360KEY_LJoy,   //左摇杆按键
	Xbox360KEY_RJoy,   //右摇杆按键
	Xbox360KEY_LB ,    //左上按键LB
	Xbox360KEY_RB ,    //右上按键RB
	Xbox360KEY_HOME ,  //logo按键Home
	Xbox360_PaddingBit,//填充位,为了对齐按键功能使用,实际该位无意义
	Xbox360KEY_A ,     //按键A
	Xbox360KEY_B ,     //按键B
	Xbox360KEY_X ,     //按键X
	Xbox360KEY_Y ,     //按键Y
	//扳机按键预留
};

//手柄解码函数
void Xbox360_gamepad_Decode(USBH_HandleTypeDef *phost,uint8_t* buffer,uint8_t datalen);

//对外提供手柄对象
extern GamePadType_t Xbox360Gamepad;

#endif /* __Xbox360_GAMEPAD_H */
