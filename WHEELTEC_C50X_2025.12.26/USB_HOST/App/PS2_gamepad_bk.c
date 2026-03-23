#include "PS2_gamepad.h"
/*
 * PS2游戏手柄协议解析
*/

//PS2手柄按键回调函数
void PS2GamePad_OnSingleClick(uint8_t key_id)
{
	
}

void PS2GamePad_OnDoubleClick(uint8_t key_id)
{
	
}

void PS2GamePad_OnLongClick(uint8_t key_id)
{
	
}


//PS2 16个按键数值读取,内部使用
static uint16_t ps2_KeyVal = 0;

//16个ps2按键
#define PS2_KEY_NUM 16 

//按键检测变量值,内部使用
static GamePad_CheckEventType_t PS2_GamePadKeyCheckEvent[PS2_KEY_NUM] = { 0 };

//直接反馈按键的状态值
static GamePadKeyStateType_t PS2_GetKeyState(uint8_t bit)
{
	if( (ps2_KeyVal>>bit)&0x01 )
		return GamePadKeyState_Press;
	else
		return GamePadKeyState_Release;
}

//标志位设置函数,用于辅助ps2手柄解码
static void ps2_set_bit(uint16_t* state,uint8_t state_bit,uint8_t bit)
{
	if(state_bit==1) //指定的位(bit)设置为1,其他位不变
	{
		*state |= (1U<<bit);
	}
	else //指定的位(bit)设置为0,其他位不变
	{
		*state &= ~(1U<<bit);
	}
}

//游戏手柄对象
GamePadType_t ps2_gamepad = { 
	.LX = 127,
	.LY = 127,
	.RX = 127,
	.RY = 127,
	.LT = 0,
	.RT = 0,
	.StartFlag = 0,
	.Vib_EN = 1,
	.SetVibration = 0,
	.getKeyState = PS2_GetKeyState
};

//有线PS2手柄的数据解码
void PS2_Wired_Decode(const uint8_t *data,uint8_t datalen)
{
	uint8_t tmp_bool = 0 ;
	
	ps2_gamepad.LX = data[3];
	ps2_gamepad.LY = data[4];
	ps2_gamepad.RX = data[1];
	ps2_gamepad.RY = data[2];
	
	tmp_bool = (data[6]>>4)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,0); //seltec key 选择按键
	
	tmp_bool = (data[6]>>6)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,1); //左摇杆按键
	
	tmp_bool = (data[6]>>7)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,2); //右摇杆按键
	
	tmp_bool = (data[6]>>5)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,3); //start
	
	tmp_bool = data[5]&0x0F;//取出低4位
	if(tmp_bool==0x0F)//没有任何按键按下
	{
		ps2_set_bit(&ps2_KeyVal,0,4); //↑
		ps2_set_bit(&ps2_KeyVal,0,5); //→
		ps2_set_bit(&ps2_KeyVal,0,6); //↓
		ps2_set_bit(&ps2_KeyVal,0,7); //←
	}
	else if( (tmp_bool&0x01)==0 )
	{	
		switch ((tmp_bool>>1)&0x03)
		{
			case 0x00://↑
				ps2_set_bit(&ps2_KeyVal,1,4); //↑
				break;
			case 0x01://→
				ps2_set_bit(&ps2_KeyVal,1,5); //→
				break;
			case 0x02://↓
				ps2_set_bit(&ps2_KeyVal,1,6); //↓
				break;
			case 0x03://←
				ps2_set_bit(&ps2_KeyVal,1,7); //←
				break;
			default:
				break;
		}
	}
	else if( (tmp_bool&0x01)==1 ) //首位为1,代表存在左盘2个按键按下的情况
	{
		switch ((tmp_bool>>1)&0x03)
		{
			case 0x00://↑→
				ps2_set_bit(&ps2_KeyVal,1,4);//↑
				ps2_set_bit(&ps2_KeyVal,1,5); //→
				break;
			case 0x01://↓→
				ps2_set_bit(&ps2_KeyVal,1,6); //↓
				ps2_set_bit(&ps2_KeyVal,1,5); //→
				break;
			case 0x02://↓←
				ps2_set_bit(&ps2_KeyVal,1,6); //↓
				ps2_set_bit(&ps2_KeyVal,1,7); //←
				break;
			case 0x03://↑←
				ps2_set_bit(&ps2_KeyVal,1,4); //↑
				ps2_set_bit(&ps2_KeyVal,1,7); //←
				break;
			default:
				break;
		}
	}
	
	tmp_bool = (data[6]>>2)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,8); //左扳机2号
	if( tmp_bool ) ps2_gamepad.LT = 255;
	else ps2_gamepad.LT = 0;
	
	tmp_bool = (data[6]>>3)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,9); //右扳机2号
	if( tmp_bool ) ps2_gamepad.RT = 255;
	else ps2_gamepad.RT = 0;
	
	tmp_bool = (data[6]>>0)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,10); //左扳机1号
	
	tmp_bool = (data[6]>>1)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,11); //右扳机1号

	tmp_bool = (data[5]>>4)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,12); //一号,绿色GREEN
	
	tmp_bool = (data[5]>>5)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,13); //二号,红色RED

	tmp_bool = (data[5]>>6)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,14); //三号,蓝牙BLUE
	
	tmp_bool = (data[5]>>7)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,15); //四号,粉色PINK
	
	//按键回调函数触发
	for (uint8_t key = PS2KEY_SELECT; key <= PS2KEY_4PINK; key++) 
	{
		GamePadKeyEventType_t event = GamePadKey_CheckEvent(ps2_KeyVal,
                                		&PS2_GamePadKeyCheckEvent[key], key);
		
		if (event == GamePadKeyEvent_NONE) continue;

		switch (event) {
			case GamePadKeyEvent_SINGLECLICK: PS2GamePad_OnSingleClick(key); break;
			case GamePadKeyEvent_DOUBLECLICK: PS2GamePad_OnDoubleClick(key); break;
			case GamePadKeyEvent_LONGCLICK: PS2GamePad_OnLongClick(key); break;
		}
	}
}

//无线安卓模式手柄数据解码
void PS2_Wiredless_Android_Decode(const uint8_t *data,uint8_t datalen)
{
	uint8_t tmp_bool = 0 ;
	
	uint8_t rm_val = 0;
	if( data[6]==0&&data[7]==0 ) rm_val=128;
	else rm_val = data[6];
	ps2_gamepad.LX = rm_val;
	
	if( data[8]==0&&data[9]==0  ) rm_val=128;
	else rm_val = data[8];
	ps2_gamepad.LY = 255 - rm_val;
	
	if( data[10]==0&&data[11]==0  ) rm_val=128;
	else rm_val = data[10];
	ps2_gamepad.RX = rm_val;
	
	if( data[12]==0&&data[13]==0  ) rm_val=128;
	else rm_val = data[12];
	ps2_gamepad.RY = 255 - rm_val;
	
	ps2_gamepad.LT = data[4];
	ps2_gamepad.RT = data[5];
	
	//data[2]
	//Rm    Lm    select   start    →      ←       ↓        ↑
	//0		0		0		0		0		0		0		0
	tmp_bool = (data[2]>>0)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,4); //↑
	
	tmp_bool = (data[2]>>3)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,5); //→
	
	tmp_bool = (data[2]>>1)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,6); //↓
	
	tmp_bool = (data[2]>>2)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,7); //←
	
	tmp_bool = (data[2]>>5)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,0); //seltec key 选择按键	
	
	tmp_bool = (data[2]>>4)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,3); //start key 选择按键
	
	tmp_bool = (data[2]>>6)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,1); //左摇杆按键	
	
	tmp_bool = (data[2]>>7)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,2); //右摇杆按键
	
	tmp_bool = (data[3]>>0)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,10); //左扳机1号
	
	tmp_bool = (data[3]>>1)&0x01;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,11); //右扳机1号
	
	if(data[4]==0xff) tmp_bool=1;
	else tmp_bool=0;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,8); //左扳机2号
	
	if(data[5]==0xff) tmp_bool=1;
	else tmp_bool=0;
	ps2_set_bit(&ps2_KeyVal,tmp_bool,9); //右扳机2号
	
	tmp_bool = (data[3]>>4)&0x01;//BLUE
	ps2_set_bit(&ps2_KeyVal,tmp_bool,14);
	
	tmp_bool = (data[3]>>5)&0x01;//RED
	ps2_set_bit(&ps2_KeyVal,tmp_bool,13);
	
	tmp_bool = (data[3]>>6)&0x01;//PINK
	ps2_set_bit(&ps2_KeyVal,tmp_bool,15);
	
	tmp_bool = (data[3]>>7)&0x01;//GREEN
	ps2_set_bit(&ps2_KeyVal,tmp_bool,12);
	
	//按键回调函数触发
	for (uint8_t key = PS2KEY_SELECT; key <= PS2KEY_4PINK; key++) 
	{
		GamePadKeyEventType_t event = GamePadKey_CheckEvent(ps2_KeyVal,
                                		&PS2_GamePadKeyCheckEvent[key], key);
		
		if (event == GamePadKeyEvent_NONE) continue;

		switch (event) {
			case GamePadKeyEvent_SINGLECLICK: PS2GamePad_OnSingleClick(key); break;
			case GamePadKeyEvent_DOUBLECLICK: PS2GamePad_OnDoubleClick(key); break;
			case GamePadKeyEvent_LONGCLICK: PS2GamePad_OnLongClick(key); break;
		}
	}
}

//无线pc模式手柄数据解码
void PS2_Wiredless_PC_Decode(const uint8_t *data,uint8_t datalen)
{
	uint8_t tmp_bool = 0;
	
	ps2_gamepad.LX = data[3];
	ps2_gamepad.LY = data[4];
	ps2_gamepad.RX = data[5];
	ps2_gamepad.RY = data[6];
	
	tmp_bool = (data[1]>>0)&0x01;//select
	ps2_set_bit(&ps2_KeyVal,tmp_bool,0); //seltec key 选择按键	
	
	tmp_bool = (data[1]>>1)&0x01;//start
	ps2_set_bit(&ps2_KeyVal,tmp_bool,3); //start key 选择按键
	
	tmp_bool = (data[1]>>2)&0x01;//Lm
	ps2_set_bit(&ps2_KeyVal,tmp_bool,1); //左摇杆按键	
	
	tmp_bool = (data[1]>>3)&0x01;//Rm
	ps2_set_bit(&ps2_KeyVal,tmp_bool,2); //右摇杆按键
	
	tmp_bool = (data[0]>>4)&0x01;//L1
	ps2_set_bit(&ps2_KeyVal,tmp_bool,10); //左扳机1号
	
	tmp_bool = (data[0]>>5)&0x01;//R1
	ps2_set_bit(&ps2_KeyVal,tmp_bool,11); //右扳机1号
	
	tmp_bool = (data[0]>>6)&0x01;//L2
	ps2_set_bit(&ps2_KeyVal,tmp_bool,8); //左扳机2号
	if( tmp_bool ) ps2_gamepad.LT = 255;
	else ps2_gamepad.LT = 0;
	
	tmp_bool = (data[0]>>7)&0x01;//R2
	ps2_set_bit(&ps2_KeyVal,tmp_bool,9); //左扳机2号
	if( tmp_bool ) ps2_gamepad.RT = 255;
	else ps2_gamepad.RT = 0;
	
	tmp_bool = (data[0]>>0)&0x01;//GREEN
	ps2_set_bit(&ps2_KeyVal,tmp_bool,12);
	
	tmp_bool = (data[0]>>1)&0x01;//RED
	ps2_set_bit(&ps2_KeyVal,tmp_bool,13);
	
	tmp_bool = (data[0]>>2)&0x01;//BLUE
	ps2_set_bit(&ps2_KeyVal,tmp_bool,14);
	
	tmp_bool = (data[0]>>3)&0x01;//PINK
	ps2_set_bit(&ps2_KeyVal,tmp_bool,15);
	
	tmp_bool = data[2]&0x0F;//取出低4位
	if(tmp_bool==0x0F)//没有任何按键按下
	{
		ps2_set_bit(&ps2_KeyVal,0,4); //↑
		ps2_set_bit(&ps2_KeyVal,0,5); //→
		ps2_set_bit(&ps2_KeyVal,0,6); //↓
		ps2_set_bit(&ps2_KeyVal,0,7); //←
	}
	else if( (tmp_bool&0x01)==0 )
	{	
		switch ((tmp_bool>>1)&0x03)
		{
			case 0x00://↑
				ps2_set_bit(&ps2_KeyVal,1,4); //↑
				break;
			case 0x01://→
				ps2_set_bit(&ps2_KeyVal,1,5); //→
				break;
			case 0x02://↓
				ps2_set_bit(&ps2_KeyVal,1,6); //↓
				break;
			case 0x03://←
				ps2_set_bit(&ps2_KeyVal,1,7); //←
				break;
			default:
				break;
		}
	}
	else if( (tmp_bool&0x01)==1 ) //首位为1,代表存在左盘2个按键按下的情况
	{
		switch ((tmp_bool>>1)&0x03)
		{
			case 0x00://↑→
				ps2_set_bit(&ps2_KeyVal,1,4);//↑
				ps2_set_bit(&ps2_KeyVal,1,5); //→
				break;
			case 0x01://↓→
				ps2_set_bit(&ps2_KeyVal,1,6); //↓
				ps2_set_bit(&ps2_KeyVal,1,5); //→
				break;
			case 0x02://↓←
				ps2_set_bit(&ps2_KeyVal,1,6); //↓
				ps2_set_bit(&ps2_KeyVal,1,7); //←
				break;
			case 0x03://↑←
				ps2_set_bit(&ps2_KeyVal,1,4); //↑
				ps2_set_bit(&ps2_KeyVal,1,7); //←
				break;
			default:
				break;
		}
	}

	//按键回调函数触发
	for (uint8_t key = PS2KEY_SELECT; key <= PS2KEY_4PINK; key++) 
	{
		GamePadKeyEventType_t event = GamePadKey_CheckEvent(ps2_KeyVal,
                                		&PS2_GamePadKeyCheckEvent[key], key);
		
		if (event == GamePadKeyEvent_NONE) continue;

		switch (event) {
			case GamePadKeyEvent_SINGLECLICK: PS2GamePad_OnSingleClick(key); break;
			case GamePadKeyEvent_DOUBLECLICK: PS2GamePad_OnDoubleClick(key); break;
			case GamePadKeyEvent_LONGCLICK: PS2GamePad_OnLongClick(key); break;
		}
	}	
}

