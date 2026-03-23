#ifndef __BALANCE_H
#define __BALANCE_H			  	 
#include "system.h"

#include "bsp_gamepad.h"

//RTOS任务配置
#define BALANCE_TASK_PRIO		4             //Task priority //任务优先级
#define BALANCE_STK_SIZE 		512           //Task stack size //任务堆栈大小
#define BALANCE_TASK_RATE       RATE_100_HZ   //任务频率

//机器人控制相关参数
typedef struct{
	u8 ControlMode;       //机器人控制模式_(变量说明:非用户直接操作变量,请勿进行直接赋值操作,仅可读)
	u8 FlagStop;          //机器人是否使能标志位,1代表机器人失能,0代表使能_(变量说明:非用户直接操作变量,请勿进行直接赋值操作,仅可读)
	u8 command_lostcount; //机器人丢失控制命令计数,丢失命令到达一定时间可停止机器人的自运动
	u8 SoftWare_Stop;     //机器人软件急停,设置1急停,0使能. 仅为预留功能,若需要使用时，设置该值为1即可
	float Vx;            //机器人x轴目标速度
	float Vy;            //机器人y轴目标速度
	float Vz;            //机器人z轴目标速度
	float smooth_Vx;     //机器人速度平滑结果值
	float smooth_Vy;     //机器人速度平滑结果值
	float smooth_Vz;     //机器人速度平滑结果值
	
	float smooth_MotorStep;   //机器人电机速度平滑步进值
	float smooth_ServoStep;   //机器人舵机速度平滑步进值
	float smooth_Servo;  //机器人速度平滑结果值
	u8 ServoLow_flag;    //低速舵机模式,在上电时以及小车失能后舵机位置发生改变时使用(仅顶配阿克曼).
	
	float rc_speed;      //机器人遥控速度基准,默认500 mm/s
	float limt_max_speed;//机器人最大允许的速度限制.单位m/s,标准车型默认设置3.5 m/s，实际上标准机器人达不到这个速度.
	uint32_t LineDiffParam;//纠偏系数，0-100可调整

}ROBOT_CONTROL_t;

//机器人自检相关变量
typedef struct{
	u8 errorflag; //自检报错标志位
	u8 check_end; //自检结束标志位
	int check_a;  //4个轮子编码器自检相关变量
	int check_b;
	int check_c;
	int check_d;
	u8 DeepCheck; //进入深度检测标志位
}ROBOT_SELFCHECK_t;

//机器人失能时的电机状态. 
enum{
	UN_LOCK = 0, //电机解轴
	LOCK    = 1  //电机锁轴
};

//机器人驻车模式相关变量
typedef struct{
	u8 wait_check;  //等待检测pwm清除标志位
	u16 timecore;   //计时变量
	u8 start_clear; //是否开始清除残余PWM
	u8 clear_state; //pwm残余清除的状态.使用最高位和低4位:低4位代表4个电机,最低位为电机A,依次类推. 最高位代表清除任务是否完成.
}ROBOT_PARKING_t;

//增量式PI控制器结构体
typedef struct{
	float Bias;    // 与目标值偏差
	float LastBias;// 上一次的偏差
	int Output;    // 输出
	int kp;        // kp值
	int ki;        // ki值
}PI_CONTROLLER;

//顶配阿克曼专用变量,用于舵机非自锁功能的实现
typedef struct{
	uint8_t UnLock;      //舵机非自锁模式标志位
	uint8_t wait_Calib;  //等待校准
	float UnLock_Pos;   //舵机非自锁模式下记录的位置
	float UnLock_Target;//非自锁模式下的保存到的目标值
	uint16_t UnLock_Output;//进入非自锁模式时的舵机PWM值,仅用作显示使用.
}AKM_SERVO_UNLOCK_t;

extern ROBOT_CONTROL_t robot_control;
extern ROBOT_SELFCHECK_t robot_check;
extern AKM_SERVO_UNLOCK_t ServoState;

//机器人控制方式设置与读取
enum
{
	_ROS_Control   =  (1<<0), //ROS控制
	_PS2_Control   =  (1<<1), //PS2控制
	_APP_Control   =  (1<<2), //APP控制
	_RC_Control    =  (1<<3), //航模遥控控制
	_CAN_Control   =  (1<<4), //CAN通信控制
	_USART_Control =  (1<<5), //串口控制
};
//设置机器人控制方式
#define Set_Control_Mode(mask)  (robot_control.ControlMode |= (mask), robot_control.ControlMode &= (mask)) 
//读取机器人控制方式
#define Get_Control_Mode(mask)  (robot_control.ControlMode & (mask))

//对外变量
extern SYS_VAL_t SysVal;
extern ROBOT_CONTROL_t robot_control;
extern PI_CONTROLLER PI_MotorA,PI_MotorB,PI_MotorC,PI_MotorD,PI_Servo;

//对外函数
void Balance_task(void *pvParameters); //任务声明
//void Akm_ReadServo_Param(void);//从Flash读取阿克曼舵机的信息
void PI_Controller_Init(PI_CONTROLLER* p,int kp,int ki); //软件初始化函数
void ROBOT_CONTROL_t_Init(ROBOT_CONTROL_t* p);
void  Set_Robot_PI_Param(int kp,int ki); //PI控制器设置参数
float rad_to_angle(const float rad);  //角度与弧度互转
float angle_to_rad(const float angle);//角度与弧度互转
float Akm_Vz_to_Angle(float Vx,float Vz);//将阿克曼的目标速度转换为左前轮转角
short get_ServoPWM(short TmpPos);//根据滑轨的数据估测舵机的PWM数值
float target_limit_float(float insert,float low,float high);
void FlashParam_Read(void);

//内部使用函数
static void ROBOT_SELFCHECK_t_Init(ROBOT_SELFCHECK_t* p); //初始化类

static void PI_Controller_Reset(PI_CONTROLLER *p); //PI控制类
static void PI_SetParam(PI_CONTROLLER* p,int kp,int ki);
static int Incremental_MOTOR(PI_CONTROLLER* p,float current,float target);
static int Incremental_Servo(PI_CONTROLLER* p,float current,float target);
static uint8_t PI_Clear_Output(PI_CONTROLLER* p);

static void InverseKinematics_akm(float Vx,float Vz); //运动学分析类
static void InverseKinematics_diff(float Vx,float Vz);
static void InverseKinematics_mec(float Vx,float Vy,float Vz);
static void InverseKinematics_4wd(float Vx,float Vz);
static void InverseKinematics_omni(float Vx,float Vy,float Vz);

static void Drive_Motor(float T_Vx,float T_Vy,float T_Vz); //控制类
static void ResponseControl(void);
static void UnResponseControl(uint8_t mode);
static void Set_Pwm(int m_a,int m_b,int m_c,int m_d,int servo);
static u8 Turn_Off(void);
static void Get_APPcmd(void);
static void Remote_Control(void);
static void PS2_control(void);
static void robot_mode_check(void);
static void Get_Robot_FeedBack(void);
static void Servo_UnLock_Check(uint8_t car_stopflag);

static void Robot_ParkingCheck(void); //状态检测类
static void UserKey_Scan(u16 rate);
static void Charger_DevCheck(void);

//static uint8_t Akm_SaveServo_Param(uint8_t *flag); //辅助类
static float Vel_SmoothControl(float now_speed,float targetSpeed,float step);

static int target_limit_int(int insert,int low,int high);
static int Slide_Mean_Filter(int data);
static uint8_t Deep_SelfCheck( u16 RATE );
uint8_t ValChangeCheck(const uint16_t rate,const short checkval,const uint8_t changeEva);
static uint8_t FlashParam_Save(uint8_t *flag);

#endif  

