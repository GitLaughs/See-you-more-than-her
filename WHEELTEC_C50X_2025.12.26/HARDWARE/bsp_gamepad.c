#include "bsp_gamepad.h"

#include "FreeRTOS.h"
#include "task.h"


GamePadDebugValType_t GamePadDebug = { 0 };

//定义游戏手柄品牌
GamePadBrandType_t gamepad_brand = UnKnown_Dev;

extern void OLED_ShowGamePadState(void);

//手柄设备插入函数_任务环境
void USB_GamePad_InsertCallback(void)
{
	OLED_ShowGamePadState();
	GamePadDebug.enmu_state = EnumWait;
//	vTaskDelay(pdMS_TO_TICKS(1000)); //延迟1000ms用于OLED显示
	GamePadDebug.enmu_state = EnumDone;
	GamePadDebug.type = gamepad_brand;
}

//手柄设备拔出_任务环境
void USB_GamePad_PullOutCallback(void)
{
	//标定为未知设备
	gamepad_brand = UnKnown_Dev;
	
	//将当前的手柄摇杆值复位,防止下次识别时有值
	GamePadInterface->LX = 127;
	GamePadInterface->RX = 127;
	GamePadInterface->LY = 127;
	GamePadInterface->RY = 127;
	GamePadInterface->LT = 0;
	GamePadInterface->RT = 0;
	
	//再切回至默认接口防止出现控制量
	GamePadInterface = &GamePadDefalut;
	
	//调试相关
	GamePadDebug.enmu_state = EnumNULL;
	GamePadDebug.type = gamepad_brand;
	GamePadDebug.ready = 0;
}

//通用游戏手柄对象
GamePadType_t* GamePadInterface = 0;

GamePadType_t GamePadDefalut = {
	.LX = 127,
	.LY = 127,
	.RX = 127,
	.RY = 127,
	.LT = 0,
	.RT = 0,
	.StartFlag = 0,
	.Vib_EN = 1,
	.SetVibration = 0,
	.getKeyState = 0
};

//入口参数：按键原始值，按键属性，需要检测的按键
GamePadKeyEventType_t GamePadKey_CheckEvent(uint16_t keysource,GamePad_CheckEventType_t* key,uint8_t bit)
{
    //读取对应的键值状态
    key->keystate = (keysource>>bit)&0x01;

    switch (key->statemachine)
    {
        case WaitToPress:
            if( GamePadKeyState_Press == key->keystate )
            {
                key->timebase = xTaskGetTickCount();
                key->statemachine = KEYPress;
            } 
            break;
        case KEYPress:
            //统计第一次按下按键以后的时间(无符号溢出时自然环绕.不需要溢出判断)
            key->statetime = xTaskGetTickCount() - key->timebase;

            //检查按键是否有松开
            if( GamePadKeyState_Release == key->keystate )
            {
                //单次按键按下的时间太短,忽略.作为滤波作用
                if( key->statetime < GamePad_KEYFILTER_TIME ) key->statemachine = WaitToPress;
                else
                {
                   key->timebase = xTaskGetTickCount();//重新更新时基,用于下一个状态的检测
                   key->statemachine = KEYUp;    //按键按下一定时间,又弹起,进入下一个检测状态
                }
            }
            else if( key->statetime > GamePad_LONGPRESS_TIEM ) 
            {   //按键未松开,且保持一定的时间,则为长按.
                key->statemachine = LONG_CLICK;
            }

            break;
        case KEYUp:
            //统计第一次按下按键以后的时间(无符号溢出时自然环绕.不需要溢出判断)
            key->statetime = xTaskGetTickCount() - key->timebase;

            if( GamePadKeyState_Press == key->keystate && key->statetime < GamePad_CLICK_TIME && key->statetime > GamePad_KEYFILTER_TIME )
            {
                key->statemachine = WaitToRelease;
                return GamePadKeyEvent_DOUBLECLICK;
            }
            else if( key->statetime >= GamePad_CLICK_TIME )
            {
                key->statemachine = WaitToRelease;
                return GamePadKeyEvent_SINGLECLICK;
            }
            break;
        case LONG_CLICK:
            key->statemachine = WaitToRelease;
            return GamePadKeyEvent_LONGCLICK;
        case WaitToRelease:
            //按键检测完毕,等用户松开按键后,再恢复状态机的状态
            if( GamePadKeyState_Release == key->keystate ) key->statemachine = WaitToPress;
            break;
        default:
            break;
    }

    return GamePadKeyEvent_NONE;
}

