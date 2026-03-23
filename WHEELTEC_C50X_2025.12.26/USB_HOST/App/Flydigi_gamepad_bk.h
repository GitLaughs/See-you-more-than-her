#ifndef __FLYDIGI_GAMEPAD_H
#define __FLYDIGI_GAMEPAD_H

#include <stdint.h>

#include "usbh_hid.h"
#include "usbh_hid_GamePad.h"
#include "bsp_gamepad.h"

//手柄产品ID以及序列号
#define FLYDIGI_Manufacturer "Flydigi"
#define FLYDIGI_SerialNum "5E904C0C"

//手柄产品PID、VID
#define FLYDIGI_GamePad_VID 0x045E
#define FLYDIGI_GamePad_PID 0x028E

//手柄的数据长度信息
#define FLYDIGI_GamePad_DataLen 64

//15个按键组合
enum{
	FlydigiKEY_UP = 0, //转轴十字-上
	FlydigiKEY_DOWN,   //转轴十字-下
	FlydigiKEY_LEFT,   //转轴十字-左
	FlydigiKEY_RIGHT,  //转轴十字-右
	FlydigiKEY_Menu,   // 菜单/star按键
	FlydigiKEY_SELECT, // 窗口/select按键
	FlydigiKEY_LJoy,   //左摇杆按键
	FlydigiKEY_RJoy,   //右摇杆按键
	FlydigiKEY_LB ,    //左上按键LB
	FlydigiKEY_RB ,    //右上按键RB
	FlydigiKEY_HOME ,  //logo按键Home
	Flydigi_PaddingBit,//填充位,为了对齐按键功能使用,实际该位无意义
	FlydigiKEY_A ,     //按键A
	FlydigiKEY_B ,     //按键B
	FlydigiKEY_X ,     //按键X
	FlydigiKEY_Y ,     //按键Y
	//扳机按键预留
};

//手柄解码函数
void Flydigi_gamepad_Decode(USBH_HandleTypeDef *phost,uint8_t* buffer,uint8_t datalen);

//对外提供手柄对象
extern GamePadType_t flydigiGamepad;

#endif /* __FLYDIGI_GAMEPAD_H */
