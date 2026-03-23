/***********************************************
公司：东莞市微宏智能科技有限公司
品牌：WHEELTEC
官网：wheeltec.net
淘宝店铺：shop114407458.taobao.com
速卖通: https://minibalance.aliexpress.com/store/4455017
版本：V3.5
修改时间：2021-01-29

Company: WeiHong Co.Ltd
Brand: WHEELTEC
Website: wheeltec.net
Taobao shop: shop114407458.taobao.com
Aliexpress: https://minibalance.aliexpress.com/store/4455017
Version: V3.5
Update：2021-01-29

All rights reserved
***********************************************/

#include "system.h"

//系统相关变量
SYS_VAL_t SysVal;

void systemInit(void)
{
	//================= General Hardware Initialization Section =================//
	//================= 通用硬件初始化部分 =================//
    //Interrupt priority group setting
    //中断优先级分组设置
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    //Delay function initialization
    //延时函数初始化
    delay_init(168);
	
	//Prioritize initializing IIC and IMU C50C board distinguishes between new and old versions based on IMU model
	//优先初始化IIC与IMU.C50C板通过IMU型号区分新板与旧版
    //IIC initialization
    //IIC初始化
    I2C_GPIOInit();
	
	//系统相关软件参数初始化
	SYS_VAL_t_Init(&SysVal);
	
    //Serial port 1 initialization, communication baud rate 115200,
    //can be used to communicate with ROS terminal
    //串口1初始化，通信波特率115200，可用于与ROS端通信
    UART1_Init(115200);
	
	//如果IMU为MPU6050,则是旧版C50C
	if( MPU6050_DEFAULT_ADDRESS == MPU6050_getDeviceID() )
	{
		SysVal.HardWare_Ver = V1_0;
		
		//旧版C50C硬件初始化
		V1_0_LED_Init();
		V1_0_CAN1_Mode_Init(1,3,3,6,0);
		V1_0_MiniBalance_PWM_Init(16799,0);
		
		//Initialize the hardware interface to the PS2 controller
		//初始化与PS2手柄连接的硬件接口
		PS2_Init();
		
		//PS2软件参数初始化
		PS2_Key_Param_Init();
		
		//MPU6050 is initialized to read the vehicle's three-axis attitude,
		//three-axis angular velocity and three-axis acceleration information
		//MPU6050初始化，用于读取小车三轴角速度、三轴加速度信息
		MPU6050_initialize();
	}
	
	//如果IMU型号为ICM20948,则是新版C50C
	else if( REG_VAL_WIA == ICM20948_getDeviceID() )//读取ICM20948 id
	{
		SysVal.HardWare_Ver = V1_1;
		
		//Initialize the hardware interface connected to the LED lamp
		//初始化与LED灯连接的硬件接口
		RGB_LightStrip_Init();
		
		//Initialize the CAN communication interface
		//CAN通信接口初始化
		CAN1_Mode_Init(1,3,3,6,0);
		
		//Initialize motor speed control and, for controlling motor speed, PWM frequency 10kHz
		//初始化电机速度控制以及，用于控制电机速度，PWM频率10KHZ
		MiniBalance_PWM_Init(16799,0);  //高级定时器TIM8的频率为168M，满PWM为16799，频率=168M/((16799+1)*(0+1))=10k
	
		//MPU6050 is initialized to read the vehicle's three-axis attitude,
		//three-axis angular velocity and three-axis acceleration information
		//ICM20948初始化，用于读取小车三轴角速度、三轴加速度信息
		invMSInit();
		
		//USB PS2初始化
		MX_USB_HOST_Init();//创建usb手柄任务
	}
	else //无法识别的陀螺仪,复位系统
	{
		NVIC_SystemReset();
	}
	
    //Initialize the hardware interface connected to the buzzer
    //初始化与蜂鸣器连接的硬件接口
    Buzzer_Init();
    
    //Initialize the hardware interface connected to the enable switch
    //初始化与使能开关连接的硬件接口
    EnableKey_Init();

    //Initialize the hardware interface connected to the user's key
    //初始化与用户按键连接的硬件接口
    KEY_Init();
	
    //Initialize the hardware interface connected to the OLED display
    //初始化与OLED显示屏连接的硬件接口
    OLED_Init();

	//TODO:恢复
    //Serial port 4 initialization, communication baud rate 9600,
    //used to communicate with Bluetooth APP terminal
    //串口4初始化，通信波特率9600，用于与蓝牙APP端通信
	#if VECT_TAB_OFFSET == 0x10000 //根据是否配置无线烧录来选择蓝牙的波特率
		UART4_Init(230400);
	#elif VECT_TAB_OFFSET == 0
		UART4_Init(9600);
	#endif
	
    //Serial port 3 is initialized and the baud rate is 115200.
    //Serial port 3 is the default port used to communicate with ROS terminal
    //串口3初始化，通信波特率115200，串口3为默认用于与ROS端通信的串口
    UART3_Init(115200);

    //Initialize the model remote control interface
    //初始化航模遥控接口
    Remoter_Init();
	
    //Encoder A is initialized to read the real time speed of motor A
    //编码器A初始化，用于读取电机A的实时速度
    EncoderA_Init();
    //Encoder B is initialized to read the real time speed of motor B
    //编码器B初始化，用于读取电机B的实时速度
    EncoderB_Init();
	
    //ADC pin initialization, used to read the battery voltage and potentiometer gear,
    //potentiometer gear determines the car after the boot of the car model
    //ADC引脚初始化，用于读取电池电压与电位器档位，电位器档位决定小车开机后的小车适配型号
    ADC1_Init();
	
	//阿克曼车型使用ADC2,不使用编码器C、D;其他车型反之
	#if defined AKM_CAR
		ADC2_Init();
	#else
		//Encoder C is initialized to read the real time speed of motor C  
		//编码器C初始化，用于读取电机C的实时速度	
		EncoderC_Init();
		//Encoder D is initialized to read the real time speed of motor D
		//编码器D初始化，用于读取电机D的实时速度	
		EncoderD_Init();  
	#endif
	
	//================= 软件参数初始化部分 =================//
	
	//确定机器人型号,初始化机器人机械参数和PID参数.
	Robot_Select(); 
	
	//机器人控制相关变量初始化,包含遥控速度基准、最大速度限制、速度平滑系数等内容.
	ROBOT_CONTROL_t_Init(&robot_control); 
	
	//4个PI控制器初始化
	PI_Controller_Init(&PI_MotorA,robot.V_KP,robot.V_KI);
	PI_Controller_Init(&PI_MotorB,robot.V_KP,robot.V_KI);
	PI_Controller_Init(&PI_MotorC,robot.V_KP,robot.V_KI);
	PI_Controller_Init(&PI_MotorD,robot.V_KP,robot.V_KI);
	
	//自动回充设备软件参数初始化
	auto_recharge_reset();
	
	//OLED软件参数初始化
	OLED_Param_Init(&oled);
	
	//APP软件参数初始化
	APPKey_Param_Init(&appkey);
	
	//航模遥控软件参数初始化
	Remoter_Param_Init(&remoter);
	
	//舵机参数初始化
	Akm_ServoParam_Init(&Akm_Servo);
	
	//从Flash读出舵机数据,若无数据则使用默认初始化值
	FlashParam_Read();    
	
	//阿克曼车型对舵机初始化
	#if defined AKM_CAR
	
	//高配阿克曼车型舵机初始化
	Servo_Senior_Init(10000-1,168-1,Akm_Servo.Mid);
	robot.SERVO.Output = Akm_Servo.Mid;
	
	//顶配阿克曼车型舵机初始化
	if( robot.type >= 2 && robot.type!= 9 )
	{
		//等待DMA采集数据
		delay_ms(200);
		
		//读取一组滑轨数据,估测舵机的位置.再将估测的位置作为PWM值初始化,可避免舵机突然快速归位.
		short TmpPWM = get_ServoPWM( get_DMA_SlideRes() );
		
		//顶配阿克曼车型舵机初始化,加入偏差值,避免舵机快速复位
		Servo_Top_Init(10000-1,84-1, TmpPWM );
		
		//舵机PI控制器初始化.注：舵机的PID参数不开放修改.
		PI_Controller_Init(&PI_Servo,0,0);
		
		//设置舵机PI控制基准值,加入偏差值,避免刚进入PI控制时舵机抖动
		PI_Servo.Output =  TmpPWM;
		
		//舵机速度平滑值
		robot_control.smooth_Servo = TmpPWM;
		
		//设置低速舵机模式,让舵机缓慢归位
		robot_control.ServoLow_flag = 1;
	}
	

	#endif
	
	//所有软硬件设备初始化完毕,使用蜂鸣器提示进入rtos
	Buzzer_AddTask(1,100);//蜂鸣1次,时间1000ms
}


