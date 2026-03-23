#include "pstwo.h"
#include "delay.h"

//引入ps2手柄枚举
#include "WiredPS2_gamepad.h"

#define DELAY_TIME  delay_us(5);

static GamePadKeyStateType_t ClassicPS2_GetKeyState(uint8_t bit);

//经典PS2手柄
GamePadType_t Classic_PS2Gamepad = {
	.LX = 127,
	.LY = 127,
	.RX = 127,
	.RY = 127,
	.LT = 0,
	.RT = 0,
	.StartFlag = 0,
	.Vib_EN = 0,
	.SetVibration = NULL,
	.getKeyState = ClassicPS2_GetKeyState
};


//
__weak void Classic_PS2GamePad_KeyEvent_Callback(uint8_t keyid,GamePadKeyEventType_t event)
{
	printf("%d,%d\r\n",keyid,event);
}


//Button value reading, zero time storage
//按键值读取，零时存储
u16 Handkey;
//Start the order. Request data
//开始命令。请求数据
u8 Comd[2]= {0x01,0x42};
//Data store array
//数据存储数组
u8 Data[9]= {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

//摇杆模拟量数据
#define Rocker_RX 5                
#define Rocker_RY 6
#define Rocker_LX 7
#define Rocker_LY 8

//内部使用函数
static void PS2_Cmd(u8 CMD);

//static u8 PS2_RedLight(void);
//static void PS2_Vibration(u8 motor1, u8 motor2);
//static void PS2_VibrationMode(void);

static void PS2_ReadData(void);
static u16 PS2_DataKey(void);
static u8 PS2_AnologData(u8 button);
static void PS2_ClearData(void);
static void PS2_ShortPoll(void);
static void PS2_EnterConfing(void);
static void PS2_TurnOnAnalogMode(void);
static void PS2_ExitConfing(void);
static void PS2_SetInit(void);

//获取手柄键值状态函数
static GamePadKeyStateType_t ClassicPS2_GetKeyState(uint8_t bit)
{
	if( (Handkey>>bit)&0x01 )
		return GamePadKeyState_Press;
	else
		return GamePadKeyState_Release;
}


//定义按键检测事件的各个按键值
static GamePad_CheckEventType_t GamePadKeyCheckEvent[16] = { 0 };

void PS2_Key_Param_Init(void)
{
	gamepad_brand = PS2_USB_Wiredless;
	GamePadInterface = &Classic_PS2Gamepad;
}

/**************************************************************************
Function: Ps2 handle initializer
Input   : none
Output  : none
函数功能：PS2手柄初始化
入口参数：无
返回  值：无
**************************************************************************/
void PS2_Init(void)
{
	//引脚初始化
    GPIO_InitTypeDef GPIO_InitStructure;
	
	//对应引脚时钟开启
	ENABLE_PS2_DI_PIN_CLOCK;
	ENABLE_PS2_DO_PIN_CLOCK;
	ENABLE_PS2_CS_PIN_CLOCK;
	ENABLE_PS2_CLK_PIN_CLOCK;
	
	//引脚模式设置
    GPIO_InitStructure.GPIO_Pin = PS2_DI_PIN;			//端口配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;		//普通输入模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;	//100M
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;		//下拉（数据的读取引脚配置为下拉,当ps2被拔出时,读取的信号全为0;若配置为上拉则全为1）
    GPIO_Init(PS2_DI_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PS2_DO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;   //输出
    GPIO_InitStructure.GPIO_OType =GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed =GPIO_Speed_2MHz;  //2M
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//根据设定参数初始化GPIO
    GPIO_Init(PS2_DO_PORT, &GPIO_InitStructure);
	
    GPIO_InitStructure.GPIO_Pin = PS2_CS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;   //输出
    GPIO_InitStructure.GPIO_OType =GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed =GPIO_Speed_2MHz;  //2M
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//根据设定参数初始化GPIO
    GPIO_Init(PS2_CS_PORT, &GPIO_InitStructure);
	
    GPIO_InitStructure.GPIO_Pin = PS2_CLK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;   //输出
    GPIO_InitStructure.GPIO_OType =GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed =GPIO_Speed_2MHz;  //2M
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//根据设定参数初始化GPIO
    GPIO_Init(PS2_CLK_PORT, &GPIO_InitStructure);
	
	//PS2内部参数初始化
	PS2_SetInit();
}


/**************************************************************************
Function: Read the control of the ps2 handle
Input   : none
Output  : none
函数功能：读取PS2手柄的控制量(对外使用函数)
入口参数：无
返回  值：无
**************************************************************************/
void PS2_Read(void)
{
    //Reading key
    //读取按键键值
	PS2_DataKey();
    //Read the analog of the remote sensing x axis on the left
    //读取左边遥感X轴方向的模拟量
    Classic_PS2Gamepad.LX = PS2_AnologData(Rocker_LX);
    //Read the analog of the directional direction of remote sensing on the left
    //读取左边遥感Y轴方向的模拟量
    Classic_PS2Gamepad.LY= 255 - PS2_AnologData(Rocker_LY);
    //Read the analog of the remote sensing x axis
    //读取右边遥感X轴方向的模拟量
    Classic_PS2Gamepad.RX=PS2_AnologData(Rocker_RX);
    //Read the analog of the directional direction of the remote sensing y axis
    //读取右边遥感Y轴方向的模拟量
    Classic_PS2Gamepad.RY=PS2_AnologData(Rocker_RY);
	
	//LT、RT赋值
	if( ClassicPS2_GetKeyState(PS2KEY_L2)!=0 ) Classic_PS2Gamepad.LT = 255;
	else  Classic_PS2Gamepad.LT = 0;
	
	if( ClassicPS2_GetKeyState(PS2KEY_R2)!=0 ) Classic_PS2Gamepad.RT = 255;
	else  Classic_PS2Gamepad.RT = 0;
	
	//按键回调函数触发
	for (uint8_t key = PS2KEY_SELECT; key <= PS2KEY_4PINK; key++) 
	{
		GamePadKeyEventType_t event = GamePadKey_CheckEvent(Handkey,
                                		&GamePadKeyCheckEvent[key], key);
		
		if (event == GamePadKeyEvent_NONE) continue;
		
		//触发回调函数
		Classic_PS2GamePad_KeyEvent_Callback(key,event);
	}
}
/**************************************************************************
Function: Send commands to the handle
Input   : none
Output  : none
函数功能：向手柄发送命令
入口参数：无
返回  值：无
**************************************************************************/
static void PS2_Cmd(u8 CMD)
{
    volatile u16 ref=0x01;
    Data[1] = 0;
    for(ref=0x01; ref<0x0100; ref<<=1)
    {
        if(ref&CMD)
        {
            DO_H;     //Output a control bit //输出一位控制位
        }
        else DO_L;
        delay_us(16);
        CLK_H;      //Clock lift //时钟拉高
        DELAY_TIME;
        CLK_L;
        DELAY_TIME;
        CLK_H;
        delay_us(16);
        if(DI)
            Data[1] = ref|Data[1];
        delay_us(16);
    }
    delay_us(16);
}
/**************************************************************************
Function: Whether it is a red light mode, 0x41= analog green light, 0x73= analog red light
Input   : none
Output  : 0: red light mode, other: other modes
函数功能：判断是否为红灯模式,0x41=模拟绿灯，0x73=模拟红灯
入口参数：无
返回  值：0：红灯模式，其他：其他模式
**************************************************************************/
//static u8 PS2_RedLight(void)
//{
//    CS_L;
//    PS2_Cmd(Comd[0]);  //Start orders //开始命令
//    PS2_Cmd(Comd[1]);  //Request data //请求数据
//    CS_H;
//    if( Data[1] == 0X73)   return 0 ;
//    else return 1;

//}
/**************************************************************************
Function: Read the handle data
Input   : none
Output  : none
函数功能：读取手柄数据
入口参数：无
返回  值：无
**************************************************************************/
static void PS2_ReadData(void)
{
    volatile u8 byte=0;
    volatile u16 ref=0x01;
    CS_L;
    PS2_Cmd(Comd[0]);  //Start orders //开始命令
    PS2_Cmd(Comd[1]);  //Request data //请求数据
    for(byte=2; byte<9; byte++) //Start receiving data //开始接受数据
    {
        for(ref=0x01; ref<0x100; ref<<=1)
        {
            CLK_H;
            DELAY_TIME;
            CLK_L;
            DELAY_TIME;
            CLK_H;
            if(DI)
                Data[byte] = ref|Data[byte];
        }
        delay_us(16);
    }
    CS_H;
}
/**************************************************************************
Function: Handle the data of the read 2 and handle only the key parts
Input   : none
Output  : 0: only one button presses the next time; No press
函数功能：对读出来的PS2的数据进行处理,只处理按键部分
入口参数：无
返回  值：0: 只有一个按键按下时按下; 1: 未按下
**************************************************************************/
u8 ps2_default_buf[9] = {0,0x73,0,0xff,0xff,128,128,128,128};

static u16 PS2_DataKey(void)
{
	static u8 last_ps2_data[9] = {0,0,0,0xff,0xff,128,128,128,128};
	static u8 ps2_connect_timeout = 0;
	
	//读取PS2手柄数据
    PS2_ClearData();
    PS2_ReadData();
	
	//有线ps2数字模式(0x41)或无线ps2手柄数字模式(0x20),出现这个模式手柄出现了热插入
	//255为异常数据,无线手柄只插入了部分而没有完全插入可读到此数据
	if( Data[1]==0x41 || Data[1]==0x20 || Data[1]==255 )
	{
		u8 save = Data[1];
		PS2_SetInit();//初始化为模拟模式
		memcpy(Data,ps2_default_buf,9);//第一组数据使用模式配置,防止失控
		Data[1] = save;//将本次的值写入,防止初始化不成功导致跳过了该判断
	}
	
	//若出现所有数据都是0，则采用上一次的数据(全0或全1,跟引脚配置的初始化上下拉有关)
	u8 a=0;
	for(u8 i=0;i<9;i++)
	{
		a+=Data[i];
	}
	if(a==0) //数据全0说明手柄被拔出
	{
		//此状态下可当作ps2手柄未连接,到达一定次数后将清空ps2控制值
		ps2_connect_timeout++;
		if( ps2_connect_timeout> 10 ) 
		{   //手柄连接超时,使用默认变量停止小车控制
			ps2_connect_timeout = 11, memcpy(last_ps2_data,ps2_default_buf,9);
		}			
		
		//全0时使用上一次的数据(所有按键同时按下才为0)
		memcpy(Data,last_ps2_data,9);
	}
	else
	{
		ps2_connect_timeout = 0;//计时复位
	}
	
	//保存本次数据
	memcpy(last_ps2_data,Data,9); 
	
	//This is 16 buttons, pressed down to 0, and not pressed for 1 
	//这是16个按键，按下为0，未按下为1
    Handkey=(Data[4]<<8)|Data[3]; 
	
	//转化为 按下为1,未按下为0（适应usb ps2数据格式）
	Handkey = ~Handkey;
	
	//返回键值情况
	return Handkey;
}
/**************************************************************************
Function: Get a simulation of a rocker
Input   : Rocker
Output  : Simulation of rocker, range 0~ 256
函数功能：得到一个摇杆的模拟量
入口参数：摇杆
返回  值：摇杆的模拟量, 范围0~256
**************************************************************************/
static u8 PS2_AnologData(u8 button)
{
    return Data[button];
}
/**************************************************************************
Function: Clear data buffer
Input   : none
Output  : none
函数功能：清除数据缓冲区
入口参数：无
返回  值：无
**************************************************************************/
static void PS2_ClearData(void)
{
    u8 a;
    for(a=0; a<9; a++)
        Data[a]=0x00;
}
/******************************************************
Function: Handle vibration function
Input   : motor1: the right small vibrator, 0x00, other
          motor2: the left big shock motor 0x40~ 0xff motor is open, and the value is greater
Output  : none
函数功能：手柄震动函数
入口参数：motor1:右侧小震动电机 0x00关，其他开
	        motor2:左侧大震动电机 0x40~0xFF 电机开，值越大 震动越大
返回  值：无
******************************************************/
//static void PS2_Vibration(u8 motor1, u8 motor2)
//{
//    CS_L;
//    delay_us(16);
//    PS2_Cmd(0x01); //Start order //开始命令
//    PS2_Cmd(0x42); //Request data //请求数据
//    PS2_Cmd(0X00);
//    PS2_Cmd(motor1);
//    PS2_Cmd(motor2);
//    PS2_Cmd(0X00);
//    PS2_Cmd(0X00);
//    PS2_Cmd(0X00);
//    PS2_Cmd(0X00);
//    CS_H;
//    delay_us(16);
//}
/**************************************************************************
Function: Short press
Input   : none
Output  : none
函数功能：短按
入口参数：无
返回  值：无
**************************************************************************/
static void PS2_ShortPoll(void)
{
    CS_L;
    delay_us(16);
    PS2_Cmd(0x01);
    PS2_Cmd(0x42);
    PS2_Cmd(0X00);
    PS2_Cmd(0x00);
    PS2_Cmd(0x00);
    CS_H;
    delay_us(16);
}
/**************************************************************************
Function: Enter configuration
Input   : none
Output  : none
函数功能：进入配置
入口参数：无
返回  值：无
**************************************************************************/
static void PS2_EnterConfing(void)
{
    CS_L;
    delay_us(16);
    PS2_Cmd(0x01);
    PS2_Cmd(0x43);
    PS2_Cmd(0X00);
    PS2_Cmd(0x01);
    PS2_Cmd(0x00);
    PS2_Cmd(0X00);
    PS2_Cmd(0X00);
    PS2_Cmd(0X00);
    PS2_Cmd(0X00);
    CS_H;
    delay_us(16);
}
/**************************************************************************
Function: Send mode Settings
Input   : none
Output  : none
函数功能：发送模式设置
入口参数：无
返回  值：无
**************************************************************************/
static void PS2_TurnOnAnalogMode(void)
{
    CS_L;
    PS2_Cmd(0x01);
    PS2_Cmd(0x44);
    PS2_Cmd(0X00);
    PS2_Cmd(0x01); //analog=0x01;digital=0x00  Software Settings send mode 软件设置发送模式
    PS2_Cmd(0x03); //0x03 lock storage setup, which cannot be set by the key "mode" set mode. //0x03锁存设置，即不可通过按键“MODE”设置模式。
    //0xee non-locking software Settings can be set by the key "mode" set mode.//0xEE不锁存软件设置，可通过按键“MODE”设置模式。
    PS2_Cmd(0X00);
    PS2_Cmd(0X00);
    PS2_Cmd(0X00);
    PS2_Cmd(0X00);
    CS_H;
    delay_us(16);
}
/**************************************************************************
Function: Vibration setting
Input   : none
Output  : none
函数功能：振动设置
入口参数：无
返回  值：无
**************************************************************************/
//static void PS2_VibrationMode(void)
//{
//    CS_L;
//    delay_us(16);
//    PS2_Cmd(0x01);
//    PS2_Cmd(0x4D);
//    PS2_Cmd(0X00);
//    PS2_Cmd(0x00);
//    PS2_Cmd(0X01);
//    CS_H;
//    delay_us(16);
//}
/**************************************************************************
Function: Complete and save the configuration
Input   : none
Output  : none
函数功能：完成并保存配置
入口参数：无
返回  值：无
**************************************************************************/
static void PS2_ExitConfing(void)
{
    CS_L;
    delay_us(16);
    PS2_Cmd(0x01);
    PS2_Cmd(0x43);
    PS2_Cmd(0X00);
    PS2_Cmd(0x00);
    PS2_Cmd(0x5A);
    PS2_Cmd(0x5A);
    PS2_Cmd(0x5A);
    PS2_Cmd(0x5A);
    PS2_Cmd(0x5A);
    CS_H;
    delay_us(16);
}
/**************************************************************************
Function: Handle configuration initialization
Input   : none
Output  : none
函数功能：手柄配置初始化
入口参数：无
返回  值：无
**************************************************************************/
static void PS2_SetInit(void)
{
    PS2_ShortPoll();
    PS2_ShortPoll();
    PS2_ShortPoll();
    PS2_EnterConfing();		  //Enter configuration mode //进入配置模式
    PS2_TurnOnAnalogMode();	//The "traffic light" configuration mode and select whether to save //“红绿灯”配置模式，并选择是否保存
    //PS2_VibrationMode();	//Open vibration mode //开启震动模式
    PS2_ExitConfing();		  //Complete and save the configuration //完成并保存配置
}


