#include "auto_recharge.h"
#include "can.h"
#include "robot_init.h"

CHARGER_t charger;


//自动回充软件变量复位
void auto_recharge_reset(void)
{
	//自动回充模式
	charger.AllowRecharge = 0;
	
	//是否在充电
	charger.Charging = 0;
	
	//充电电流
	charger.ChargingCurrent = 0;
	
	//导航接管标志位
	charger.NavWalk = 0;
	
	//离线监测
	charger.OutLine_Check = 0;
	
	//默认的对接速度设置
	charger.Dock_MoveX = -0.1f;
	charger.Dock_MoveY = 0;
	charger.Dock_MoveZ = 0.2f;
	
	//4路红外信号
	charger.L_A = 0;
	charger.L_B = 0;
	charger.R_A = 0;
	charger.R_B = 0;
	charger.RED_STATE = 0;
	
	//红外刷新时间
	#if defined AKM_CAR
		//阿克曼小车设置
		charger.RedRefalsh_Time = 50;
	#else
		//非阿克曼小车设置0,代表使用充电装备上存在的默认时间
		charger.RedRefalsh_Time = 0;
	#endif
	
	//红外控制、导航接管控制速度复位
	charger.Red_MoveX = 0;
	charger.Red_MoveY = 0;
	charger.Red_MoveZ = 0;
	charger.Up_MoveX = 0;
	charger.Up_MoveY = 0;
	charger.Up_MoveZ = 0;
	
	//访问是否存在充电装备
	Find_Charging_HardWare();
}


//访问当前是否存在充电装备硬件
//发送 扩展帧id为0x12345678,内容为{1,1,1,1,1,1,1,1} 到充电装备,充电装备将返回一样的id和内容,说明充电装备已上线并成功连接.
void Find_Charging_HardWare(void)
{
    u8 tmpdata[8]= {1,1,1,1,1,1,1,1};
    CAN1_Send_EXTid_Num(0x12345678,tmpdata);
}

//设置充电装备上发的对接速度的大小,以及设置红外的刷新时间
void CAN_Send_AutoRecharge(void)
{
	u8 CAN_SENT[8];
	
	//预留位
	CAN_SENT[0]=0;
	
	//Set the speed of the infrared interconnection, unit m/s
	//设置红外对接的速度大小，单位mm/s
	CAN_SENT[1]=((short)(charger.Dock_MoveX*1000))>>8;
	CAN_SENT[2]=((short)(charger.Dock_MoveX*1000));
	CAN_SENT[3]=((short)(charger.Dock_MoveY*1000))>>8;
	CAN_SENT[4]=((short)(charger.Dock_MoveY*1000));
	CAN_SENT[5]=((short)(charger.Dock_MoveZ*1000))>>8;
	CAN_SENT[6]=((short)(charger.Dock_MoveZ*1000));
	CAN_SENT[7] = charger.RedRefalsh_Time;	
	CAN1_Send_Num(0x105,CAN_SENT);

}


//非阿克曼小车的自动回充处理逻辑
#if !defined AKM_CAR
void Handle_Normal_AutoRecharge(uint8_t* temp_rxbuf)
{
	//Calculate the three-axis target velocity, unit: mm/s
	//计算三轴目标速度，单位：mm/s
	charger.Red_MoveX=((float)((short)((temp_rxbuf[0]<<8)+(temp_rxbuf[1]))))/1000; 
	charger.Red_MoveY = 0;
	//charger.Red_MoveY=((float)((short)((temp_rxbuf[2]<<8)+(temp_rxbuf[3]))))/1000;
	charger.Red_MoveZ=((float)((short)((temp_rxbuf[4]<<8)+(temp_rxbuf[5]))))/1000;
	
	charger.Charging=temp_rxbuf[6]&1;		//充电状态标志位
	charger.L_A = (temp_rxbuf[6]>>5)&0x01;
	charger.L_B = (temp_rxbuf[6]>>4)&0x01;
	charger.R_B = (temp_rxbuf[6]>>3)&0x01;
	charger.R_A = (temp_rxbuf[6]>>2)&0x01;
	charger.RED_STATE = charger.L_A + charger.L_B + charger.R_B + charger.R_A;
	
	//不存在红外信号或小车在充电,清空目标速度
	if( 0 == charger.RED_STATE || 1 == charger.Charging  ) charger.Red_MoveX = 0 , charger.Red_MoveY = 0 , charger.Red_MoveZ = 0;
	
	//充电电流换算
	if(temp_rxbuf[7]>128) charger.ChargingCurrent=-(256-temp_rxbuf[7])*30;
	else charger.ChargingCurrent=(temp_rxbuf[7]*30);
}
#endif




#if defined AKM_CAR
//便于debug的内部变量,在debug时可作为全局输出
u8 liar_adjust=0;           //单边充电模式
u8 last_state=0;            //保存红外信号的上一个状态
u16 adjust_timeout=0;       //调节超时设置,用于统计调节时间是否过长
u16 double_adjust_timeout=0;//双边调节超时设置,用于统计调节时间是否过长
u8 red_now_state;           //红外信号当前的状态
u8 touch_state;             //充电桩与充电装备弹片的接触状态

void Handle_AKM_AutoRecharge(uint8_t* temp_rxbuf)
{
	static u8 tmp_state=0;
	static u8 last_touchstate=0;
	
	//接触弹片后的状态锁定
	static u8 state_lock = 0;
	
	//接触弹片后离开时的姿态标志
	static u8 change_state = 0;
	
	//接触弹片后离开时的走时内核
	static u16 time_core = 0;
	
	//接触弹片时的滤波消抖变量
	static u16 filter_cur = 0,filter_vol = 0;
	
	//小车充电姿态的调整次数
	static u8 adjust_times = 0;
	static u8 adjust_vol = 0 , adjust_cur = 0;
	
	//单边充电标志位
	static u8 liar_charge = 0;
	
	//两边都接触到了，标记调整1次
	if(adjust_vol&&adjust_cur) adjust_vol = 0,adjust_cur = 0,adjust_times++;
	
	//红外状态解算
	charger.L_A = (temp_rxbuf[6]>>5)&0x01;
	charger.L_B = (temp_rxbuf[6]>>4)&0x01;
	charger.R_B = (temp_rxbuf[6]>>3)&0x01;
	charger.R_A = (temp_rxbuf[6]>>2)&0x01;
	
	//用于检查充电区、测压区接触情况
	touch_state = temp_rxbuf[2];
	if( last_touchstate!=0xAA&&touch_state==0xAA ) adjust_timeout=0,liar_adjust++;//单边状态调整记录
	if( (last_touchstate!=0xAA&&touch_state==0xAA) || (last_state!=0xBB&&touch_state==0xBB) ) double_adjust_timeout=0;//任意一边接触都清空双边延迟时间
	last_touchstate = touch_state;
	
	//两边都无接触超过5秒，清空调整次数标定
	if(adjust_times>0) double_adjust_timeout++;
	if( double_adjust_timeout>250 ) adjust_times=0,double_adjust_timeout=0;
	
	//充电边无接触5秒，清空充电边调整次数标定
	if( liar_adjust>0 ) adjust_timeout++;
	if( adjust_timeout>250 ) liar_adjust=0,adjust_timeout=0;//接触间隔大于5秒，判断为异常情况，清空调整次数记录。
	
	
	//检测到已经脱离充电口，则需要复位
	if(liar_charge==1&&touch_state!=0xAA&&touch_state!=0xCF) 
	{
		if( touch_state==0xAB && robot.voltage >=25.0f )//充满电了
		{
			charger.AllowRecharge=0;
		}
		adjust_times = 0,liar_charge=0;
	}	
	
	//单边充电，自定义标志位touch_state
	if(liar_charge) touch_state = 0xFC;
	
	/* 针对不同车型设置不同参数 */
	u16 leave_times = 75; //接触弹片后离开的时间 75*20ms = 1.5s
	u8 yuzhi = 40;        //接触弹片后消抖的时间 40*20ms = 800ms
	if(charger.AllowRecharge==0) 
	{

		adjust_times = 0,liar_charge=0,liar_adjust=0;//关闭自动回充时，标定已调整的次数为0
		if( robot.type==2 || robot.type==3 )  Set_Robot_PI_Param(300,300); 
		if( robot.type==4 ) Set_Robot_PI_Param(400,100); 
		if( robot.type==5 ) Set_Robot_PI_Param(50,200); 
	}
	else
	{	
		if( robot.type==0 || robot.type==1 ) leave_times=80, charger.RedRefalsh_Time=100; //原80，50
		if( robot.type==2 || robot.type==3 ) Set_Robot_PI_Param(600,600) , charger.RedRefalsh_Time=100; 
		if( robot.type==4 || robot.type==5 ) 
		{
			//Red_Docker_X=-0.1f, Red_Docker_Y=0, Red_Docker_Z=0.2f; 默认值
			charger.Dock_MoveX = -0.08f;
			charger.Dock_MoveZ = 0.15f;
			Set_Robot_PI_Param(600,600);
			leave_times=113;//顶配独立悬挂电机响应较慢，需要更长的调节时间
			charger.RedRefalsh_Time=150;//红外状态刷新时间(电机响应越慢，建议的时间越长，跟电机减速比、小车负载都有关系)
		}
	}

	//将红外融合成一个数值表示状态，方便给状态标号
	red_now_state = charger.L_A << 3 | charger.L_B << 2 | charger.R_B << 1 | charger.R_A << 0 ;
	
	if( charger.AllowRecharge )//自动回充开启后才记录状态
	{
		//红外状态发生改变时，保存上一个红外状态
		if(red_now_state!=tmp_state) last_state = tmp_state;	
		tmp_state = red_now_state;
	}
	
	////////////////////////////  对接逻辑处理 开始 ////////////////////////////	
	charger.RED_STATE = charger.L_A + charger.L_B + charger.R_B + charger.R_A;
	
		 if(charger.L_A==0&&charger.L_B==0&&charger.R_B==0&&charger.R_A==1)  front_right; //1
		 
	else if(charger.L_A==0&&charger.L_B==0&&charger.R_B==1&&charger.R_A==0)  back_left;  //2
	
	else if(charger.L_A==0&&charger.L_B==0&&charger.R_B==1&&charger.R_A==1)  front_right;  //3

	else if(charger.L_A==0&&charger.L_B==1&&charger.R_B==0&&charger.R_A==0)  //4
	{
		front_left; 
	}	
	
	// charger.L_A==0&&charger.L_B==1&&charger.R_B==0&&charger.R_A==1 不存在 5 
	
	else if(charger.L_A==0&&charger.L_B==1&&charger.R_B==1&&charger.R_A==0) //6
	{
		front_left;   
		if(last_state==2) back;
		
	}	
	
	else if(charger.L_A==0&&charger.L_B==1&&charger.R_B==1&&charger.R_A==1)  back; //7
	
	else if(charger.L_A==1&&charger.L_B==0&&charger.R_B==0&&charger.R_A==0) 
	{
		back_right; //8
		if( last_state==9 ) back;
	}

	else if(charger.L_A==1&&charger.L_B==0&&charger.R_B==0&&charger.R_A==1)   //9
	{
		front_right;
		if( last_state==8||last_state==1 ) back;
	}
	
	else if (charger.L_A==1&&charger.L_B==0&&charger.R_B==1&&charger.R_A==0)  back; //10
	
	else if(charger.L_A==1&&charger.L_B==0&&charger.R_B==1&&charger.R_A==1) // 11
	{
		front_right;
		if(last_state==9) back;
	}
	
	else if(charger.L_A==1&&charger.L_B==1&&charger.R_B==0&&charger.R_A==0)  back_right;  //12	
	
	else if(charger.L_A==1&&charger.L_B==1&&charger.R_B==0&&charger.R_A==1)
	{
		back; //13
	}			
	
	else if(charger.L_A==1&&charger.L_B==1&&charger.R_B==1&&charger.R_A==0)  //14
	{
		front_left; 
		if(last_state==6||last_state==3) back;
	}			

	else if(charger.L_A==1&&charger.L_B==1&&charger.R_B==1&&charger.R_A==1)  back;		  //15
	
	//其他情况，全0或者5
	else
	{
		charger.RED_STATE = 0;//红外识别状态设置为0
		stop;
	}

	
	if(touch_state==0xAA||state_lock==1)//充电装备接触到了充电区
	{
		stop;		

		filter_cur++;   
		if(filter_cur>yuzhi) //延迟消抖
		{
			adjust_cur = 1; //标记接触
			
			filter_cur = yuzhi+1;//滤波变量自锁，防止循环溢出
			
			state_lock = 1;//锁定接触状态，表示在更新状态前，会一直进入本判断
			
			cur_front_right;//调整姿态
			
			//调整次数超过了限制，又接触到了充电区，不再调整了；使用单边充电
			u8 allow_adjust = 2; //对接后允许调整位姿的次数
			if(adjust_times==allow_adjust||( liar_adjust>=3&&adjust_times>0 ))
			{
				stop;		
				liar_charge = 1;//不调整了，开启单边充电
				liar_adjust=0;
			}
		}
		
	}
	else if(touch_state==0xBB||state_lock==2)//接触到了测压区
	{
		stop;
		
		filter_vol++;   
		if(filter_vol>yuzhi) //延迟消抖
		{  
			adjust_vol = 1; //标记接触1次
			
			filter_vol = yuzhi+1;
			state_lock = 2;
			cur_front_left;//调整姿态
		}
	}
	else if(touch_state==0xCF||touch_state==0xFC)
	{
		liar_charge=1;//同时接触测压区和充电区时，开放单边充电，提高稳定性
		liar_adjust=0;
		stop;
	}
	
	//弹片接触状态更新
	if(state_lock!=0)
	{
		time_core++;
		if(time_core> leave_times )
		{
			if(state_lock==2) change_state = 2 ;
			if(state_lock==1) change_state = 1 ;
			state_lock = 0,time_core=0,filter_vol=0,filter_cur=0;
		}				
	}
	else
		time_core =  0;
	
	//弹片状态更新后，屏蔽掉全1以外的所有红外情况
	//接触过弹片又离开，则只接收全红外后退的数据，其他数据一律前进，无需转向
	if(change_state!=0)
	{
		static u8 shielding=0;
		static u8 leaveCount=0;	

		if(++leaveCount==255) leaveCount=0, change_state = 0,shielding=0;//限制总体的调整时间
		
		if(red_now_state==9) {front_right;shielding++;}//特殊状态，靠太边了
		else if( red_now_state==6 ) {front_left;shielding++;}//特殊状态，靠太边了
		else if( red_now_state!=15 ) {front;shielding=0;}
		
		//如果该变量一直自增，说明屏蔽时间太长了，清空调整次数重新开始
		if(shielding>250) adjust_times=0,liar_adjust=0;
		
		if(last_state==15 || last_state==9 || last_state==6 ) leaveCount=0,change_state = 0,shielding=0;
	}
	
	//Z速度转换为角度
	charger.Red_MoveZ = Akm_Vz_to_Angle(charger.Red_MoveX,charger.Red_MoveZ);

	////////////////////////////  对接逻辑处理 结束 ////////////////////////////
	
	if(liar_charge==1) charger.Charging = 1;//机器人正在充电
	else charger.Charging = 0;
	
	if(charger.Charging==1) charger.Red_MoveX = 0, charger.Red_MoveY = 0 , charger.Red_MoveZ = 0;
	
	//充电电流换算
	if(temp_rxbuf[7]>128) charger.ChargingCurrent=-(256-temp_rxbuf[7])*30;
	else  charger.ChargingCurrent=(temp_rxbuf[7]*30);
}
#endif


