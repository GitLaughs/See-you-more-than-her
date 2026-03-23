#include "robot_select_init.h"

static void Robot_Init(uint8_t type,float wheelspacing,float axlespacing,float gearratio,int Accuracy,\
	                    float tyre_diameter,float akm_min_turn,float omin_turnR,int vkp,int vki);

//定义一个机器人，内部包含机器人的各种参数
static ROBOT robot;

/**************************************************************************
Function: According to the potentiometer switch needs to control the car type
Input   : none
Output  : none
函数功能：根据电位器切换需要控制的小车类型
入口参数：无
返回  值：无
**************************************************************************/
void Robot_Select(void)
{
	uint16_t TypeNum = 4096/CAR_NUMBER;//根据车型数量,获取ADC采集的分频系数
	
	TypeNum = Get_ADC1_Average(CarMode_Ch,10)/TypeNum;//根据板子电位器选择确定不同的车型
	
	#if defined AKM_CAR
	
	switch( TypeNum )
	{
		//高配常规、重载
		case 0: Robot_Init(TypeNum,SENIOR_AKM_wheelspacing,SENIOR_AKM_axlespacing,MD36N_27,GMR_500,SENIOR_AKM_WheelDiameter,SENIOR_AKM_MIN_TURN_RADIUS,0,VEL_KP,VEL_KI);break;
		case 1: Robot_Init(TypeNum,SENIOR_AKM_wheelspacing,SENIOR_AKM_axlespacing,MD36N_51,GMR_500,SENIOR_AKM_WheelDiameter,SENIOR_AKM_MIN_TURN_RADIUS,0,VEL_KP,VEL_KI);break;
		
		//顶配摆式常规、重载
		case 2: Robot_Init(TOP_AKM_BS_wheelspacing,TOP_AKM_BS_axlespacing,MD60N_18,GMR_500,TOP_AKM_BS_WheelDiameter,VEL_KP,VEL_KI);break;
		case 3: Robot_Init(TOP_AKM_BS_wheelspacing,TOP_AKM_BS_axlespacing,MD60N_47,GMR_500,TOP_AKM_BS_WheelDiameter,VEL_KP,VEL_KI);break;
		
		//顶配独立常规、重载
		case 4: Robot_Init(TOP_AKM_DL_wheelspacing,TOP_AKM_DL_axlespacing,MD60N_18,GMR_500,TOP_AKM_DL_WheelDiameter,400,100);break;
		case 5: Robot_Init(TOP_AKM_DL_wheelspacing,TOP_AKM_DL_axlespacing,MD60N_47,GMR_500,TOP_AKM_DL_WheelDiameter,50,200);break;
		
		//预留非标定制车 车型
		case 6: Robot_Init(SENIOR_AKM_wheelspacing,SENIOR_AKM_axlespacing,MD36N_27,GMR_500,SENIOR_AKM_WheelDiameter,VEL_KP,VEL_KI);break;
		
		//7、8安装测试使用车型
		case 7: Robot_Init(SENIOR_AKM_wheelspacing,SENIOR_AKM_axlespacing,MD36N_27,GMR_500,SENIOR_AKM_WheelDiameter,VEL_KP,VEL_KI);break;
		case 8: Robot_Init(SENIOR_AKM_wheelspacing,SENIOR_AKM_axlespacing,MD36N_27,GMR_500,SENIOR_AKM_WheelDiameter,VEL_KP,VEL_KI);break;
		
		//高速阿克曼(高配车型,5.18减速比电机)
		case 9: Robot_Init(SENIOR_AKM_wheelspacing,SENIOR_AKM_axlespacing,MD36N_5_18,GMR_500,SENIOR_AKM_WheelDiameter,200,50);break;
		
		default:break;
	}
	
	
	#elif defined DIFF_CAR
	
	#elif defined MEC_CAR
	
	#elif defined _4WD_CAR
	
	#elif defined OMNI_CAR
	
	#endif
    
}


/**************************************************************************
Function: Initialize cart parameters
Input   : wheelspacing, axlespacing, omni_rotation_radiaus, motor_gear_ratio, Number_of_encoder_lines, tyre_diameter
Output  : none
函数功能：初始化小车参数
入口参数：轮距 轴距 自转半径 电机减速比 电机编码器精度 轮胎直径
返回  值：无
**************************************************************************/

void robot_param_setKP(int setKP)
{
	robot.V_KP = setKP;
}

void robot_param_setKI(int setKI)
{
	robot.V_KI = setKI;
}


//入口参数：车型、轮距、轴距、电机减速比、电机编码器精度、轮子直径、阿克曼最小转弯半径、全向轮自转半径、默认kp、默认ki
static void Robot_Init(uint8_t type,float wheelspacing,float axlespacing,float gearratio,int Accuracy,\
	                    float tyre_diameter,float akm_min_turn,float omin_turnR,int vkp,int vki)
{
	robot.HardwareParam.type = type;                   //车型
    robot.HardwareParam.WheelSpacing = wheelspacing;   //轮距
	robot.HardwareParam.AxleSpacing = axlespacing;     //轴距
	robot.HardwareParam.GearRatio = gearratio;         //电机减速比
	robot.HardwareParam.EncoderAccuracy = Accuracy;    //电机编码器精度
	robot.HardwareParam.Wheel_Circ = tyre_diameter*PI; //轮子周长(PI*D)
	robot.HardwareParam.MIN_turn_radius = akm_min_turn;//最小转弯半径
	
	
	robot_param_setKP(vkp);
	robot_param_setKI(vki);
}

//轮距
float get_wheelspacing(void)
{
	return robot.HardwareParam.WheelSpacing;
}

//轴距
float get_AxleSpacing(void)
{
	return robot.HardwareParam.AxleSpacing;
}

//减速比
float get_GearRatio(void)
{
	return robot.HardwareParam.GearRatio;
}

//编码器精度
uint16_t get_EncoderAccuracy(void)
{
	return robot.HardwareParam.EncoderAccuracy;
}

//轮子周长
float get_Wheel_Circ(void)
{
	return robot.HardwareParam.Wheel_Circ;
}

int get_robot_KP(void)
{
	return robot.V_KP;
}

int get_robot_KI(void)
{
	return robot.V_KI;
}
