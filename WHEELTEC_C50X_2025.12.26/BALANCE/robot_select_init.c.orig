#include "robot_select_init.h"

//Initialize the robot parameter structure
//初始化机器人参数结构体
Robot_Parament_InitTypeDef  Robot_Parament; 
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
	//The ADC value is variable in segments, depending on the number of car models. Currently there are 6 car models, CAR_NUMBER=6
  //ADC值分段变量，取决于小车型号数量
	Divisor_Mode=4096/CAR_NUMBER+5;
	Car_Mode=(int) ((Get_adc_Average(CAR_MODE_ADC,10))/Divisor_Mode); //Collect the pin information of potentiometer //采集电位器引脚信息	
  if(Car_Mode>10)Car_Mode=10;
	#if Akm_Car
	{
		if (Car_Mode==0)  Robot_Init(SENIOR_AKM_wheelspacing, SENIOR_AKM_axlespacing, MD36N_27, Photoelectric_500, SENIOR_AKM_Tyre_Diameter); //SENIOR_AKM_27 - 高配阿克曼小车常规型 
		if (Car_Mode==1)  Robot_Init(SENIOR_AKM_wheelspacing, SENIOR_AKM_axlespacing, MD36N_51, Photoelectric_500, SENIOR_AKM_Tyre_Diameter); //SENIOR_AKM_51 - 高配阿克曼小车重载型
		
		if (Car_Mode==2)  Robot_Init(TOP_AKM_BS_wheelspacing, TOP_AKM_BS_axlespacing, MD60N_18, Photoelectric_500, TOP_AKM_BS_WHEEL_Diameter); //TOP_AKM_BS_18 - 顶配阿克曼小车摆式悬挂常规型  //BS: Pendulum suspension
		if (Car_Mode==3)  Robot_Init(TOP_AKM_BS_wheelspacing, TOP_AKM_BS_axlespacing, MD60N_47, Photoelectric_500, TOP_AKM_BS_WHEEL_Diameter); //TOP_AKM_BS_18 - 顶配阿克曼小车摆式悬挂重载型 
		
		if (Car_Mode==4)  Robot_Init(TOP_AKM_DL_wheelspacing, TOP_AKM_DL_axlespacing, MD60N_18, Photoelectric_500, TOP_AKM_DL_Tyre_Diameter),  //TOP_AKM_DL_18 - 顶配阿克曼小车独立悬挂常规型(卡丁车) //DL: Independent suspension
			                Velocity_KP=400,Velocity_KI=100; //PID parameter optimization //PID参数优化
		if (Car_Mode==5)  Robot_Init(TOP_AKM_DL_wheelspacing, TOP_AKM_DL_axlespacing, MD60N_47, Photoelectric_500, TOP_AKM_DL_Tyre_Diameter),  //TOP_AKM_DL_47 - 顶配阿克曼小车独立悬挂重载型(卡丁车) 
		                  Velocity_KP=50,Velocity_KI=200; //PID parameter optimization //PID参数优化
		//if (Car_Mode==6)  Robot_Init(LONG_AKM_wheelspacing, LONG_AKM_axlespacing,   MD36N_51, Photoelectric_500, SENIOR_AKM_Tyre_Diameter);  //Customized special - 定制专用
		if (Car_Mode==7)  Robot_Init(TOP_AKM_DL_wheelspacing, TOP_AKM_DL_axlespacing,   MD60N_18, Photoelectric_500, TOP_AKM_DL_Tyre_Diameter);  //Customized special - 测试专用
		if (Car_Mode==8)  Robot_Init(TOP_AKM_DL_wheelspacing, TOP_AKM_DL_axlespacing,   MD60N_18, Photoelectric_500, TOP_AKM_DL_Tyre_Diameter);  //Customized special - 测试专用
		if (Car_Mode==9)  Robot_Init(SENIOR_AKM_wheelspacing, SENIOR_AKM_axlespacing, MD36N_5_18, Photoelectric_500, SENIOR_AKM_Tyre_Diameter),
                			Velocity_KP=200,Velocity_KI=50; //SENIOR_AKM_5.18 - 高配阿克曼小车5.18减速比车型
		
  }
	#elif Diff_Car
	{
		if (Car_Mode==0)  Robot_Init(TOP_DIFF_wheelspacing,           0, MD36N_27, Photoelectric_500, TOP_DIFF_Tyre_Diameter);//TOP_DIFF_27 - 顶配差速常规型  
		if (Car_Mode==1)  Robot_Init(TOP_DIFF_wheelspacing,           0, MD36N_51, Photoelectric_500, TOP_DIFF_Tyre_Diameter);//TOP_DIFF_51 - 顶配差速重载型
		
		if (Car_Mode==2)  Robot_Init(FOUR_WHEEL_DIFF_BS_wheelspacing, 0, MD60N_18, Photoelectric_500, FOUR_WHEEL_DIFF_Tyre_Diameter);//FOUR_WHEEL_DIFF_BS_18 - 四轮两驱摆式悬挂常规型 //BS: Pendulum suspension
		if (Car_Mode==3)  Robot_Init(FOUR_WHEEL_DIFF_BS_wheelspacing, 0, MD60N_47, Photoelectric_500, FOUR_WHEEL_DIFF_Tyre_Diameter);//FOUR_WHEEL_DIFF_BS_47 - 四轮两驱摆式悬挂重载型
		
		if (Car_Mode==4)  Robot_Init(FOUR_WHEEL_DIFF_DL_wheelspacing, 0, MD60N_18, Photoelectric_500, FOUR_WHEEL_DIFF_Tyre_Diameter);//FOUR_WHEEL_DIFF_BS_18 - 四轮两驱独立悬挂常规型 //DL: Independent suspension
		if (Car_Mode==5)  Robot_Init(FOUR_WHEEL_DIFF_DL_wheelspacing, 0, MD60N_47, Photoelectric_500, FOUR_WHEEL_DIFF_Tyre_Diameter);//FOUR_WHEEL_DIFF_BS_47 - 四轮两驱独立悬挂重载型
  }
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
void Robot_Init(float wheelspacing,float axlespacing,int gearratio,int Accuracy,float tyre_diameter) 
{
  Robot_Parament.WheelSpacing=wheelspacing;   //Wheelspacing 轮距  
	Robot_Parament.AxleSpacing=axlespacing;     //Axlespacing 轴距
  Robot_Parament.GearRatio=gearratio;         //motor_gear_ratio //电机减速比
  Robot_Parament.EncoderAccuracy=Accuracy;    //Number_of_encoder_lines //编码器精度(编码器线数)
  Robot_Parament.WheelDiameter=tyre_diameter; //Diameter of driving wheel //主动轮轮径
	
	//Encoder value corresponding to 1 turn of motor (wheel)
	//电机(车轮)转1圈对应的编码器数值
	Encoder_precision=EncoderMultiples*Robot_Parament.EncoderAccuracy*Robot_Parament.GearRatio;
	//Driving wheel circumference //主动轮周长	
	Wheel_perimeter=Robot_Parament.WheelDiameter*PI; 
	//Wheelspacing 轮距 
  Wheel_spacing=Robot_Parament.WheelSpacing;  
 //Wheel_axlespacing (Wheel_axlespacing is not required for motion analysis of differential trolleys) //轴距(差速小车的运动分析不需要轴距)	
	Axle_spacing=Robot_Parament.AxleSpacing;   
}
