#ifndef __ROBOTSELECTINIT_H
#define __ROBOTSELECTINIT_H
#include "sys.h"
#include "system.h"

//Parameter structure of robot
//机器人结构相关参数
typedef struct  
{
	float WheelSpacing;           //Wheelspacing, Mec_Car is half wheelspacing //轮距参数 麦轮车为半轮距
	float AxleSpacing;            //Axlespacing, Mec_Car is half axlespacing   //轴距参数 麦轮车为半轴距	
	float Wheel_Circ;             //Wheel circumference                        //主动轮周长
	float GearRatio;              //Motor_gear_ratio                            //电机减速比参数
	uint16_t   EncoderAccuracy;   //Number_of_encoder_lines                     //编码器精度(编码器线数)
	uint8_t    type;              //Robot model                                 //型号
	
	//不同类型车的专属参数
	#if defined AKM_CAR
		float MIN_turn_radius;    //阿克曼小车的最小转弯半径限制
	#endif
	
	#if defined OMNI_CAR
		float TurnRadiaus;        //Rotation radius of omnidirectional trolley //全向轮小车旋转半径
	#endif
	
}Robot_Parament_InitTypeDef;

//Robot servo motor related parameters
//机器人伺服电机相关参数
typedef struct  
{
	float Target;                //Control the target speed of the motor            //电机目标速度值，控制电机目标速度
	float Encoder;               //Read the real time speed of the motor by encoder //编码器数值，读取电机实时速度
}Moto_parameter;


typedef struct
{
	Robot_Parament_InitTypeDef HardwareParam;//机器人的结构参数
	
	Moto_parameter MOTOR_A;
	Moto_parameter MOTOR_B;

	#if defined AKM_CAR
		Moto_parameter MOTOR_SERVO;
	#elif defined OMNI_CAR
		Moto_parameter MOTOR_C;
	#elif defined MEC_CAR || defined _4WD_CAR
		Moto_parameter MOTOR_C;
		Moto_parameter MOTOR_D;
	#endif
	
	//用于控制机器人运动的pid参数
	int V_KP;
	int V_KI;
	
}ROBOT;


//Motor_gear_ratio
//电机减速比
#define   MD36N_5_18  5.18
#define   MD36N_27    27
#define   MD36N_51    51
#define   MD36N_71    71
#define   MD60N_18    18
#define   MD60N_47    47

//Number_of_encoder_lines
//编码器精度
#define		GMR_500  500
#define	    Hall_13  13

//The encoder octave depends on the encoder initialization Settings
//编码器倍频数，取决于编码器初始化设置
#define   EncoderMultiples 4

//默认PID参数
#define VEL_KP 300
#define VEL_KI 300

//不同类别车型下的参数
#if defined AKM_CAR
	//阿克曼小车拥有的车型数量
	#define CAR_NUMBER    10    
	//Wheel_spacing //轮距
	#define   SENIOR_AKM_wheelspacing  0.322f //高配阿克曼
	#define   TOP_AKM_BS_wheelspacing  0.508f //顶配摆式悬挂阿克曼
	#define   TOP_AKM_DL_wheelspacing  0.585f //顶配独立悬挂阿克曼
	
	//Axle_spacing //轴距
	#define   SENIOR_AKM_axlespacing   0.322f //高配阿克曼
	#define   TOP_AKM_BS_axlespacing   0.590f //顶配摆式悬挂阿克曼
	#define   TOP_AKM_DL_axlespacing   0.530f //顶配独立悬挂阿克曼
	
	//Diameter of trolley tire
	//小车轮胎直径
	#define   SENIOR_AKM_WheelDiameter  0.125 //高配阿克曼
	#define   TOP_AKM_BS_WheelDiameter  0.180 //顶配摆式悬挂阿克曼
	#define   TOP_AKM_DL_WheelDiameter  0.254 //顶配独立悬挂阿克曼

	//The minimum turning radius of different Ackermann models is determined by the mechanical structure:
	//the maximum Angle of the wheelbase, wheelbase and front wheel
	//不同阿克曼车型的最小转弯半径，由机械结构决定：轮距、轴距、前轮最大转角
	#define   SENIOR_AKM_MIN_TURN_RADIUS  0.750f //高配阿克曼
	#define   TOP_AKM_BS_MIN_TURN_RADIUS  1.400f //顶配摆式悬挂阿克曼
	#define   TOP_AKM_DL_MIN_TURN_RADIUS  1.200f //顶配独立悬挂阿克曼
	
#elif defined DIFF_CAR
	//差速小车的车型数量
	#define   CAR_NUMBER    9   
	
	//轮距
	#define   TOP_DIFF_wheelspacing            0.329f //差速车
	#define   FOUR_WHEEL_DIFF_BS_wheelspacing  0.573f //四轮差速摆式悬挂
	#define   FOUR_WHEEL_DIFF_DL_wheelspacing  0.573f //四轮差速独立悬挂
	
	//差速车无轴距数据
	
	//轮胎直径
	#define   TOP_DIFF_WheelDiameter        0.125 //两轮差速主动轮直径
	#define   FOUR_WHEEL_DIFF_WheelDiameter 0.215 //四轮差速主动轮直径
	
#elif defined MEC_CAR

	#define CAR_NUMBER    11     
	
#elif defined _4WD_CAR

	#define CAR_NUMBER    12    
	
#elif defined OMNI_CAR

	#define CAR_NUMBER    7     
	
#endif




#endif
