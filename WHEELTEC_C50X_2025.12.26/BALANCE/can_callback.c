#include "can_callback.h"

void CAN1_RX0_IRQHandler(void)
{
	CanRxMsg RxMessage;   
	
	u8 temp_rxbuf[8];
	
	//读取CAN1 FIFO0邮箱的数据
	CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);
	
	//将数据读出缓冲区使用
	memcpy(temp_rxbuf,RxMessage.Data,8);
	
	//标准帧ID数据处理
	if( RxMessage.IDE==CAN_Id_Standard )
	{
		switch( RxMessage.StdId ) //帧ID号
		{
			case 0x181: //帧ID 0x181为小车控制指令
			{
				//设置CAN控制模式
				if( Get_Control_Mode(_CAN_Control)==0 ) Set_Control_Mode(_CAN_Control);
				robot_control.command_lostcount = 0;//命令超时刷新
				//接受小车目标速度
				robot_control.Vx = ((float)((short)((temp_rxbuf[0]<<8)|(temp_rxbuf[1]))))/1000.0f;
				robot_control.Vy = ((float)((short)((temp_rxbuf[2]<<8)|(temp_rxbuf[3]))))/1000.0f;
				robot_control.Vz = ((float)((short)((temp_rxbuf[4]<<8)|(temp_rxbuf[5]))))/1000.0f;
				break;
			}
			
			case 0x182: //自动回充设备的命令
			{		
				charger.OutLine_Check = 0;//回充状态在线刷新
				#if defined AKM_CAR
					Handle_AKM_AutoRecharge(temp_rxbuf); //阿克曼自动回充逻辑处理函数
				#else
					Handle_Normal_AutoRecharge(temp_rxbuf); //其他小车自动回充处理逻辑函数
				#endif
			}
			
			default:
				break;
		}
	}
	
	//扩展帧ID数据处理
	else if( RxMessage.IDE==CAN_Id_Extended )
	{
		switch( RxMessage.ExtId ) //帧ID号
		{
			case 0x12345678://自动回充特征码校验,用于检查小车是否存在自动回充设备
			{
				u8 check=0;
				for(u8 i=0;i<8;i++) 
				{
					check += temp_rxbuf[i];
				}
				if( check==8 ) SysVal.HardWare_charger = 1;
				break;
			}
			
			case 0x001:
			{
				break;
			}
				
			default:
				break;
		}
	}
	
}
