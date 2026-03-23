#include "robot_init.h"

static void set_RobotType(uint8_t type);

static void Robot_Init(float wheelspacing,float axlespacing,float gearratio,int Accuracy,\
	                    float tyre_diameter,int vkp,int vki);

//小车硬件结构和电机相关变量
ROBOT_t robot;

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
	
	TypeNum = Get_ADC1_Average(CarMode_Ch,10)/TypeNum;
	
	set_RobotType(TypeNum);//根据板子电位器选择确定不同的车型
	
	switch( TypeNum )
	{
		//高配摆式常规、重载
		case 0: Robot_Init( SENIOR_4WD_BS_wheelspacing , SENIOR_4WD_BS_axlespacing , MD36N_27 , GMR_500 , WheelDiameter_4WD_152 , VEL_KP , VEL_KI ); break;
		case 1: Robot_Init( SENIOR_4WD_BS_wheelspacing , SENIOR_4WD_BS_axlespacing , MD36N_51 , GMR_500 , WheelDiameter_4WD_152 , VEL_KP , VEL_KI ); break;
		
		//高配独立常规
		case 2: Robot_Init( SENIOR_4WD_DL_wheelspacing , SENIOR_4WD_DL_axlespacing , MD36N_27 , GMR_500 , WheelDiameter_4WD_152 , VEL_KP , VEL_KI ); break;
		case 3: Robot_Init( SENIOR_4WD_DL_wheelspacing , SENIOR_4WD_DL_axlespacing , MD36N_51 , GMR_500 , WheelDiameter_4WD_152 , VEL_KP , VEL_KI ); break;
		
		//顶配摆式常规、重载
		case 4: Robot_Init( TOP_4WD_BS_wheelspacing , TOP_4WD_BS_axlespacing , MD60N_18 , GMR_500 , WheelDiameter_4WD_225 , VEL_KP , VEL_KI ); break;
		case 5: Robot_Init( TOP_4WD_BS_wheelspacing , TOP_4WD_BS_axlespacing , MD60N_47 , GMR_500 , WheelDiameter_4WD_225 , VEL_KP , VEL_KI ); break;
		
		//顶配独立常规、重载
		case 6: Robot_Init( TOP_4WD_DL_wheelspacing , TOP_4WD_DL_axlespacing , MD60N_18 , GMR_500 , WheelDiameter_4WD_225 , VEL_KP , VEL_KI ); break;
		case 7: Robot_Init( TOP_4WD_DL_wheelspacing , TOP_4WD_DL_axlespacing , MD60N_47 , GMR_500 , WheelDiameter_4WD_225 , VEL_KP , VEL_KI ); break;
		
		//旗舰摆式常规、重载
		case 8: Robot_Init( FlagShip_4WD_BS_wheelspacing , FlagShip_4WD_BS_axlespacing , MD60N_18 , GMR_500 , WheelDiameter_4WD_225 , VEL_KP , VEL_KI ); break;
		case 9: Robot_Init( FlagShip_4WD_BS_wheelspacing , FlagShip_4WD_BS_axlespacing , MD60N_47 , GMR_500 , WheelDiameter_4WD_225 , VEL_KP , VEL_KI ); break;
		
		//旗舰独立常规、重载
		case 10: Robot_Init( FlagShip_4WD_DL_wheelspacing , FlagShip_4WD_DL_axlespacing , MD60N_18 , GMR_500 , WheelDiameter_4WD_225 , VEL_KP , VEL_KI ); break;
		case 11: Robot_Init( FlagShip_4WD_DL_wheelspacing , FlagShip_4WD_DL_axlespacing , MD60N_47 , GMR_500 , WheelDiameter_4WD_225 , VEL_KP , VEL_KI ); break;
		
		//预留定制车型
		case 12: Robot_Init( SENIOR_4WD_BS_wheelspacing , SENIOR_4WD_BS_axlespacing , MD36N_27 , GMR_500 , WheelDiameter_4WD_152 , VEL_KP , VEL_KI ); break;
		default:break;
	}
}


/**************************************************************************
Function: Initialize cart parameters
Input   : wheelspacing, axlespacing, omni_rotation_radiaus, motor_gear_ratio, Number_of_encoder_lines, tyre_diameter
Output  : none
函数功能：初始化小车参数
入口参数：轮距 轴距 自转半径 电机减速比 电机编码器精度 轮胎直径
返回  值：无
**************************************************************************/
static void set_RobotType(uint8_t type)
{
	robot.type = type;
}

//入口参数：车型、轮距、轴距、电机减速比、电机编码器精度、轮子直径、阿克曼最小转弯半径、全向轮自转半径、默认kp、默认ki
static void Robot_Init(float wheelspacing,float axlespacing,float gearratio,int Accuracy,\
	                    float tyre_diameter,int vkp,int vki)
{
    robot.HardwareParam.WheelSpacing = wheelspacing;   //轮距
	robot.HardwareParam.AxleSpacing = axlespacing;     //轴距
	robot.HardwareParam.GearRatio = gearratio;         //电机减速比
	robot.HardwareParam.EncoderAccuracy = Accuracy;    //电机编码器精度
	robot.HardwareParam.Wheel_Circ = tyre_diameter*PI; //轮子周长(PI*D)
	//编码器精度*电机减速比*倍频数 = 电机旋转一圈的编码器读数
	robot.HardwareParam.Encoder_precision = EncoderMultiples * robot.HardwareParam.EncoderAccuracy * robot.HardwareParam.GearRatio;
	robot.V_KP = vkp;                                  //默认kp参数
	robot.V_KI = vki;                                  //默认ki参数
}

