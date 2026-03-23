#include "balance_task.h"

//与小车控制相关
ROBOT_CONTROL_t robot_control;

//与小车自检相关变量
ROBOT_SELFCHECK_t robot_check;

//机器人驻车模式相关变量
static ROBOT_PARKING_t park;

//增量式PI控制器
PI_CONTROLLER PI_MotorA,PI_MotorB,PI_MotorC,PI_MotorD,PI_Servo;

uint8_t g_usbhost_connect=0;
int PengZhuang=0;

#if defined AKM_CAR
AKM_SERVO_UNLOCK_t ServoState;//舵机非自锁控制器
#endif
/**************************************************************************
Function: FreerTOS task, core motion control task
Input   : none
Output  : none
函数功能：FreeRTOS任务，核心运动控制任务
入口参数：无
返回  值：无
**************************************************************************/

void Balance_task(void *pvParameters)
{
    u32 lastWakeTime = getSysTickCnt();
	
    while(1)
    {
        // This task runs at a frequency of 100Hz (10ms control once)
        //此任务以100Hz的频率运行（10ms控制一次）
        vTaskDelayUntil(&lastWakeTime, F2T(BALANCE_TASK_RATE));

        //Time count is no longer needed after 30 seconds
        //时间计数，30秒后不再需要
        if(SysVal.Time_count<3000) SysVal.Time_count++;
		
        //and convert to transposition international units
        //获取编码器数据反馈、舵机数据反馈
        Get_Robot_FeedBack();
		
		//自动回充设备、状态、航模遥控器等信号监测
		Charger_DevCheck();
		
        //Function to Scan User Key Press Status
        //扫描用户按键状态
        UserKey_Scan( BALANCE_TASK_RATE );
		
		//Robot parking status monitoring, clearing residual motor control signals. \
		  Reduce power consumption, reduce motor noise.
		//机器人驻车状态监测,清除残余电机控制量.降低功耗,降低电机噪音.
		Robot_ParkingCheck();
		
		//机器人上电自检
        //等陀螺仪初始化完成后,检测机器人型号是否选择错误
        //When the gyroscope is initialized, check whether the robot model is selected incorrectly
        if(CONTROL_DELAY<SysVal.Time_count && SysVal.Time_count<CONTROL_DELAY+200)
        {
            Drive_Motor(0.2f,0,0);//低速前进测试
			//TODO:自检需要测试各种车型自检
            robot_mode_check();  //Detection function //检测函数
        }
        else if(CONTROL_DELAY+200<SysVal.Time_count && SysVal.Time_count<CONTROL_DELAY+230)
        {
			robot_check.check_end = 1;
            Drive_Motor(0,0,0); //The stop forward control is completed //检测完成停止前进控制
			SysVal.LED_delay = 500;
        }
		
		//After the self-check is completed, obtain the control commands for the robot.
        //自检结束后，获取机器人的控制命令
        if(SysVal.Time_count>CONTROL_DELAY+230)
        {
			if( 0 == SysVal.SecurityLevel )//0为最高安全等级,带有机器人控制命令丢失保护
			{
				//TODO:监测丢失,不同控制方式,监测丢失的频率不同,以实测生效为准.
				//uart,ros,can,usb ps2,安卓app 可用,苹果app无法使用,航模需要其他实现
				//TODO:安全等级只对uart,ros,can生效. ps2，app，航模信号丢失需要无条件停止
				robot_control.command_lostcount++;
				if( robot_control.command_lostcount>BALANCE_TASK_RATE )
					robot_control.Vx = 0 , robot_control.Vy = 0 , robot_control.Vz = 0;
			}
			
			//自动回充模式下的控制命令.
			if(charger.AllowRecharge==1)
			{	
				#if defined AKM_CAR //阿克曼车型回充独立逻辑
					if( charger.NavWalk ) Drive_Motor(charger.Up_MoveX,charger.Up_MoveY,charger.Up_MoveZ);
					else
					{	
						if( charger.RED_STATE!=0 ) Drive_Motor(charger.Red_MoveX,charger.Red_MoveY,charger.Red_MoveZ); 
						else Drive_Motor(0,0,0); 
					}
				#else //非阿克曼车型共用回充逻辑
					//如果开启了导航回充，同时没有接收到红外信号，接收来自上位机的的回充控制命令
					if      (charger.NavWalk==1 && charger.RED_STATE==0) Drive_Motor(charger.Up_MoveX,charger.Up_MoveY,charger.Up_MoveZ); 
					//接收到了红外信号，接收来自回充装备的回充控制命令
					else if (charger.RED_STATE!=0) charger.NavWalk = 0,Drive_Motor(charger.Red_MoveX,charger.Red_MoveY,charger.Red_MoveZ); 
					//防止没有红外信号时小车运动
					if (charger.NavWalk==0&&charger.RED_STATE==0) Drive_Motor(0,0,0); 
				#endif

			}
			//正常模式控制命令
			else
			{
				if      ( Get_Control_Mode(_APP_Control) )    Get_APPcmd();      //Handle the APP remote commands //处理APP遥控命令
				else if ( Get_Control_Mode(_RC_Control)  )    Remote_Control();  //Handle model aircraft remote commands //处理航模遥控命令
				else if (Get_Control_Mode(_PS2_Control)  )    PS2_control();     //Handle PS2 controller commands //处理PS2手柄控制命令
				
				//CAN, Uart x control can directly get the 3 axis target speed, 
				//without additional processing
				//CAN、串口x 控制直接得到3轴目标速度，无须额外处理
				else    Drive_Motor(robot_control.Vx,robot_control.Vy,robot_control.Vz);
			}
        }
		
		//深度自检模式,由用户需要时进入
		if( 1 == robot_check.DeepCheck && SysVal.Time_count>CONTROL_DELAY+300)
		{
			if( 1 == Deep_SelfCheck( BALANCE_TASK_RATE ) )
			{
				robot_check.DeepCheck = 0;
			}
			continue;
		}
		//Robot Operation Status Inspection. Inspection Items: Check if the voltage is too low, \
		if there are any self-test errors, and if the emergency stop switch has been pressed.   \
		The robot will not be allowed to be controlled if any of these conditions are not met.
		//机器人运行状态检测.检测内容：电压是否过低、自检是否出错、急停开关是否被按下.不符合条件时将不允许机器人被控制
		robot_control.FlagStop = Turn_Off();
		
		#if defined AKM_CAR
			Servo_UnLock_Check(robot_control.FlagStop);//舵机非自锁模式监测
		#endif
		
		//机器人允许被控制
		if( 0 == robot_control.FlagStop )
		{
			//执行控制,根据车型、型号不同,进行不同的控制.
			ResponseControl();
		}
		else //失能控制.清空控制量.使用解轴的方式
		{
			UnResponseControl(UN_LOCK);
		}
		
		
		//保存FLash参数,内容包含阿克曼参数、纠偏系数等
		uint8_t flash_check = 0;
		flash_check = FlashParam_Save( &appkey.ParamSaveFlag ); //保存Flash参数
		if( 1 == flash_check ) Buzzer_AddTask(1,100);      //保存成功,正常蜂鸣提示1秒
		else if( 1 < flash_check ) Buzzer_AddTask(10,11); //保存失败,快速蜂鸣10声警告

    }
}

/*-------------------------------- Functions Related to PI Controller ------------------------------------*/
/*--------------------------------           PI 控制器相关函数          ------------------------------------*/
/**************************************************************************
Functionality: Set PID Control Parameters - Configures the proportional (Kp) and integral (Ki) parameters for PID control.
Input Parameters: Kp parameter, Ki parameter.
Return Value: None.
Author: WHEELTEC
函数功能：设置PID控制的参数
入口参数：Kp参数、Ki参数
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void PI_SetParam(PI_CONTROLLER* p,int kp,int ki)
{
	//PID参数不可为负,也不可为0.
	if( kp > 0 ) 
	{
		p->kp = kp;
	}
	if( ki > 0 ) 
	{
		p->ki = ki;
	}
}

void Set_Robot_PI_Param(int kp,int ki)
{
	if( kp > 0 ) robot.V_KP = kp;
	if( ki > 0 ) robot.V_KI = ki;
	
	//设置四路pi参数
	PI_SetParam(&PI_MotorA,robot.V_KP,robot.V_KI);
	PI_SetParam(&PI_MotorB,robot.V_KP,robot.V_KI);
	PI_SetParam(&PI_MotorC,robot.V_KP,robot.V_KI);
	PI_SetParam(&PI_MotorD,robot.V_KP,robot.V_KI);
}

/**************************************************************************
Functionality: Incremental PI Controller Reset - Resets an incremental PI (Proportional-Integral) controller to its initial state.
Input Parameters: PI controller.
Return Value: None.
Author: WHEELTEC
函数功能：增量式PI控制器复位
入口参数：PI控制器
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void PI_Controller_Reset(PI_CONTROLLER *p)
{
	p->Bias = 0;
	p->LastBias = 0;
	p->Output = 0;
}


/**************************************************************************
Functionality: Incremental PI Control - Implements the control logic for an 
               incremental PI (Proportional-Integral) controller using feedback and target values.
Input Parameters: PI controller, feedback value, target value.
Return Value: Control output result.
Author: WHEELTEC
函数功能：增量式PI控制
入口参数：PI控制器,反馈值,目标值
返回  值：控制量结果输出
作    者：WHEELTEC
**************************************************************************/
static int Incremental_MOTOR(PI_CONTROLLER* p,float current,float target)
{
	//计算偏差
	p->Bias = target - current;
	
	//计算输出
	p->Output += p->kp * ( p->Bias - p->LastBias ) + p->ki * p->Bias;
	
	//输出限幅
	if( p->Output >   FULL_DUTYCYCLE ) p->Output =   FULL_DUTYCYCLE;
	if( p->Output < - FULL_DUTYCYCLE ) p->Output = - FULL_DUTYCYCLE;
	
	//保存本次偏差
	p->LastBias = p->Bias;
	
	//输出
	return p->Output;
}

/**************************************************************************
Functionality: Incremental PI Control - Implements the control logic for an 
               incremental PI (Proportional-Integral) controller using feedback and target values.
Input Parameters: PI controller, feedback value, target value.
Return Value: Control output result.
Author: WHEELTEC
函数功能：增量式PI控制
入口参数：PI控制器,反馈值,目标值
返回  值：控制量结果输出
作    者：WHEELTEC
**************************************************************************/
#if defined AKM_CAR
static int Incremental_Servo(PI_CONTROLLER* p,float current,float target)
{
	//舵机独立PID参数,不开放用户修改.已调整为最佳值
	float servo_kp = 0;
	float servo_ki = 0;
	int8_t servo_dir = 1;
	
	//根据滑轨的行程、不同的车型 来确定PI参数
	if( robot.type==2 || robot.type==3 ) //顶配摆式阿克曼
	{
		//因机械结构原因,右转的行程比较长,此时需要加快转向速度
		if( target<0 || current<-200 ) servo_kp = 0.002f*1.3f , servo_ki = 0.006f*1.3f;
		else                           servo_kp = 0.002f , servo_ki = 0.006f;
	}
	else if( robot.type==4 || robot.type==5 ) //顶配独立阿克曼
	{
		servo_dir = -1;//舵机反装,执行方向有变化
		
		//因机械结构原因,右转的行程比较长,此时需要加快转向速度
		if( target>0 || current<200 )  servo_kp = 0.002f*1.3f , servo_ki = 0.006f*1.3f;
		else                           servo_kp = 0.002f , servo_ki = 0.006f;
	}
	
	//低速舵机模式
	static uint16_t low_speedMode = 0;
	if( robot_control.ServoLow_flag ) 
	{
		servo_kp = 0.001,servo_ki = 0.003;
		low_speedMode++;
		if( low_speedMode > BALANCE_TASK_RATE*2 ) //设置自动退出时间,2秒
		{
			low_speedMode = 0;
			robot_control.ServoLow_flag = 0;//自动退出低速模式
		}
	}
	else
		low_speedMode = 0;
	
	//计算偏差
	p->Bias = target - current ;
	
	//输出计算
	p->Output += (servo_kp*servo_dir) * ( p->Bias - p->LastBias ) + (servo_ki*servo_dir) * p->Bias;
	
	//输出限幅
	if( p->Output > Akm_Servo.Max )  p->Output = Akm_Servo.Max;
	if( p->Output < Akm_Servo.Min )  p->Output = Akm_Servo.Min;
	
	//保存本次偏差
	p->LastBias = p->Bias;
	
	//舵机非自锁模式实现.通过在判断条件加入标志位可禁用非自锁模式
	static uint16_t count = 0;
	if( robot_control.smooth_Servo == p->Output && robot_control.FlagStop==0 ) //仅在机器人允许控制时使用本逻辑
	{
		count++;
		if( count>=BALANCE_TASK_RATE )
		{
			count=0;
			ServoState.UnLock = 1;           //进入不自锁模式
			ServoState.UnLock_Pos =  current;//并记录舵机当前的位置
			ServoState.UnLock_Target = target - Akm_Servo.Bias;//记录舵机的目标位置
			ServoState.UnLock_Output = robot.SERVO.Output;//记录舵机当前的pwm值
			return 0;//舵机到达目标角度后,不自锁
		}
	}
	else count = 0;
	
	#if 1
	//舵机平滑控制,降低加速度
	//让舵机值缓慢增加到的计算的目标值
	if( robot_control.smooth_Servo > p->Output ) 
	{
		robot_control.smooth_Servo -= robot_control.smooth_ServoStep;
		if( robot_control.smooth_Servo <= p->Output ) robot_control.smooth_Servo = p->Output;
	}
	else if( robot_control.smooth_Servo < p->Output )
	{
		robot_control.smooth_Servo += robot_control.smooth_ServoStep;
		if( robot_control.smooth_Servo >= p->Output  ) robot_control.smooth_Servo = p->Output;
	}
	else
	{
		robot_control.smooth_Servo = p->Output;
	}
	
	return (int)robot_control.smooth_Servo;
	#else
		//不使用滤波直接输出
		return p->Output;
	#endif
}



/**************************************************************************
Function function: Determine the PWM value of the servo motor based on the position of the slide rail,
                   only available for top of the line models
Entrance parameters: servo encoder reading
Return value: Servo PWM
Author: WHEELTEC
函数功能：通过滑轨位置判断舵机PWM值,仅顶配车型可用
入口参数：舵机编码器读数
返回  值：舵机PWM
作    者：WHEELTEC
**************************************************************************/
short get_ServoPWM(short TmpPos)
{
	//公式拟合版本,效果好,但每台车公式不一样，客户可以自行拟合
//	//估测舵机的位置与舵机中值的偏差
//	//顶配摆式与顶配独立悬挂舵机方向相反.
//	if( robot.type == 2 || robot.type == 3 )
//	{
//		//通过滑轨的位置猜测舵机的PWM数值大小.公式由采样几组数据进行拟合得出
//		return 1.57e+03 + 4.04e-01*TmpPos + (-5.82e-05)*pow(TmpPos,2);
//	}
//	else if( robot.type == 4 || robot.type == 5 )
//	{
//		//通过滑轨的位置猜测舵机的PWM数值大小.公式由采样几组数据进行拟合得出
////		return 1.46e+03 + (-4.05e-01)*TmpPos + (-9.42e-07)*pow(TmpPos,2);
//		return 1.48e+03 + (-2.39e-01)*TmpPos + (4.90e-06)*pow(TmpPos,2);	
//	}
//	else
//		return 0;
	
	uint16_t pwm_val = Akm_Servo.Mid;
	
	//估测版本
	if( robot.type==4||robot.type==5 )
	{
			 if( TmpPos>=500&&TmpPos<=1000 )    pwm_val = Akm_Servo.Mid - 250;
		else if( TmpPos>1000 )                  pwm_val = Akm_Servo.Mid - 450;
		else if( TmpPos<=-500&&TmpPos>=-1000 )   pwm_val = Akm_Servo.Mid + 250;
		else if( TmpPos<-1000 )                   pwm_val = Akm_Servo.Mid + 450;
	}
	else if( robot.type==2 || robot.type==3 )
	{
			 if( TmpPos>=500&&TmpPos<=1000 )    pwm_val = Akm_Servo.Mid + 250;
		else if( TmpPos>1000 )                  pwm_val = Akm_Servo.Mid + 450;
		else if( TmpPos<=-500&&TmpPos>=-1000 )   pwm_val = Akm_Servo.Mid - 250;
		else if( TmpPos<-1000 )                   pwm_val = Akm_Servo.Mid - 450;
	}
	
	return pwm_val;
}

/**************************************************************************
Function Function: Servo Calibration Function. When the servo is in non self-locking mode and 
the position of the servo changes due to external factors, this function can be used for calibration
Entrance parameters: None
Return value: None
Author: SHEELTEC
函数功能：舵机校正函数.当舵机在非自锁模式下时,因为外界因素导致舵机位置发生变化,使用该函数校准
入口参数：无
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void ServoCalibration(void)
{
	short tmp_pwm;
	tmp_pwm = get_ServoPWM( robot.SERVO.Encoder ); //获取当前位置对应PWM数值
	SERVO_TOP = tmp_pwm;                  //初始化舵机的位置,该变量直接决定舵机的位置
	PI_Controller_Reset(&PI_Servo);       //复位舵机的PI控制器
	PI_Servo.Output = tmp_pwm;            //PI控制器的输出值,将会以该数值作为基础进行速度平滑.
	robot_control.smooth_Servo = tmp_pwm; //平滑值,这是实际的控制量.
}

//舵机非自锁模式监测
static void Servo_UnLock_Check(uint8_t car_stopflag)
{
	static uint8_t now = 0 ;
	static uint8_t last = 0;
	
	if(SysVal.Time_count < CONTROL_DELAY ) return;
	
	//舵机非自锁模式相关
	if ( robot.type>=2 && robot.type!=9 ) //仅顶配车型
	{
		//小车使能标志位检测
		now = car_stopflag;
		if( now==1 && last==0 ) //小车被失能
		{
			ServoState.UnLock_Pos = robot.SERVO.Encoder;//记录失能时舵机的位置
			ServoState.UnLock_Output = robot.SERVO.Output;//舵机PWM值
		}
		else if( now==0 && last==1 ) //小车被使能
		{
			if( fabs( ServoState.UnLock_Pos - robot.SERVO.Encoder ) > 100 ) //在失能期间,舵机位置被外界改变了,则重新矫正
			{
				ServoState.UnLock = 0;
				ServoCalibration();//矫正舵机位置
				robot_control.ServoLow_flag = 1;//进入舵机低速模式,保证舵机缓慢复位(仅顶配阿克曼)
			}
		}
		last = now;
		
		//非自锁模式退出检测
		if( 1 == ServoState.UnLock )
		{
			//在非自锁期间,舵机角度被外接改变,需要进行校准.
			if( fabs( robot.SERVO.Encoder - ServoState.UnLock_Pos ) > 100 ) ServoState.wait_Calib = 1;
			
			//舵机有控制量时,解除非自锁,进入正常控制模式
			if( fabs( robot.SERVO.Target - ServoState.UnLock_Target ) > 50 || 1 == ValChangeCheck(BALANCE_TASK_RATE,Akm_Servo.Bias,3)) 
			{
				if( ServoState.wait_Calib ) 
				{
					ServoState.wait_Calib=0,ServoCalibration();//如有需要则先校准舵机
					if( fabs(robot_control.smooth_Vx) < 0.001f ) robot_control.ServoLow_flag = 1;//如果小车不在行驶中，在矫正时使用低速矫正
				}
				ServoState.UnLock = 0;//解除非自锁模式
			}		
			
		} 
	}//车型判断end
}
#endif
/**************************************************************************
Functionality: PI Controller Clearing Function - Clears the output residue of a PI (Proportional-Integral) 
               controller when the robot is disabled, aiming to reduce power consumption and motor noise.
Input Parameters: PI controller.
Return Value: None.
Author: WHEELTEC
函数功能：PI控制器清除函数,在机器人禁止时清空输出残余，降低功耗以及电机的噪音
入口参数：PI控制器
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static uint8_t PI_Clear_Output(PI_CONTROLLER* p)
{
	u8 state = 0;//清除标志位
	
	if( p->Output > 0 ) p->Output -- ;
	if( p->Output < 0 ) p->Output ++ ;
	
	//接近0时,直接置0
	if( p->Output < 2.0f && p->Output > -2.0f ) p->Output = 0 , state = 1;
	
	//返回清除结果. 0:未完成 1:清除完毕
	return state;
}

/*-------------------------------- Function for Inverse Kinematics Solution ------------------------------------*/
/*--------------------------------         各车型运动学逆解相关函数          ------------------------------------*/
#if defined AKM_CAR

//输入滑块位置，计算出当前的左前轮转角
float SteeringStructure_ForwardKinematic(short ori_pos)
{
    float pos;
    float tmp_theta;
    float tmp_C;

    pos = (float)ori_pos*0.0244f + 103;

    tmp_C = ( -1500.5f - pow(pos,2) ) / ( 140.4f*sqrt( pow(pos,2) + 3340.8f ) );

    tmp_theta = atan2(pos,57.8f) - asin( tmp_C ) - PI;

    return tmp_theta/PI * 180.0f + 72.39f;
}

//输入左前轮转角,输出滑轨电位器的目标值
static int SteeringStructure_Reverse(float target_theta)
{
	float theta;
	float pos;
	
	//顶配摆式阿克曼
	if( 2 == robot.type || 3 == robot.type )
	{
		theta = target_theta - 1.26f;   //6768.4f
		pos = 70.2f * cos(theta) + sqrt( 6084.0f - pow((70.2f*sin(theta)+57.8f),2) ) - 103;
		pos /= 0.0244f;
		
		//电位器的输出需要保证在可达范围.
		pos = target_limit_float(pos,-2000,2000);
	}
	
	//顶配独立阿克曼
	else if( 4 == robot.type || 5 == robot.type )
	{
		theta = target_theta - 1.170f;    //24831.4f
		pos = 95.15f * cos(theta) + sqrt( 24025.0f - pow((95.15f*sin(theta)+56.5f),2) ) - 191.54f;
		pos /= 0.0244f;
		
		//电位器的输出需要保证在可达范围.
		pos = target_limit_float(pos,-2000,2000);
	}

	return (int)pos;

}

/**************************************************************************
Function function: Ackermann inverse kinematics solution
Entrance parameters: X-axis velocity (in m/s), left front wheel angle (in rad)
Return value: None
Author: WHEELTEC
函数功能：阿克曼运动学逆解
入口参数：X轴的速度(单位m/s)，左前轮的转角(单位rad)
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void InverseKinematics_akm(float Vx,float Vz) 
{
	//运动学分析前,优先采集舵机的电位器校准值,以便运动学分析结果调用
	Akm_Servo.Bias = -get_DMA_ServoBias() / 5.0f;
	
	//非标定制车型,请根据舵机的类型不同来复制对应的运动学分析内容.
	if( robot.type==6 )
	{
	  
	}
	//SENIOR_AKM - 高配阿克曼 
	else if(robot.type==0||robot.type==1||robot.type==9) 
	{
		//Ackerman car specific related variables //阿克曼小车专用相关变量
		float TurnR=0, Left_Angle=0;
		
		// For Ackerman small car, Vz represents the front wheel steering Angle
		//对于阿克曼小车Vz代表右前轮转向角度
		Left_Angle = Vz;
		
		//限制左右转向的角度
		const float limt_angle = 30.0f;
		//左极限的转弯半径
		float bd_turnR = robot.HardwareParam.AxleSpacing/tan(angle_to_rad(limt_angle)) + 0.5f*robot.HardwareParam.WheelSpacing;
		//根据极限转弯半径,计算往右转时候的左轮角度
		float bd_rightAngle = atan(robot.HardwareParam.AxleSpacing/((-bd_turnR)-0.5f*robot.HardwareParam.WheelSpacing));
		
		// Front wheel steering Angle limit (front wheel steering Angle controlled by steering engine), unit: rad
		//前轮转向角度限幅(舵机控制前轮转向角度)，单位：rad
		Left_Angle=target_limit_float(Left_Angle,bd_rightAngle,angle_to_rad(limt_angle));
		
		//根据转向角度计算出当前的转弯半径
		if(Left_Angle!=0)
			TurnR = robot.HardwareParam.AxleSpacing/tan(Left_Angle) + 0.5f*robot.HardwareParam.WheelSpacing;
		
		//根据转弯半径、轮距,逆解出左右轮的速度
		//Inverse kinematics //运动学逆解
		if(Left_Angle!=0)
		{	//左右两轮目标速度,单位是m/s
			robot.MOTOR_A.Target = Vx*(TurnR-0.5f*robot.HardwareParam.WheelSpacing)/TurnR;
			robot.MOTOR_B.Target = Vx*(TurnR+0.5f*robot.HardwareParam.WheelSpacing)/TurnR;
		}
		else //不带转向的控制
		{	
			robot.MOTOR_A.Target = Vx;
			robot.MOTOR_B.Target = Vx;
		}
		
		// The PWM value of the servo controls the steering Angle of the front wheel
		//舵机PWM值，舵机控制前轮转向角度
		//前轮转角与舵机数值的对应关系
//		robot.SERVO.Target    =  7.3f + 823.4f*Left_Angle + (-440.4f)*pow(Left_Angle,2) + (-28.8)*pow(Left_Angle,3);
		robot.SERVO.Target    =  7.85f + 847.0f*Left_Angle + (-394.0f)*pow(Left_Angle,2) + (-74.2f)*pow(Left_Angle,3);
		
		//舵机最终输出 = 中值 + 电位器纠正值 + 计算值
		robot.SERVO.Output = Akm_Servo.Mid + Akm_Servo.Bias + robot.SERVO.Target;
	}
	
	//TOP_AKM_BS - 顶配阿克曼小车摆式悬挂
	else if( robot.type==2 || robot.type==3 )
	{
		//转弯半径、左轮角度
		float TurnR = 0, Left_Angle = 0;
		
		//对于阿克曼小车Vz代表左前轮转向角度
		Left_Angle = Vz;
		
		//限制左右转向的角度
		const float limt_angle = 25.0f;
		
		//计算角度时的连杆距离K
		const float K = 0.326f;
		
		//左极限的转弯半径
		float bd_turnR = robot.HardwareParam.AxleSpacing/tan(angle_to_rad(limt_angle)) + 0.5f*K;
		
		//根据极限转弯半径,计算往右转时候的左轮角度
		float bd_rightAngle = atan(robot.HardwareParam.AxleSpacing/((-bd_turnR)-0.5f*K));
		
		//前轮转向角度限幅(舵机控制前轮转向角度)，单位：rad
		Left_Angle=target_limit_float(Left_Angle,bd_rightAngle,angle_to_rad(limt_angle));//注: 左+ 右-
		
		//滑轨目标值计算：
		robot.SERVO.Target = SteeringStructure_Reverse(Left_Angle);
		//robot.SERVO.Target+=Akm_Servo.Bias;	//加入偏差值,实现前轮转角可用电位器调节补偿
		
		//根据转向角度计算出当前的转弯半径
		if(Left_Angle!=0)
			TurnR = robot.HardwareParam.AxleSpacing/tan(Left_Angle) + 0.5f*K;
		
		//左右轮目标值计算：
		//根据转弯半径、轮距,逆解出左右轮的速度
		//Inverse kinematics //运动学逆解
		if(Left_Angle!=0)
		{
			robot.MOTOR_A.Target = Vx*(TurnR-0.5f*robot.HardwareParam.WheelSpacing)/TurnR;
			robot.MOTOR_B.Target = Vx*(TurnR+0.5f*robot.HardwareParam.WheelSpacing)/TurnR;
		}
		else //不带转向的控制
		{
			robot.MOTOR_A.Target = Vx;
			robot.MOTOR_B.Target = Vx;
		}
		
	}
	
	else if ( robot.type==4 || robot.type==5 )
	{
		//转弯半径、左轮角度
		float TurnR = 0, Left_Angle = 0;
		
		//对于阿克曼小车Vz代表左前轮转向角度
		Left_Angle = Vz;
		
		//限制左右转向的角度
		const float limt_angle = 25.0f;
		
		//计算角度时的连杆距离K
		const float K = 0.441f;
		
		//左极限的转弯半径
		float bd_turnR = robot.HardwareParam.AxleSpacing/tan(angle_to_rad(limt_angle)) + 0.5f*K;
		
		//根据极限转弯半径,计算往右转时候的左轮角度
		float bd_rightAngle = atan(robot.HardwareParam.AxleSpacing/((-bd_turnR)-0.5f*K));
		
		//前轮转向角度限幅(舵机控制前轮转向角度)，单位：rad
		Left_Angle=target_limit_float(Left_Angle,bd_rightAngle,angle_to_rad(limt_angle));//注: 左+ 右-
		
		//滑轨目标值计算：
		robot.SERVO.Target = SteeringStructure_Reverse(Left_Angle);
		
		//根据转向角度计算出当前的转弯半径
		if(Left_Angle!=0)
			TurnR = robot.HardwareParam.AxleSpacing/tan(Left_Angle) + 0.5f*K;
		
		//左右轮目标值计算：
		//根据转弯半径、轮距,逆解出左右轮的速度
		//Inverse kinematics //运动学逆解
		if(Left_Angle!=0)
		{
			robot.MOTOR_A.Target = Vx*(TurnR-0.5f*robot.HardwareParam.WheelSpacing)/TurnR;
			robot.MOTOR_B.Target = Vx*(TurnR+0.5f*robot.HardwareParam.WheelSpacing)/TurnR;
		}
		else //不带转向的控制
		{
			robot.MOTOR_A.Target = Vx;
			robot.MOTOR_B.Target = Vx;
		}
	}
	else if( robot.type == 7 )
	{
		if( Vz > 0 ) robot.SERVO.Output = 1800;
		else if( Vz < 0 ) robot.SERVO.Output = 1100;
		else robot.SERVO.Output = 1500;
	}
}

/**************************************************************************
Function function: Convert the target forward velocity Vx and target angular velocity Vz sent by
                the upper computer into the left front wheel steering angle of the Ackermann car
Entrance parameters: target forward velocity Vx, target angular velocity Vz, unit: m/s, rad/s
Return value: Left front wheel steering angle of Ackermann car, unit: rad
函数功能：将上位机发过来目标前进速度Vx、目标角速度Vz，转换为阿克曼小车的左前轮转角
入口参数：目标前进速度Vx、目标角速度Vz，单位：m/s，rad/s
返回  值：阿克曼小车的左前轮转角，单位：rad
**************************************************************************/
float Akm_Vz_to_Angle(float Vx,float Vz)
{
    float TurnR, Angle_Left;//转弯半径,左前轮角度

    if(Vz!=0 && Vx!=0)
    {
		//计算转弯半径
		TurnR = Vx/Vz;
		
        //确保转弯半径不会超出运动学公式的计算范围内
        if( TurnR > 0 && TurnR <= 0.5f*robot.HardwareParam.WheelSpacing ) TurnR = 0.5f*robot.HardwareParam.WheelSpacing+0.01f;
        if( TurnR < 0 && TurnR >= -0.5f*robot.HardwareParam.WheelSpacing ) TurnR = -0.5f*robot.HardwareParam.WheelSpacing-0.01f;
		
		//根据转弯半径确定左前轮转角弧度
		Angle_Left=atan(robot.HardwareParam.AxleSpacing/(TurnR - 0.5f*robot.HardwareParam.WheelSpacing));

    }
    else
    {
        Angle_Left=0;
    }

    return Angle_Left;
}

/**************************************************************************
Function function: Differential robot inverse kinematics solution
Entrance parameters: X-axis velocity (in m/s), Z-axis velocity (in rad/s)
Return value: None
Author: WHEELTEC
函数功能：差速机器人运动学逆解
入口参数：X轴速度(单位m/s)、Z轴速度(单位 rad/s)
返回  值：无
作    者：WHEELTEC
**************************************************************************/
#elif defined DIFF_CAR
static void InverseKinematics_diff(float Vx,float Vz)
{
	robot.MOTOR_A.Target = Vx - Vz * robot.HardwareParam.WheelSpacing / 2.0f;
	robot.MOTOR_B.Target = Vx + Vz * robot.HardwareParam.WheelSpacing / 2.0f;
}

/**************************************************************************
Function function: inverse kinematics solution of the wheat wheel robot
Entrance parameters: X-axis velocity (in m/s), Y-axis velocity (in m/s), Z-axis velocity (in rad/s)
Return value: None
Author: WHEELTEC
函数功能：麦轮机器人运动学逆解
入口参数：X轴速度(单位m/s)、Y轴速度(m/s)、Z轴速度(rad/s)
返回  值：无
作    者：WHEELTEC
**************************************************************************/
#elif defined MEC_CAR
static void InverseKinematics_mec(float Vx,float Vy,float Vz)
{
    robot.MOTOR_A.Target = +Vy+Vx-Vz*(robot.HardwareParam.AxleSpacing+robot.HardwareParam.WheelSpacing);
    robot.MOTOR_B.Target = -Vy+Vx-Vz*(robot.HardwareParam.AxleSpacing+robot.HardwareParam.WheelSpacing);
    robot.MOTOR_C.Target = +Vy+Vx+Vz*(robot.HardwareParam.AxleSpacing+robot.HardwareParam.WheelSpacing);
    robot.MOTOR_D.Target = -Vy+Vx+Vz*(robot.HardwareParam.AxleSpacing+robot.HardwareParam.WheelSpacing);
}

/**************************************************************************
Function function: inverse kinematics solution of four-wheel drive robot
Entrance parameters: X-axis velocity (in m/s), Y-axis velocity (in m/s), Z-axis velocity (in rad/s)
Return value: None
Author  :WHEELTEC
函数功能：四驱机器人运动学逆解
入口参数：X轴速度(单位m/s)、Y轴速度(m/s)、Z轴速度(rad/s)
返回  值：无
作    者：WHEELTEC
**************************************************************************/
#elif defined _4WD_CAR
static void InverseKinematics_4wd(float Vx,float Vz)
{
    robot.MOTOR_A.Target = Vx-Vz*(robot.HardwareParam.AxleSpacing+robot.HardwareParam.WheelSpacing);
    robot.MOTOR_B.Target = Vx-Vz*(robot.HardwareParam.AxleSpacing+robot.HardwareParam.WheelSpacing);
    robot.MOTOR_C.Target = Vx+Vz*(robot.HardwareParam.AxleSpacing+robot.HardwareParam.WheelSpacing);
    robot.MOTOR_D.Target = Vx+Vz*(robot.HardwareParam.AxleSpacing+robot.HardwareParam.WheelSpacing);
}

/**************************************************************************
Function function: Inverse kinematics solution for omnidirectional wheeled robots
Entrance parameters: X-axis velocity (in m/s), Y-axis velocity (in m/s), Z-axis velocity (in rad/s)
Return value: None
Author: WHEELTEC
函数功能：全向轮机器人运动学逆解
入口参数：X轴速度(单位m/s)、Y轴速度(m/s)、Z轴速度(rad/s)
返回  值：无
作    者：WHEELTEC
**************************************************************************/
#elif defined OMNI_CAR
static void InverseKinematics_omni(float Vx,float Vy,float Vz)
{
    robot.MOTOR_A.Target =  Vy + robot.HardwareParam.TurnRadiaus*Vz;
    robot.MOTOR_B.Target = -robot.HardwareParam.X_PARAMETER*Vx - robot.HardwareParam.Y_PARAMETER*Vy + robot.HardwareParam.TurnRadiaus*Vz;
    robot.MOTOR_C.Target = +robot.HardwareParam.X_PARAMETER*Vx - robot.HardwareParam.Y_PARAMETER*Vy + robot.HardwareParam.TurnRadiaus*Vz;
}

#endif

/*-------------------------------- Control class related functions ------------------------------------*/
/*--------------------------------        控制类相关函数          ------------------------------------*/
//计算纠偏系数
static float wheelCoefficient(uint32_t diffparam,uint8_t isLeftWheel)
{
	if( 1 == isLeftWheel ) //左轮纠偏,对应50~100对应1.0~1.2倍的纠偏系数
	{
		if( diffparam>=50 )
			return 1.0f + 0.004f*(diffparam-50);
	}
	else //右轮纠偏,50~0对应1.0~1.2倍的纠偏系数
	{
		if( diffparam<=50 )
			return 1.0f + 0.004f*(50-diffparam);
	}
	
	return 1.0f;//不满足条件时,默认是1.
}

/**************************************************************************
Function: The inverse kinematics solution is used to calculate the target speed of each wheel according to the target speed of three axes
Input   : X and Y, Z axis direction of the target movement speed
Output  : none
函数功能：执行运动学逆解函数，根据三轴目标速度计算各车轮目标转速
入口参数：X、Y、Z三轴速度
返回  值：无
**************************************************************************/
static void Drive_Motor(float T_Vx,float T_Vy,float T_Vz)
{
	// Function to Limit Maximum Target Speed Input 
	//对输入的目标速度进行最大值限幅,单位m/s
	T_Vx = target_limit_float( T_Vx, -robot_control.limt_max_speed , robot_control.limt_max_speed );
	T_Vy = target_limit_float( T_Vy, -robot_control.limt_max_speed , robot_control.limt_max_speed );
	T_Vz = target_limit_float( T_Vz, -robot_control.limt_max_speed , robot_control.limt_max_speed );
	
	//Smoothly control the target speed input.
	//对输入的目标速度进行平滑控制
	if( charger.AllowRecharge==0 )
	{
		robot_control.smooth_Vx = Vel_SmoothControl(robot_control.smooth_Vx,T_Vx,robot_control.smooth_MotorStep);
		robot_control.smooth_Vy = Vel_SmoothControl(robot_control.smooth_Vy,T_Vy,robot_control.smooth_MotorStep);
		
		//阿克曼车型,Vz代表左前轮角度.这里不做平滑处理
		#if defined AKM_CAR
			robot_control.smooth_Vz = T_Vz;
		#else
			robot_control.smooth_Vz = Vel_SmoothControl(robot_control.smooth_Vz,T_Vz,robot_control.smooth_MotorStep);
		#endif	
	}
	else
	{
		robot_control.smooth_Vx = T_Vx;//自动回充模式下不使用平滑功能,提高小车的响应速度对接.
		robot_control.smooth_Vy = T_Vy;
		robot_control.smooth_Vz = T_Vz;
	}

	// Call Inverse Kinematics Function for Corresponding Vehicle Model to Obtain Target Values for Wheels and Steering Angle
	//调用对应车型的运动学逆解函数,获得各个轮子的目标值、舵机角度
	#if defined AKM_CAR
		InverseKinematics_akm(robot_control.smooth_Vx,robot_control.smooth_Vz);
	#elif defined DIFF_CAR
		InverseKinematics_diff(robot_control.smooth_Vx,robot_control.smooth_Vz);
	#elif defined MEC_CAR
		InverseKinematics_mec(robot_control.smooth_Vx,robot_control.smooth_Vy,robot_control.smooth_Vz);
	#elif defined _4WD_CAR
		InverseKinematics_4wd(robot_control.smooth_Vx,robot_control.smooth_Vz);
	#elif OMNI_CAR
		InverseKinematics_omni(robot_control.smooth_Vx,robot_control.smooth_Vy,robot_control.smooth_Vz);
	#endif

	//纠偏系数计算
	float LeftWheelDiff = wheelCoefficient(robot_control.LineDiffParam,1);
	float RightWheelDiff = wheelCoefficient(robot_control.LineDiffParam,0);
	
	// Limit Final Wheel Speeds or Servo Travel After Kinematic Analysis.
	//经过运动学分析后输出各个轮子速度与舵机角度,下面对最终的轮速或舵机行程进行限幅.
	robot.MOTOR_A.Target = target_limit_float( robot.MOTOR_A.Target , -robot_control.limt_max_speed , robot_control.limt_max_speed );
	robot.MOTOR_B.Target = target_limit_float( robot.MOTOR_B.Target , -robot_control.limt_max_speed , robot_control.limt_max_speed );
	
	#if defined AKM_CAR || defined DIFF_CAR
		robot.MOTOR_A.Target*=LeftWheelDiff;
		robot.MOTOR_B.Target*=RightWheelDiff;
	#elif defined MEC_CAR || defined _4WD_CAR
		robot.MOTOR_C.Target = target_limit_float( robot.MOTOR_C.Target , -robot_control.limt_max_speed , robot_control.limt_max_speed );
		robot.MOTOR_D.Target = target_limit_float( robot.MOTOR_D.Target , -robot_control.limt_max_speed , robot_control.limt_max_speed );	
		robot.MOTOR_A.Target*=LeftWheelDiff;
		robot.MOTOR_B.Target*=LeftWheelDiff;
		robot.MOTOR_C.Target*=RightWheelDiff;
		robot.MOTOR_D.Target*=RightWheelDiff;
	#elif defined OMNI_CAR
		robot.MOTOR_C.Target = target_limit_float( robot.MOTOR_C.Target , -robot_control.limt_max_speed , robot_control.limt_max_speed );	
		robot.MOTOR_B.Target*=LeftWheelDiff;
		robot.MOTOR_C.Target*=RightWheelDiff;
	#endif
}

/**************************************************************************
Function function: Control the corresponding function. After inverse kinematics solution, 
obtain the target values of each executing mechanism of the robot, and control the motion 
of the executing mechanism through PID control
Entrance parameters: None
Return value: None
Author: WHEELTEC
函数功能：控制相应函数.经过运动学逆解后得出机器人各个执行机构的目标值,通过PID控制执行机构运动
入口参数：无
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void ResponseControl(void)
{
	#if defined AKM_CAR
	//PI控制
	robot.MOTOR_A.Output = Incremental_MOTOR( &PI_MotorA , robot.MOTOR_A.Encoder , robot.MOTOR_A.Target );
	robot.MOTOR_B.Output = Incremental_MOTOR( &PI_MotorB , robot.MOTOR_B.Encoder , robot.MOTOR_B.Target );
	
	//The servo of the top of the line Ackermann model only requires PI control. \
	The high-end model does not come with steering rail feedback
	//仅顶配阿克曼车型的舵机需要PI控制.高配车型不带转向滑轨反馈.
	if( robot.type>=2 && robot.type!=9 && robot.type!=7 )
	{
		if(ServoState.UnLock==0)
			robot.SERVO.Output   = Incremental_Servo( &PI_Servo  , robot.SERVO.Encoder  ,  robot.SERVO.Target + Akm_Servo.Bias );	
	}
		
	//将结果输出到执行机构
	      if( robot.type == 6 )                      Set_Pwm(0,0,0,0,0); //非标预留定制车,请根据电机型号设置输出值的正负方向
	else if ( robot.type <= 1 || robot.type == 9)   Set_Pwm( robot.MOTOR_A.Output , robot.MOTOR_B.Output , 0 , 0 ,robot.SERVO.Output ); //高配阿克曼-MD36电机
	else if ( robot.type >= 2 && robot.type <= 5 )  Set_Pwm(-robot.MOTOR_A.Output ,-robot.MOTOR_B.Output , 0 , 0 ,robot.SERVO.Output ); //顶配阿克曼-MD60电机
	else if ( robot.type == 7 )                     Set_Pwm( 0 , 0 , 0 , 0 , robot.SERVO.Output ); //安装测试专用.舵机开环控制
	else if ( robot.type == 8 )                     Set_Pwm( 0 , 0 , 0 , 0 , 1500 ); //安装测试专用.让舵机保持在中间位置校准
	
	#elif defined DIFF_CAR
	
	robot.MOTOR_A.Output = Incremental_MOTOR(&PI_MotorA,robot.MOTOR_A.Encoder,robot.MOTOR_A.Target);
	robot.MOTOR_B.Output = Incremental_MOTOR(&PI_MotorB,robot.MOTOR_B.Encoder,robot.MOTOR_B.Target);
	
	if( robot.type<=1 )  Set_Pwm( robot.MOTOR_A.Output , robot.MOTOR_B.Output ,0,0,0);//高配差速-MD36电机
	else                 Set_Pwm(-robot.MOTOR_A.Output ,-robot.MOTOR_B.Output ,0,0,0);//顶配差速-MD60电机
	
	#elif defined MEC_CAR
	
	robot.MOTOR_A.Output = Incremental_MOTOR(&PI_MotorA,robot.MOTOR_A.Encoder,robot.MOTOR_A.Target);
	robot.MOTOR_B.Output = Incremental_MOTOR(&PI_MotorB,robot.MOTOR_B.Encoder,robot.MOTOR_B.Target);
	robot.MOTOR_C.Output = Incremental_MOTOR(&PI_MotorC,robot.MOTOR_C.Encoder,robot.MOTOR_C.Target);
	robot.MOTOR_D.Output = Incremental_MOTOR(&PI_MotorD,robot.MOTOR_D.Encoder,robot.MOTOR_D.Target);
	
	if( robot.type<=2 ) Set_Pwm( robot.MOTOR_A.Output, robot.MOTOR_B.Output, robot.MOTOR_C.Output, robot.MOTOR_D.Output,0);//高配麦轮-MD36电机
	else                Set_Pwm(-robot.MOTOR_A.Output,-robot.MOTOR_B.Output,-robot.MOTOR_C.Output,-robot.MOTOR_D.Output,0);//顶配或旗舰麦轮-MD60电机
	
	#elif defined _4WD_CAR
	
	robot.MOTOR_A.Output = Incremental_MOTOR(&PI_MotorA,robot.MOTOR_A.Encoder,robot.MOTOR_A.Target);
	robot.MOTOR_B.Output = Incremental_MOTOR(&PI_MotorB,robot.MOTOR_B.Encoder,robot.MOTOR_B.Target);
	robot.MOTOR_C.Output = Incremental_MOTOR(&PI_MotorC,robot.MOTOR_C.Encoder,robot.MOTOR_C.Target);
	robot.MOTOR_D.Output = Incremental_MOTOR(&PI_MotorD,robot.MOTOR_D.Encoder,robot.MOTOR_D.Target);
	
	if( robot.type<=3 ) Set_Pwm( robot.MOTOR_A.Output, robot.MOTOR_B.Output, robot.MOTOR_C.Output, robot.MOTOR_D.Output,0);//高配四驱-MD36电机
	else                Set_Pwm(-robot.MOTOR_A.Output,-robot.MOTOR_B.Output,-robot.MOTOR_C.Output,-robot.MOTOR_D.Output,0);//顶配或旗舰四驱-MD60电机
	
	#elif defined OMNI_CAR
	 
	robot.MOTOR_A.Output = Incremental_MOTOR(&PI_MotorA,robot.MOTOR_A.Encoder,robot.MOTOR_A.Target);
	robot.MOTOR_B.Output = Incremental_MOTOR(&PI_MotorB,robot.MOTOR_B.Encoder,robot.MOTOR_B.Target);
	robot.MOTOR_C.Output = Incremental_MOTOR(&PI_MotorC,robot.MOTOR_C.Encoder,robot.MOTOR_C.Target);
	
	if( robot.type<=1 ) Set_Pwm(-robot.MOTOR_A.Output,-robot.MOTOR_B.Output, robot.MOTOR_C.Output,0,0);//高配全向轮-MD36电机
	else                Set_Pwm( robot.MOTOR_A.Output, robot.MOTOR_B.Output,-robot.MOTOR_C.Output,0,0);//顶配全向轮-MD60电机

	#endif
}

/**************************************************************************
Functionality: Non-Control Response Function - Executes when the robot encounters an error, prohibiting the robot's movement.
Input Parameters: Set the robot's stop mode: 0: Set to unclamp axes, allowing the robot to be freely pushed. 
                  1: Set to clamp axes, locking all actuators.
Return Value: None
Author: WHEELTEC
函数功能：非控制响应函数,当机器人存在报错时,将执行此函数.禁止机器人的运动
入口参数：设置机器人停止运动的方式: 0:设置为解轴,机器人可以随意推动  1:设置为锁轴,机器人锁住所有执行机构
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void UnResponseControl(uint8_t mode)
{
	//数据有效性校验
	if(mode!=UN_LOCK && mode!=LOCK) mode = UN_LOCK;
	
	//解轴失能
	if(mode==UN_LOCK)
	{
		//复位PI控制器,消除积累
		PI_Controller_Reset(&PI_MotorA);
		PI_Controller_Reset(&PI_MotorB);
		PI_Controller_Reset(&PI_MotorC);
		PI_Controller_Reset(&PI_MotorD);
		
		//发送0停止电机的转动
		Set_Pwm( 0 , 0 , 0 , 0 , 0 );
	}
	
	//锁轴失能,无法推动小车
	else if( mode==LOCK )
	{
		Drive_Motor(0,0,0);
		ResponseControl();
	}
}

/**************************************************************************
Functionality: Send Control Values to Actuators - Distributes control values to each actuator (A, B, C, D wheels and servo).
Input Parameters: Control value for A wheel motor, control value for B wheel motor, control value for C wheel motor, 
                   control value for D wheel motor, control value for servo.
Return Value: None.
Author: WHEELTEC
函数功能：下发控制量到各个执行机构
入口参数：A轮电机控制量、B轮电机控制量、C轮电机控制量、D轮电机控制量、舵机控制量
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void Set_Pwm(int m_a,int m_b,int m_c,int m_d,int servo)
{
	#if defined AKM_CAR || defined DIFF_CAR
	
		//电机控制
		if( m_a < 0 ) AIN1=1,AIN2=0;
		else          AIN1=0,AIN2=1;
		
		if(SysVal.HardWare_Ver==V1_0)
		{
			if( m_b > 0 ) V1_0_BIN1=1,V1_0_BIN2=0;
			else          V1_0_BIN1=0,V1_0_BIN2=1;
		}
		else if( SysVal.HardWare_Ver==V1_1 )
		{
			if( m_b > 0 ) BIN1=1,BIN2=0;
			else          BIN1=0,BIN2=1;
		}
		
		PWMA = abs(m_a);
		PWMB = abs(m_b);
	
		#if defined AKM_CAR
		//舵机控制
		if( robot.type == 0 || robot.type == 1 || robot.type==9 )//高配阿克曼车型
		{
			SERVO_SENIOR = servo;
		}
		else //顶配阿克曼车型
		{
			SERVO_TOP = servo;
		}
		#endif
		
	#else 
		
		//电机控制
		if( m_a > 0 ) AIN1=0,AIN2=1;
		else          AIN1=1,AIN2=0;
		
		if(SysVal.HardWare_Ver==V1_0)
		{
			if( m_b > 0 ) V1_0_BIN1=0,V1_0_BIN2=1;
			else          V1_0_BIN1=1,V1_0_BIN2=0;
		}
		else if( SysVal.HardWare_Ver==V1_1 )
		{
			if( m_b > 0 ) BIN1=0,BIN2=1;
			else          BIN1=1,BIN2=0;
		}
	
		if( m_c > 0 ) CIN1=1,CIN2=0;
		else          CIN1=0,CIN2=1;
		
		if( m_d > 0 ) DIN1=1,DIN2=0;
		else          DIN1=0,DIN2=1;
		
		PWMA = abs(m_a);
		PWMB = abs(m_b);
		PWMC = abs(m_c);
		PWMD = abs(m_d);
	#endif
}

/**************************************************************************
Function: Check the battery voltage, enable switch status, software failure flag status
Input   : Voltage
Output  : Whether control is allowed, 1: not allowed, 0 allowed
函数功能：检查电池电压、使能开关状态、软件失能标志位状态
入口参数：电压
返回  值：是否允许控制，1：不允许，0允许
**************************************************************************/
static u8 Turn_Off(void)
{
    u8 temp = 0;
	static uint8_t saveflag = 0;
	
	//电压不满足要求：电压低于20V,且不是自动回充状态.(若是进入自动回充状态,则忽略电压)
	if( robot.voltage < 20.0f && charger.AllowRecharge==0 ) temp = 1;

	//急停开关被按下
	if( EN==0 ) temp = 1;
	
	//机器人自检出错
	if( robot_check.errorflag==1 ) temp = 1;

	//机器人通过软件设置了急停
	if( robot_control.SoftWare_Stop==1 ) temp = 1;
	
	//小车由失能变为使能的过程,清空目标速度.
	if(temp==0 && saveflag==1)
	{
		robot_control.Vx = 0;
		robot_control.Vy = 0;
		robot_control.Vz = 0;
		robot_control.smooth_Vx = 0;
		robot_control.smooth_Vy = 0;
		robot_control.smooth_Vz = 0;
	}
	saveflag = temp;
	
	return temp;
}

/**************************************************************************
Function: Processes the command sent by APP through usart x
Input   : none
Output  : none
函数功能：对APP通过串口x发送过来的命令进行处理
入口参数：无
返回  值：无
**************************************************************************/
static void Get_APPcmd(void)
{
	/*  APP摇杆与速度方向
	     A
	     ^  +Vx       ^ +Vz
   H     |       B    ）
	     |
   G----------> -Vy  C
	     |
   F     |    D
	     E
	*/
	
	short m_sign = 1;         //符号变量
	float base_vz= PI/4.0f;   //Z轴速度基准
	
	//非全向移动小车
	#if defined AKM_CAR || defined DIFF_CAR || defined _4WD_CAR
	
		#if defined AKM_CAR
			m_sign = -1;                  //阿克曼Vz表示前轮转角,部分方向符号需要修改.
			base_vz = angle_to_rad(30);
		#endif
	
	switch( appkey.DirectionFlag ) 
	{
		case 1: robot_control.Vx = robot_control.rc_speed , robot_control.Vz = 0               ; break; //A
		case 2: robot_control.Vx = robot_control.rc_speed , robot_control.Vz =-base_vz         ; break; //B
		case 3: robot_control.Vx = 0 ,                      robot_control.Vz =-base_vz         ; break; //C
		case 4: robot_control.Vx =-robot_control.rc_speed , robot_control.Vz = (base_vz)*m_sign; break; //D
		case 5: robot_control.Vx =-robot_control.rc_speed , robot_control.Vz = 0               ; break; //E
		case 6: robot_control.Vx =-robot_control.rc_speed , robot_control.Vz =-(base_vz)*m_sign; break; //F
		case 7: robot_control.Vx = 0 ,                      robot_control.Vz = base_vz         ; break; //G
		case 8: robot_control.Vx = robot_control.rc_speed , robot_control.Vz = base_vz         ; break; //H
		
		default : robot_control.Vx=0;robot_control.Vy=0;robot_control.Vz=0; break;
	}
	
	
	//可全向移动小车
	#elif defined MEC_CAR || defined OMNI_CAR
	
	switch( appkey.DirectionFlag ) 
	{
		case 1: robot_control.Vx = robot_control.rc_speed , robot_control.Vy = 0                      ; break; //A
		case 2: robot_control.Vx = robot_control.rc_speed , robot_control.Vy =-robot_control.rc_speed ; break; //B
		case 3: robot_control.Vx = 0 ,                      robot_control.Vy =-robot_control.rc_speed ; break; //C
		case 4: robot_control.Vx =-robot_control.rc_speed , robot_control.Vy =-robot_control.rc_speed ; break; //D
		case 5: robot_control.Vx =-robot_control.rc_speed , robot_control.Vy = 0                      ; break; //E
		case 6: robot_control.Vx =-robot_control.rc_speed , robot_control.Vy = robot_control.rc_speed ; break; //F
		case 7: robot_control.Vx = 0 ,                      robot_control.Vy = robot_control.rc_speed ; break; //G
		case 8: robot_control.Vx = robot_control.rc_speed , robot_control.Vy = robot_control.rc_speed ; break; //H
		
		default : robot_control.Vx=0;robot_control.Vy=0;robot_control.Vz=0 , m_sign=0 ; break;
	}
	
	//小车不存在x、y轴命令时,检查Z轴命令
	if( m_sign==0 )
	{
		     if( appkey.TurnFlag==1 ) robot_control.Vz =  base_vz;
		else if( appkey.TurnFlag==2 ) robot_control.Vz = -base_vz;
		else                          robot_control.Vz = 0;
	}
	
	#endif

	//单位转换 mm/s -> m/s
	robot_control.Vx = robot_control.Vx/1000.0f;
	robot_control.Vy = robot_control.Vy/1000.0f;
	robot_control.Vz = robot_control.Vz * ( robot_control.rc_speed/500.0f );//Z轴速度根据遥控速度调整.

    //Control target value is obtained and kinematics analysis is performed
    //得到控制目标值，进行运动学分析
    Drive_Motor(robot_control.Vx,robot_control.Vy,robot_control.Vz);
}

/**************************************************************************
Function: The remote control command of model aircraft is processed
Input   : none
Output  : none
函数功能：对航模遥控控制命令进行处理
入口参数：无
返回  值：无
**************************************************************************/
static void Remote_Control(void)
{
    //Data within 1 second after entering the model control mode will not be processed
    //对进入航模控制模式后1秒内的数据不处理
    static u8 thrice=BALANCE_TASK_RATE;
    int Yuzhi=100; //Threshold to ignore small movements of the joystick //阈值，忽略摇杆小幅度动作

    //limiter //限幅
    int LX,LY,RY,RX,Remote_RCvelocity;
	float base_vz = PI/4.0f ;//Z轴速度基准
	
	//阿克曼Z轴代表左前轮角度,设置基准值为 30 度
	#if defined AKM_CAR
		base_vz = angle_to_rad(30.0f);
	#endif
	
	//4个通道原始值限幅
	remoter.ch1 = target_limit_int(remoter.ch1,1000,2000);
	remoter.ch2 = target_limit_int(remoter.ch2,1000,2000);
	remoter.ch3 = target_limit_int(remoter.ch3,1000,2000);
	remoter.ch4 = target_limit_int(remoter.ch4,1000,2000);

    //Front and back direction of left rocker. Control forward and backward.
    //左摇杆前后方向。控制前进后退。
    LX=remoter.ch2-1500;
    
	//The left joystick's horizontal directions control lateral \
	  movement for omnidirectional mobile vehicles.
    //左摇杆左右方向,对于全向移动小车可控制左右横移
    LY=remoter.ch4-1500;

    //Front and back direction of right rocker. Throttle/acceleration/deceleration.
    //右摇杆前后方向。油门/加减速。
    RX=remoter.ch3-1500;
	
    //Right stick left and right. To control the rotation.
    //右摇杆左右方向。控制自转。
    RY=-(remoter.ch1-1500);//自转

    if(LX>-Yuzhi&&LX<Yuzhi)LX=0;
    if(LY>-Yuzhi&&LY<Yuzhi)LY=0;
    if(RX>-Yuzhi&&RX<Yuzhi)RX=0;
    if(RY>-Yuzhi&&RY<Yuzhi)RY=0;

	//油门调整速度 Remote_RCvelocity取值:0~1000
    Remote_RCvelocity= robot_control.rc_speed + RX;
    if(Remote_RCvelocity<0)Remote_RCvelocity=0;

    //The remote control command of model aircraft is processed
    //对航模遥控控制命令进行处理
	
	robot_control.Vx = LX * (float)Remote_RCvelocity/500.0f;
	robot_control.Vy =-LY * (float)Remote_RCvelocity/500.0f;
	
	//基准:base_vz,自身倍数:RY/500,取值[-1~1],油门倍数:(Remote_RCvelocity/500.0f),取值[0,2]
	robot_control.Vz = base_vz*((float)RY/500.0f) * ((float)Remote_RCvelocity/500.0f) ; 

	//非阿克曼车型,在后退时转向方向取反.符合摇杆操作逻辑.
	#if !defined AKM_CAR
	if(  robot_control.Vx < 0 ) robot_control.Vz = -robot_control.Vz;
	#endif

    //Unit conversion, mm/s -> m/s
    //单位转换，mm/s -> m/s
	robot_control.Vx = robot_control.Vx/1000.0f;
	robot_control.Vy = robot_control.Vy/1000.0f;

    //Data within 1 second after entering the model control mode will not be processed
    //对进入航模控制模式后1秒内的数据不处理
    if(thrice>0) robot_control.Vx=0,robot_control.Vy=0,robot_control.Vz=0,thrice--;

    //Control target value is obtained and kinematics analysis is performed
    //得到控制目标值，进行运动学分析
    Drive_Motor(robot_control.Vx,robot_control.Vy,robot_control.Vz);
}

/**************************************************************************
Function: Handle PS2 controller control commands
Input   : none
Output  : none
函数功能：对PS2手柄控制命令进行处理
入口参数：无
返回  值：无
**************************************************************************/
#include "xbox360_gamepad.h"
#include "WiredPS2_gamepad.h"
//xbox360游戏手柄按键回调函数
void Xbox360GamePad_KeyEvent_Callback(uint8_t keyid,GamePadKeyEventType_t event)
{
	//按下start按键
	if( keyid == Xbox360KEY_Menu && event == GamePadKeyEvent_SINGLECLICK )
		GamePadInterface->StartFlag = 1;
	
	if( gamepad_brand == Xbox360 )
	{
		//手柄加减速
		if( keyid == Xbox360KEY_LB && (event == GamePadKeyEvent_DOUBLECLICK || event == GamePadKeyEvent_SINGLECLICK )  )
			robot_control.rc_speed -= 50;
		else if( keyid == Xbox360KEY_RB && (event == GamePadKeyEvent_DOUBLECLICK || event == GamePadKeyEvent_SINGLECLICK )  )
			robot_control.rc_speed += 50;
		
		if( robot_control.rc_speed < 0 ) robot_control.rc_speed = 0;
	}
	else if(  gamepad_brand == PS2_USB_Wiredless )
	{
		if( keyid == Xbox360KEY_LB && (event == GamePadKeyEvent_DOUBLECLICK || event == GamePadKeyEvent_SINGLECLICK )  )
			robot_control.rc_speed += 50;
		else if( keyid == Xbox360_PaddingBit && (event == GamePadKeyEvent_DOUBLECLICK || event == GamePadKeyEvent_SINGLECLICK ) )
			robot_control.rc_speed -= 50;
		if( robot_control.rc_speed < 0 ) robot_control.rc_speed = 0;
	}
	
	
	//震动激活与取消
	if( keyid == Xbox360KEY_SELECT && event == GamePadKeyEvent_LONGCLICK )
	{
		if( GamePadInterface->Vib_EN )
		{
			GamePadInterface->SetVibration(0,127);
			vTaskDelay(50);
			GamePadInterface->Vib_EN = !GamePadInterface->Vib_EN;
		}
		else
		{
			GamePadInterface->Vib_EN = !GamePadInterface->Vib_EN;
			vTaskDelay(50);
			GamePadInterface->SetVibration(0,127);
		}	
	}
}

//有线USB手柄回调函数
void Wired_USB_PS2GamePad_KeyEvent_Callback(uint8_t keyid,GamePadKeyEventType_t event)
{
	//按下start按键
	if( keyid == PS2KEY_START && event == GamePadKeyEvent_SINGLECLICK )
		GamePadInterface->StartFlag = 1;
	
	//手柄加减速
	else if( keyid == PS2KEY_L2 && (event == GamePadKeyEvent_DOUBLECLICK || event == GamePadKeyEvent_SINGLECLICK )  )
		robot_control.rc_speed -= 50;
	else if( keyid == PS2KEY_L1 && (event == GamePadKeyEvent_DOUBLECLICK || event == GamePadKeyEvent_SINGLECLICK )  )
		robot_control.rc_speed += 50;
	
	if( robot_control.rc_speed < 0 ) robot_control.rc_speed = 0;
}

//经典PS2手柄回调函数,非USB款
void Classic_PS2GamePad_KeyEvent_Callback(uint8_t keyid,GamePadKeyEventType_t event)
{
	//按下start按键
	if( keyid == PS2KEY_START && event == GamePadKeyEvent_SINGLECLICK )
		GamePadInterface->StartFlag = 1;
	
	//手柄加减速
	else if( keyid == PS2KEY_L2 && (event == GamePadKeyEvent_DOUBLECLICK || event == GamePadKeyEvent_SINGLECLICK )  )
		robot_control.rc_speed -= 50;
	else if( keyid == PS2KEY_L1 && (event == GamePadKeyEvent_DOUBLECLICK || event == GamePadKeyEvent_SINGLECLICK )  )
		robot_control.rc_speed += 50;
	
	if( robot_control.rc_speed < 0 ) robot_control.rc_speed = 0;
}


//手柄震动映射函数
static uint8_t map_to_vib(float x) {
    // 检查输入范围，限制在 [0.2, 1.2] 内
    if (x < 0.1f) return 0;
    if (x > 1.2f) x = 1.2f;

    // 线性映射
    float result = 255.0f * (x - 0.1f) / 1.1f;

    // 四舍五入并转换为 uint8_t
    return (uint8_t)(result + 0.5f);
}

uint8_t g_usbhost_disconnect=0;
extern USBH_HandleTypeDef hUsbHostFS;
static void PS2_control(void)
{
	float LX=127,LY=127,RX=127;
	float ThrottleTri = 255;
	float base_vz = PI/4.0f ;//Z轴速度基准
	
	g_usbhost_connect++;
	if(g_usbhost_connect==50 && SysVal.HardWare_Ver==V1_1)
	{
		g_usbhost_connect=50;
		
		//被动断连标志位
		g_usbhost_disconnect=1;
		
		//激活USB任务重新连接
        hUsbHostFS.os_msg = (uint32_t)USBH_PORT_EVENT;
        osMessageQueuePut( hUsbHostFS.os_event, & hUsbHostFS.os_msg, 0U, 0U);
		
		//清空目标值
		GamePadInterface->LY=127;
		GamePadInterface->LX=127;
		GamePadInterface->RX=127;
		GamePadInterface->RT=0;
		GamePadInterface->LT=0;
		
		//退出手柄模式
		//Set_Control_Mode(_ROS_Control);
		robot_control.Vx=0;robot_control.Vy=0;robot_control.Vz=0;
	}
	
	//前进摇杆
	LY = GamePadInterface->LY - 127;
	
	//左右横移
	LX = 127 - GamePadInterface->LX;
	
	//顺逆时针
	RX = 127 - GamePadInterface->RX;
	
	//摇杆微小幅度过滤
	if( fabs(LY)<20 ) LY = 0;
	if( fabs(LX)<20 ) LX = 0;
	if( fabs(RX)<20 ) RX = 0;
	
	//针对xbox360手柄，扳机为模拟量时，允许使用扳机控制
	if( gamepad_brand == Xbox360 )
	{
		//前进摇杆无值时,采用扳机的值
		if( (int)LY == 0 )
		{
			if( GamePadInterface->LT == 0 && GamePadInterface->RT != 0 )
				ThrottleTri =  GamePadInterface->RT, LY = 127;
			else if( GamePadInterface->LT != 0 && GamePadInterface->RT == 0 )
				ThrottleTri =  -GamePadInterface->LT,LY = 127;
			else
				ThrottleTri = 0;
		}
	}
	
	//针对usb有线手柄,在非模拟量模式下的摇杆值映射
	else if( gamepad_brand == PS2_USB_Wired ||  gamepad_brand == PS2_USB_WiredV2 )
	{
		if( fabs(RX)<0.0001f )
		{
			if( GamePadInterface->getKeyState(PS2KEY_4PINK) )
				RX = 127;
			else if( GamePadInterface->getKeyState(PS2KEY_2RED) )
				RX = -127;
		}
	}
	
	robot_control.Vx = (LY/127.0f) * robot_control.rc_speed * (ThrottleTri/255.0f);
	robot_control.Vy = (LX/127.0f) * robot_control.rc_speed;
	robot_control.Vz = base_vz * (RX/127.0f) * ( robot_control.rc_speed/500.0f );
	
	#if !defined AKM_CAR
		if( robot_control.Vx<0 ) robot_control.Vz = -robot_control.Vz;
	#endif

    //Unit conversion, mm/s -> m/s
    //单位转换，mm/s -> m/s
    robot_control.Vx = robot_control.Vx/1000.0f;
	robot_control.Vy = robot_control.Vy/1000.0f;

    //Control target value is obtained and kinematics analysis is performed
    //得到控制目标值，进行运动学分析
    Drive_Motor(robot_control.Vx,robot_control.Vy,robot_control.Vz);
	
	//根据加速度反应震动情况传达到手柄
	#include "bsp_gamepad.h"
	
	//Z轴数据判断震动强度
	float now_z = imu.accel.z/1671.84f;
	static float last_z = 0;
	float strength = fabs(last_z - now_z);
	
	#if defined MEC_CAR || defined OMNI_CAR
	const float vib_strength = 0.6f;
	#else
	const float vib_strength = 0.1f;
	#endif
	
	//震动映射到手柄
	if( strength>vib_strength && SysVal.Time_count>CONTROL_DELAY)
	{
		if( GamePadInterface->SetVibration!=NULL )
			GamePadInterface->SetVibration(map_to_vib(strength),0);
	}
	last_z = now_z;
		
//	//TODO:适配PS2控制
//    float LX,LY,RY;
//    int Yuzhi=20; //Threshold to ignore small movements of the joystick //阈值，忽略摇杆小幅度动作
//	
//	float base_vz = PI/4.0f ;//Z轴速度基准
//	
//	//针对 usb有线ps2手柄 数字模式控制转换
//	if( SysVal.HardWare_Ver==V1_1 )
//	{
//		if( ps2_type == Normal_PS2 )
//		{
//			if( ps2_val.RX==128 && ps2_val.RY==128 )
//			{
//				if( Read_PS2_KEY(R_4PINK )==PS2_KEY_ON )  ps2_val.RX  = 0;
//				if( Read_PS2_KEY(R_2RED  )==PS2_KEY_ON )  ps2_val.RX = 255;
//				if( Read_PS2_KEY(R_1GREEN)==PS2_KEY_ON )  ps2_val.RY = 0;
//				if( Read_PS2_KEY(R_3BLUE )==PS2_KEY_ON )  ps2_val.RY = 255;
// 			}
//		}
//	}
//	
//    //128 is the median.The definition of X and Y in the PS2 coordinate system is different from that in the ROS coordinate system
//    //128为中值。PS2坐标系与ROS坐标系对X、Y的定义不一样
//    LY=-(ps2_val.LX-128);//取值[-128,128]
//    LX=-(ps2_val.LY-128);
//    RY=-(ps2_val.RX-128);

//    //Ignore small movements of the joystick //忽略摇杆小幅度动作
//    if(LX>-Yuzhi&&LX<Yuzhi)LX=0;
//    if(LY>-Yuzhi&&LY<Yuzhi)LY=0;
//    if(RY>-Yuzhi&&RY<Yuzhi)RY=0;
//	
//	static uint8_t low_fre = 0;
//	if(++low_fre==10)//降低按键扫描频率
//	{
//		low_fre = 0;
//		//PS2手柄,左扳机1号可加速,2号可减速
//			 if( Read_PS2_KEY(L1_KEY)==PS2_KEY_ON ) robot_control.rc_speed+=20;
//		else if( Read_PS2_KEY(L2_KEY)==PS2_KEY_ON ) robot_control.rc_speed-=20;
//		
//		//速度最大最小值限制
//		robot_control.rc_speed = target_limit_float(robot_control.rc_speed,0,robot_control.limt_max_speed*1000);
//	}

//	#if defined AKM_CAR
//		base_vz = angle_to_rad(30.0f);//阿克曼Z轴基准角度
//	#endif
//	
//    //Handle PS2 controller control commands
//    //对PS2手柄控制命令进行处理
//	robot_control.Vx = (LX/128.0f) * robot_control.rc_speed;
//	robot_control.Vy = (LY/128.0f) * robot_control.rc_speed;
//	robot_control.Vz = base_vz * (RY/128.0f) * ( robot_control.rc_speed/500.0f );
//	
//	#if !defined AKM_CAR
//		if( robot_control.Vx<0 ) robot_control.Vz = -robot_control.Vz;
//	#endif

//    //Unit conversion, mm/s -> m/s
//    //单位转换，mm/s -> m/s
//    robot_control.Vx = robot_control.Vx/1000.0f;
//	robot_control.Vy = robot_control.Vy/1000.0f;

//    //Control target value is obtained and kinematics analysis is performed
//    //得到控制目标值，进行运动学分析
//    Drive_Motor(robot_control.Vx,robot_control.Vy,robot_control.Vz);
}

/**************************************************************************
Functionality: Robot Self-Check Function - Performs a self-check on the robot to verify if there are any errors in the wiring of motors, encoders, and drivers.
Input Parameters: None.
Return Value: None.
Author: WHEELTEC
函数功能： 机器人自检函数,检查机器人电机、编码器、驱动器是否存在错误接线
入口参数：无
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void robot_mode_check(void)
{
	//TODO:自检程序,同向电机接反难判断.
	
	#define OVERLOAD_PWM 8000 //超载PWM预警值
	#define ENCODER_LOST_PWM 5500 //编码器未接时,PWM报错阈值
	
    static u8 init=0;//初始化
    if( 0 == init ) 
	{
		ROBOT_SELFCHECK_t_Init(&robot_check);
		init=1;
	}

    if( EN==1 && robot_check.errorflag ==0 ) //保留可以使用急停开关跳过自检的功能
    {
		#if defined OMNI_CAR //全向轮小车结构特殊,独立自检逻辑
			
			//1、全向轮自检时,仅B、C电机转动.若A路存在数值则说明驱动器或编码器线接错(/4代表四分一圈)
			if( robot_check.check_a > robot.HardwareParam.Encoder_precision/4 || \
				robot_check.check_a < -robot.HardwareParam.Encoder_precision/4	)
			{
				Buzzer_AddTask(1,20);
				robot_check.errorflag = 1;
			}
			
			//2、车型不对或接线不对导致的电机反向错误
			if( robot_check.check_b > robot.HardwareParam.Encoder_precision/4 || \
				robot_check.check_c < -robot.HardwareParam.Encoder_precision/4 )
			{
				Buzzer_AddTask(2,20);
				robot_check.errorflag = 1;
			}
			
			//3、任意电机PWM输出到达阈值,对应的编码器数据很小(轮子转动不到12°)或无数据,则认为编码器线未接
			if( ( fabs( robot.MOTOR_B.Output ) > ENCODER_LOST_PWM && robot_check.check_b > -robot.HardwareParam.Encoder_precision/30 ) || \
				( fabs( robot.MOTOR_C.Output ) > ENCODER_LOST_PWM && robot_check.check_c < robot.HardwareParam.Encoder_precision/30 ) )
			{
				Buzzer_AddTask(3,20);
				robot_check.errorflag = 1;
			}
			
			//4、任意电机PWM输出超过更大阈值,则认为小车过载
			if( fabs( robot.MOTOR_A.Output ) > OVERLOAD_PWM || fabs( robot.MOTOR_B.Output ) > OVERLOAD_PWM || \
				fabs( robot.MOTOR_C.Output ) > OVERLOAD_PWM )
			{
				Buzzer_AddTask(4,20);
				robot_check.errorflag = 1;
			}
			
		#else
			static int last_a=0,last_b=0,last_c=0,last_d=0;
			static u8 err_time = 0;
			
			//1、任意车轮反转 1/4 圈 认为是编码器线或驱动线接反,或者是车型选错.
			if( robot_check.check_a < -robot.HardwareParam.Encoder_precision/4  || \
				robot_check.check_b < -robot.HardwareParam.Encoder_precision/4  || \
				robot_check.check_c < -robot.HardwareParam.Encoder_precision/4  || \
				robot_check.check_d < -robot.HardwareParam.Encoder_precision/4  ) 
			{
				robot_check.errorflag = 1;
				Buzzer_AddTask(1,20);
			}
			
			//2、任意电机PWM输出到达阈值,对应的编码器数据很小(轮子转动不到12°)或无数据,则认为编码器线未接
			if( ( abs( robot.MOTOR_A.Output ) > ENCODER_LOST_PWM && robot_check.check_a < robot.HardwareParam.Encoder_precision/30 ) || \
				( abs( robot.MOTOR_B.Output ) > ENCODER_LOST_PWM && robot_check.check_b < robot.HardwareParam.Encoder_precision/30 ) || \
				( abs( robot.MOTOR_C.Output ) > ENCODER_LOST_PWM && robot_check.check_c < robot.HardwareParam.Encoder_precision/30 ) || \
				( abs( robot.MOTOR_D.Output ) > ENCODER_LOST_PWM && robot_check.check_d < robot.HardwareParam.Encoder_precision/30 )  )
			{
				robot_check.errorflag = 1;
				Buzzer_AddTask(2,20);
			}
			
			//3、任意电机PWM输出超过更大阈值,则认为小车过载
			if( abs( robot.MOTOR_A.Output ) > OVERLOAD_PWM || abs( robot.MOTOR_B.Output ) > OVERLOAD_PWM || \
				abs( robot.MOTOR_C.Output ) > OVERLOAD_PWM || abs( robot.MOTOR_D.Output ) > OVERLOAD_PWM	)
			{
				robot_check.errorflag = 1;
				Buzzer_AddTask(3,20);
			}
			
			//4、出现多次累计值倒退,除了1中的异常,也可能是多种错误累计叠加的表现.
			if( abs(robot_check.check_a) < abs(last_a) - 50 || abs(robot_check.check_b) < abs(last_b) - 50 || abs(robot_check.check_c) < abs(last_c) - 50|| abs(robot_check.check_d) < abs(last_d) - 50 )
			{
				err_time++;
				if( err_time > 20 )  robot_check.errorflag = 1 , Buzzer_AddTask(4,20);	
			}
		
			last_a = robot_check.check_a;
			last_b = robot_check.check_b;
			last_c = robot_check.check_c;
			last_d = robot_check.check_d;
		#endif
    }
}

/**************************************************************************
Functionality: Read Feedback Values - Retrieves feedback values from each actuator of the robot.
Input Parameters: None.
Return Value: None.
Author: WHEELTEC
函数功能：读取机器各个执行机构的反馈值
入口参数：无
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void Get_Robot_FeedBack(void)
{
    //Retrieves the original data of the encoder
    //获取编码器的原始数据
    short Encoder_A_pr,Encoder_B_pr;
	
	//编码器原始值,读取后清空.
    Encoder_A_pr = Read_Encoder(Encoder_A);
    Encoder_B_pr = Read_Encoder(Encoder_B);
	
	#if !defined AKM_CAR && !defined DIFF_CAR
		short Encoder_C_pr;
		Encoder_C_pr = Read_Encoder(Encoder_C);
	#endif
	
	#if defined MEC_CAR || defined _4WD_CAR
		short Encoder_D_pr;
		Encoder_D_pr = Read_Encoder(Encoder_D);
	#endif
	
	//计算纠偏系数
	float LeftWheelDiff = wheelCoefficient(robot_control.LineDiffParam,1);
	float RightWheelDiff = wheelCoefficient(robot_control.LineDiffParam,0);
	
    //The encoder converts the raw data to wheel speed in m/s
    //编码器原始数据转换为车轮速度，单位m/s
	#if defined AKM_CAR
	
		//未完成自检时采集编码器数据进行判断
		if( robot_check.check_end == 0 )
		{
			robot_check.check_a +=  Encoder_A_pr;
			robot_check.check_b += -Encoder_B_pr;
		}
		
		//将两个电机的编码器的原始读数转换为 m/s 
		robot.MOTOR_A.Encoder =   Encoder_A_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / LeftWheelDiff;
		robot.MOTOR_B.Encoder = - Encoder_B_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / RightWheelDiff;
	
        //Gets the position of the slide, representing the front wheel rotation Angle
        //获取滑轨位置,代表前轮转角角度.该数值由ADC2负责采集,DMA完成搬运,此处仅需处理数据即可
		robot.SERVO.Encoder = get_DMA_SlideRes();
		robot.SERVO.Encoder = Slide_Mean_Filter(robot.SERVO.Encoder);//将采集结果进行平滑滤波
	
	#elif defined DIFF_CAR
		//未完成自检时采集编码器数据进行判断
		if( robot_check.check_end == 0 )
		{
			robot_check.check_a +=  Encoder_A_pr;
			robot_check.check_b += -Encoder_B_pr;
		}
		
		robot.MOTOR_A.Encoder =   Encoder_A_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / LeftWheelDiff;
		robot.MOTOR_B.Encoder = - Encoder_B_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / RightWheelDiff;	
		
	#elif defined MEC_CAR || defined _4WD_CAR	
		//未完成自检时采集编码器数据进行判断
		if( robot_check.check_end == 0 )
		{
			robot_check.check_a +=  Encoder_A_pr;
			robot_check.check_b +=  Encoder_B_pr;
			robot_check.check_c += -Encoder_C_pr;
			robot_check.check_d += -Encoder_D_pr;
		}
		robot.MOTOR_A.Encoder =   Encoder_A_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / LeftWheelDiff;
		robot.MOTOR_B.Encoder =   Encoder_B_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / LeftWheelDiff;
		robot.MOTOR_C.Encoder = - Encoder_C_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / RightWheelDiff;
		robot.MOTOR_D.Encoder = - Encoder_D_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / RightWheelDiff;	
		
	#elif defined OMNI_CAR	
		//未完成自检时采集编码器数据进行判断
		if( robot_check.check_end == 0 )
		{
			robot_check.check_a += -Encoder_A_pr;
			robot_check.check_b += -Encoder_B_pr;
			robot_check.check_c += -Encoder_C_pr;
		}
		
		robot.MOTOR_A.Encoder = - Encoder_A_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ;
		robot.MOTOR_B.Encoder = - Encoder_B_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / LeftWheelDiff;
		robot.MOTOR_C.Encoder = - Encoder_C_pr*BALANCE_TASK_RATE/( robot.HardwareParam.Encoder_precision )* robot.HardwareParam.Wheel_Circ / RightWheelDiff;	
		
	#endif
}

/*-------------------------------- Robot state detection related functions ------------------------------------*/
/*--------------------------------         机器人状态检测相关函数          ------------------------------------*/
/**************************************************************************
Function Description: The function clears residual motor control signals when the robot \
  remains stationary on a non-sloping surface for a certain period of time. This helps  \
  reduce power consumption and motor noise of the robot.
Input Parameters: None
Return Value: None
Author: WHEELTEC
函数功能：小车在非斜坡上静止超过一定时间,清除电机残余的控制量.降低机器人功耗以及降低电机的噪音
入口参数：无
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void Robot_ParkingCheck(void)
{
	#define PUSH_PWM 500 //推车累计的PWM阈值,超出将执行清除
	
    //小车姿态简易判断
    float y_accle = (float)(imu.accel.y/1671.84f);//Y轴加速度实际值
    float z_accle = (float)(imu.accel.z/1671.84f);//Z轴加速度实际值
    float diff;

    //计算Y、Z加速度融合值，该值越接近9.8，表示小车姿态越水平
    if( y_accle > 0 ) diff  = z_accle - y_accle;
    else diff  = z_accle + y_accle;
	
	if( robot.MOTOR_A.Target != 0 || robot.MOTOR_B.Target!=0 || \
		robot.MOTOR_C.Target != 0 || robot.MOTOR_D.Target!=0)
	{   //小车被控制中,设置等待标志位.复位其他检测变量
		park.wait_check = 1;  //等待检测
		park.timecore = 0;    //复位检测相关变量
		park.clear_state = 0;
		park.start_clear = 0;
	}
	else 
	{
		if( park.wait_check==1 ) //速度非0 -> 0 触发检测
		{
			park.timecore++;
			if( park.timecore >= BALANCE_TASK_RATE*5 ) //小车静止时间超过5秒
			{
				park.wait_check=0;
				park.timecore = 0;
				
				//若机器人不在斜面上,则进入驻车模式,清空电机控制量
				if( diff > 8.8f ) park.start_clear = 1 , park.clear_state = 0;
				else  park.clear_state |= 1<<7 ; //在斜坡上,不可清除控制量.标记清除已完成
			}
		}
	}
	
	//检查是否已完成清除.完成清除后将会监测小车是否被动积累控制量(推车行为)
	if( ((park.clear_state>>7)&0x01)==1  )
	{
		if(diff > 8.8f )//小车不在斜坡上作为前提.
		{
			//出现推车行为
			if( abs(robot.MOTOR_A.Output) > PUSH_PWM || abs(robot.MOTOR_B.Output) > PUSH_PWM || \
				abs(robot.MOTOR_C.Output) > PUSH_PWM || abs(robot.MOTOR_D.Output) > PUSH_PWM)
			{
				park.timecore++;
				if( park.timecore >= BALANCE_TASK_RATE*10 ) //小车pwm累计时间超过10秒
				{
					park.timecore = 0;
					//进入驻车模式清除控制量
					park.start_clear = 1 , park.clear_state = 0;
				}
			}
		}
	}
	
	//开启驻车模式,执行清除任务
	if( park.start_clear==1 )
	{
		if( 1 == PI_Clear_Output(&PI_MotorA) ) park.clear_state |= 1<<0 ;
		else                                   park.clear_state &= ~(1<<0);
		
		if( 1 == PI_Clear_Output(&PI_MotorB) ) park.clear_state |= 1<<1 ;
		else                                   park.clear_state &= ~(1<<1);
		
		if( 1 == PI_Clear_Output(&PI_MotorC) ) park.clear_state |= 1<<2 ;
		else                                   park.clear_state &= ~(1<<2);
		
		if( 1 == PI_Clear_Output(&PI_MotorD) ) park.clear_state |= 1<<3 ;
		else                                   park.clear_state &= ~(1<<3);
		
		//4个电机完成清除
		if( (park.clear_state&0xff)==0x0f ) 
		{
			park.start_clear = 0;    //清除任务结束
			park.clear_state |= 1<<7;//标记清除完成
		}
	}
}

/**************************************************************************
Function function: Scan user buttons and perform different functions through different button states
Entry parameter: Frequency of executing scanning tasks
Return value: None
Author: SHEELTEC
函数功能：扫描用户按键,通过不同的按键状态执行不同的功能
入口参数：执行扫描的任务频率
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void UserKey_Scan(u16 rate)
{
	u8 state;
	
	//板子上的用户按键扫描
	state = KEY_Scan(rate,0);
	if(state==single_click) //单击
	{
		charger.AllowRecharge = ! charger.AllowRecharge;
	}
	else if( state==double_click )//双击重新标定imu数值
	{	
		ImuData_copy(&imu.Deviation_gyro,&imu.Original_gyro);
		ImuData_copy(&imu.Deviation_accel,&imu.Original_accel);
	}
	
	else if( state==long_click ) //长按
	{
		oled.page++;
		if( oled.page > oled.MAX_PAGE ) oled.page = 1;
	}
	
}

/**************************************************************************
Functionality: Check Automatic Recharge Station Status - Verifies the status of the automatic recharge station.
Input Parameters: None.
Return Value: None.
Author: WHEELTEC
函数功能：对自动回充设备状态进行检查
入口参数：无
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void Charger_DevCheck(void)
{
	//用户设置进入自动回充状态,此时先确认系统上是否存在该设备
	if( charger.AllowRecharge==1 )
		if( SysVal.HardWare_charger==0 ) charger.AllowRecharge=0,Find_Charging_HardWare();//不存在充电装备硬件,通过访问特征码进行二次确认
	
	//系统上存在该设备时,对自动回充进行硬件离线监测
	if(SysVal.HardWare_charger==1)
	{   //回充装备硬件离线,执行回充设备软件复位
		charger.OutLine_Check++;
		if( charger.OutLine_Check>RATE_100_HZ) auto_recharge_reset() , charger.OutLine_Check=BALANCE_TASK_RATE+1;//隶属哪个任务,就用哪个任务频率,这里是“BALANCE_TASK_RATE”
		
		//存在自动回充设备时，若检测到小车电量低,且收到充电桩的红外信号，将会自行开启自动回充功能
		static u8 once=1;
		if( robot.voltage < 20.0f && charger.RED_STATE!=0 && once)
		{
			once = 0;
			charger.AllowRecharge = 1;
		}
	}
	
	//航模遥控信号检测
	remoter.check_count++;//此变量在航模油门接收时会被置0
	if( remoter.check_count > Filter_Threshold )//超过设定的阈值,说明是无航模信号或者是干扰信号
	{
		remoter.check_count = Filter_Threshold + 1;
		remoter.ch1 = 1500;remoter.ch2=1500;remoter.ch3=1000;remoter.ch4=1500;//干扰或无信号,所有通道数值复位
	}
}

/*-------------------------------- Function auxiliary type related functions ------------------------------------*/
/*--------------------------------         功能辅助类相关函数          ------------------------------------*/

/**************************************************************************
Function function: Save the steering variables of the Ackermann model's servo to Flash
Entry parameter: Flag bit for executing save
Return value: 1: Successfully saved 0: Not saved or failed to save
Author: WHEELTEC
函数功能：保存阿克曼车型的舵机转向变量到Flash
入口参数：是否执行保存的标志位
返回  值：1:保存成功 0:不存在保存需求 >1:flash写入错误
作    者：WHEELTEC
**************************************************************************/
static uint8_t FlashParam_Save(uint8_t *flag)
{
	u8 check=0;
	
	if(*flag==1)
	{
		*flag = 0;
		check = 1;
		taskENTER_CRITICAL();//操作FLash进入临界，保证数据安全
		int buf[7]={0};
		buf[0] = Akm_Servo.Min;
		buf[1] = Akm_Servo.Mid;
		buf[2] = Akm_Servo.Max;
		buf[3] = robot_control.LineDiffParam;
		buf[4] = *((int32_t*)&robot_control.rc_speed);
		buf[5] = robot.V_KP;
		buf[6] = robot.V_KI;
		check += Write_Flash( (u32*)buf , 7);
		
		taskEXIT_CRITICAL();//退出临界
		//若全部写入成功,check==1
	}

	return check;
}

void FlashParam_Read(void)
{
	int read;
	read = Read_Flash(0);//读取下标为0的数据
	if( read!=0xffffffff ) Akm_Servo.Min = read;
	
	read = Read_Flash(1);//读取下标为1的数据
	if( read!=0xffffffff ) Akm_Servo.Mid = read;
	
	read = Read_Flash(2);//读取下标为2的数据
	if( read!=0xffffffff ) Akm_Servo.Max = read;
	
	read = Read_Flash(3); //纠偏系数
	if( read!=0xffffffff ) robot_control.LineDiffParam = read;
	
	
	read = Read_Flash(4);//速度
	if( read!=0xffffffff ) robot_control.rc_speed = *((float*)&read);
	if( robot_control.rc_speed < 0 || robot_control.rc_speed > 10000 )//异常速度数据过滤
		robot_control.rc_speed = 500;
	
	
	read = Read_Flash(5);//KP参数
	if( read!=0xffffffff ) 
	{
		Set_Robot_PI_Param(read,-1);
	}
	
	read = Read_Flash(6);//KI参数
	if( read!=0xffffffff ) 
	{
		Set_Robot_PI_Param(-1,read);
	}
}

//static uint8_t Akm_SaveServo_Param(uint8_t *flag)
//{
//	u8 check=0;
//	
//	if(*flag==1)
//	{
//		*flag = 0;
//		check = 1;
//		taskENTER_CRITICAL();//操作FLash进入临界，保证数据安全
//		int buf[4]={Akm_Servo.Min,Akm_Servo.Mid,Akm_Servo.Max,robot_control.LineDiffParam};
//		check += Write_Flash( (u32*)buf , 4);
//		taskEXIT_CRITICAL();//退出临界
//		//若全部写入成功,check==1
//	}

//	return check;
//}

//void Akm_ReadServo_Param(void)
//{
//	int read;
//	read = Read_Flash(0);//读取下标为0的数据
//	if( read!=0xffffffff ) Akm_Servo.Min = read;
//	
//	read = Read_Flash(1);//读取下标为1的数据
//	if( read!=0xffffffff ) Akm_Servo.Mid = read;
//	
//	read = Read_Flash(2);//读取下标为2的数据
//	if( read!=0xffffffff ) Akm_Servo.Max = read;
//	
//	read = Read_Flash(3); //纠偏系数
//	if( read!=0xffffffff ) robot_control.LineDiffParam = read;
//	
//}

/**************************************************************************
Functionality: Velocity Smoothing Function - Gradually adjusts the speed to the target speed using a specified step value.
Input Parameters: Current speed, target speed, smoothing step value.
Return Value: Smoothed speed.
Author: WHEELTEC
函数功能：速度平滑函数,让速度以设置的步进值到达目标速度
入口参数：当前速度、目标速度、平滑的步进值
返回  值：平滑后的速度
作    者：WHEELTEC
**************************************************************************/
static float Vel_SmoothControl(float now_speed,float targetSpeed,float step)
{
	if( now_speed > targetSpeed )
	{
		now_speed -= step;
		if( now_speed<=targetSpeed ) now_speed = targetSpeed;
	}
	else
	{
		now_speed += step;
		if( now_speed>=targetSpeed ) now_speed = targetSpeed;
	}

	return now_speed;
}

/**************************************************************************
Function: Limiting function
Input   : Value
Output  : none
函数功能：限幅函数
入口参数：幅值
返回  值：无
**************************************************************************/
float target_limit_float(float insert,float low,float high)
{
    if (insert < low)
        return low;
    else if (insert > high)
        return high;
    else
        return insert;
}

static int target_limit_int(int insert,int low,int high)
{
    if (insert < low)
        return low;
    else if (insert > high)
        return high;
    else
        return insert;
}

/**************************************************************************
Function: Data sliding filtering
Input   : data
Output  : Filtered data
函数功能：阿克曼滑轨数据 滤波
入口参数：新采集的数据
返回  值：滤波后的数据
作    者：WHEELTEC
**************************************************************************/
#if defined AKM_CAR
#define FILTERING_TIMES 20
static int Slide_Mean_Filter(int data)
{
    u8 i;
    s32 Sum_Speed = 0;
    s16 Filter_Speed;
    static  s16 Speed_Buf[FILTERING_TIMES]= {0};
    for(i = 1 ; i<FILTERING_TIMES; i++)
    {
        Speed_Buf[i - 1] = Speed_Buf[i];
    }
    Speed_Buf[FILTERING_TIMES - 1] =data;

    for(i = 0 ; i < FILTERING_TIMES; i++)
    {
        Sum_Speed += Speed_Buf[i];
    }
    Filter_Speed = (s16)(Sum_Speed / FILTERING_TIMES);
    return Filter_Speed;
}
#endif

///**************************************************************************
//Function: Smoothing the front wheel steering speed to prevent excessive steering gear current
//Input   : Current servo PWM, Target servo PWM, Smooth value
//Output  : none
//函数功能：对前轮转向速度做平滑处理，防止舵机电流过大
//入口参数：当前舵机控制PWM值 目标舵机控制PWM值 平滑值
//返回  值：无
//**************************************************************************/
//int Smooth_steering(int currentPWM, int targetPWM, float step)
//{
//    int threshold=7;
//    if     (targetPWM>currentPWM+threshold) currentPWM+=step;
//    else if(targetPWM<currentPWM-threshold) currentPWM-=step;
//    else                                    currentPWM =targetPWM;

//    return currentPWM;
//}


/**************************************************************************
Functionality: Radian to Degree Conversion - Converts a given radian value to its corresponding degree value.
Input Parameters: Radian value.
Return Value: Degree value corresponding to the input radian.
Author: WHEELTEC
函数功能：弧度转角度
入口参数：弧度
返回  值：输入弧度对应的角度
作    者：WHEELTEC
**************************************************************************/
float rad_to_angle(const float rad)
{
    return rad/PI*180.0f;
}

/**************************************************************************
Functionality: Degree to Radian Conversion - Converts a given degree value to its corresponding radian value.
Input Parameters: Degree value.
Return Value: Radian value corresponding to the input degree.
Author: WHEELTEC
函数功能：角度转弧度
入口参数：角度
返回  值：输入角度对应的弧度
作    者：WHEELTEC
**************************************************************************/
float angle_to_rad(const float angle)
{
    return angle/180.0f*PI;
}


/*-------------------------------- Software initialization related functions ------------------------------------*/
/*--------------------------------        软件初始化相关函数          ------------------------------------*/
/**************************************************************************
Functionality: Incremental PI Controller Initialization - Initializes an incremental PI 
               (Proportional-Integral) controller with specified parameters.
Input Parameters: PI controller, kp value (proportional gain), ki value (integral gain).
Return Value: None.
Author: WHEELTEC
函数功能：增量式PI控制器初始化
入口参数：PI控制器、kp值、ki值
返回  值：无
作    者：WHEELTEC
**************************************************************************/
void PI_Controller_Init(PI_CONTROLLER* p,int kp,int ki)
{
	p->Bias = 0;
	p->LastBias = 0;
	p->Output = 0;
	p->kp = kp;
	p->ki = ki;
}

//机器人控制相关变量初始化
void ROBOT_CONTROL_t_Init(ROBOT_CONTROL_t* p)
{
	//可调参数
	p->limt_max_speed = 3.5f;    //机器人限制最大的运动速度 m/s
	p->rc_speed = 500;           //机器人遥控速度基准,单位 mm/s
	p->smooth_MotorStep = 0.02f; //机器人电机速度平滑步进值
	p->smooth_ServoStep = 20;    //机器人舵机速度平滑步进值
	p->SoftWare_Stop = 0;        //机器人软件失能位
	
	//高速阿克曼车型,将最大运动速度限制设置为 6 m/s
	#if defined AKM_CAR
		if(robot.type==9) p->limt_max_speed = 6.0f;
		p->ServoLow_flag = 0;//低速舵机模式默认关闭
	#endif
	
	//只读参数,保持默认
	p->Vx = 0;
	p->Vy = 0;
	p->Vz = 0;
	p->smooth_Vx = 0;
	p->smooth_Vy = 0;
	p->smooth_Vz = 0;
	p->command_lostcount = 0;
	p->ControlMode = 1;
	p->FlagStop = 0;
	
	//纠偏系数
	p->LineDiffParam = 50;
}

//自检变量初始化
static void ROBOT_SELFCHECK_t_Init(ROBOT_SELFCHECK_t* p)
{
	p->check_a = 0;
	p->check_b = 0;
	p->check_c = 0;
	p->check_d = 0;
	p->check_end = 0;
	p->errorflag  =0;
	p->DeepCheck = 0;
}

//检查某个变量是否发生改变
//入口参数:执行该函数的频率、需要检查的数据、变化幅度超过多少表示变化
//返回值：1发生了改变 0:未改变
uint8_t ValChangeCheck(const uint16_t rate,const short checkval,const uint8_t changeEva)
{
	static uint16_t timecore;
	static short lastval;
	
	const uint8_t DivisionFac = 2;     //分频系数
	if( rate < DivisionFac ) return 0;//频率过小，无法检测
	
	uint8_t changeflag = 0;//表示是否有改变

	timecore++;
	if( timecore >= rate/DivisionFac ) // 500ms检测1次
	{
		timecore = 0;
		if( abs( lastval - checkval ) > changeEva )  changeflag = 1;
		else
		{
			changeflag=0;
		}
		lastval = checkval;
	}
	
	return changeflag ;
}

//深度自检程序,需要把小车架起来后运行检查
static uint8_t Deep_SelfCheck( u16 RATE )
{	
	static uint32_t timecore = 0;
	static uint8_t errflag = 0;
	uint8_t check_ready = 0;
	
	//测量方法：给定单个轮子的编码器目标值,观察各个数据情况.
	
	/*
	报错优先级：
	1.出现负数,说明是车型选错,或者编码器AB相接反或者是电机输出端的正负接反
	2.其他编码器有值,说明是电机驱动输出口接错,或者是编码器接口接错.
	3.编码器没有值+PWM过大,则是编码器未接或者是电机接口未接
	*/

	timecore++;

	if( timecore==1 )
	{
		if( data_TaskHandle!=NULL ) vTaskSuspend(data_TaskHandle);//在深度自检程序运行时,挂起数据发送任务(该任务占用串口1),以免资源冲突
		any_printf(USART1,"\r\n小车进入深度自检模式,电机的转动顺序为A、B、C、D.请观察转动情况与报错日志.\r\n");
		Buzzer_AddTask(1,20);//蜂鸣器提示表示已进入深度自检模式
	}
	
	//第0~1秒前给定A轮速度
	if( timecore<RATE*2)
	{
		//直接指定电机的目标值并允许响应
		robot.MOTOR_A.Target = 0.5f;
		robot.MOTOR_B.Target = 0.0f;
		robot.MOTOR_C.Target = 0.0f;
		robot.MOTOR_D.Target = 0.0f;
		ResponseControl();
		PWMB = 0;PWMC = 0;PWMD = 0;
	}
	else if( timecore==RATE*2 ) //检查A轮是否存在异常
	{
		any_printf(USART1,"================= 电机A ================\r\n");
		
		//开始检查数据
		if( robot.MOTOR_A.Encoder > 0.4f && robot.MOTOR_A.Encoder < 0.6f )
		{
			any_printf(USART1,"电机A正常.\r\n");
			//正常情况
			//A轮正常
		}
		else
		{
			errflag = 1; //出现错误
			
			//按报错优先级检查错误情况
			if( robot.MOTOR_A.Encoder < - 0.2f ) 
			{
				any_printf(USART1,"检测到A电机编码器极性错误.\r\n");
				any_printf(USART1,"请检查：\r\n1.车型选择是否正确\r\n2.电机A的正负极是否存在线序错误\r\n3.电机A的编码器AB相线序是否错误\r\n");
				//对应接口出现负值,请检查：1.车型是否正确 2.电机的正负级是否反接 3.编码器AB相的线序是否正确
			}
			else if( fabs(robot.MOTOR_B.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机A时,检测到编码器接口B的信号.\r\n自检时电机A是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口A、B是否存在调换\r\n否: 1.检查驱动器的A、B接口是否存在调换\r\n");
				//编码器B有值,请检查：1.编码器接口A与接口B接线是否调换了 2.驱动器的A、B接口是否存在调换
			}
			else if( fabs(robot.MOTOR_C.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机A时,检测到编码器接口C的信号.\r\n自检时电机A是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口A、C是否存在调换\r\n否: 1.检查驱动器的A、C接口是否存在调换 2.检查板子右下角的两个牛角座是否存在调换\r\n");
				//编码器C有值,请检查：1.编码器A与C接口接线是否调换了 2.驱动器A、C接口是否存在调换 3.板子的牛角座是否存在调换
			}
			else if( fabs(robot.MOTOR_D.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机A时,检测到编码器接口D的信号.\r\n自检时电机A是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口A、D是否存在调换\r\n否: 1.检查驱动器的A、D接口是否存在调换 2.检查板子右下角的两个牛角座是否存在调换\r\n");
				//编码器D有值,请检查：1.编码器A与D接口接线是否调换了 2.驱动器A、D接口是否存在调换 3.板子的牛角座是否存在调换
			}
			else if( fabs(robot.MOTOR_A.Encoder) < 0.05f && PI_MotorA.Output > 5000 ) 
			{
				any_printf(USART1,"驱动电机A时,未检测到编码器信号.\r\n自检时电机A是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口A是否存在松动或未接\r\n否: 1.检查电机A的电源线是否存在松动或未接 2.检查板子右下角左边的牛角座是否松动或未接\r\n");
				//编码器没有值,对应接口PWM值较大,请检查：1.编码器接口是否松动或未接 2.电机正负极接头是否松动或者未接
			}
			
		}
		
		any_printf(USART1,"================= 电机A ================\r\n\r\n");
		
		//检查完毕,复位PI控制器,防止下一个电机检查受到干扰
		PI_Controller_Reset(&PI_MotorA);
		PI_Controller_Reset(&PI_MotorB);
		PI_Controller_Reset(&PI_MotorC);
		PI_Controller_Reset(&PI_MotorD);
	}
	else if( timecore>RATE*2 && timecore<RATE*4 ) //B轮控制
	{
		robot.MOTOR_A.Target = 0.0f;robot.MOTOR_B.Target = 0.5f;
		robot.MOTOR_C.Target = 0.0f;robot.MOTOR_D.Target = 0.0f;
		ResponseControl();
		PWMA = 0;PWMC = 0;PWMD = 0;
	} 
	else if( timecore==RATE*4 )  //B轮检查
	{
		any_printf(USART1,"================= 电机B ================\r\n");
		
		if( robot.MOTOR_B.Encoder > 0.4f && robot.MOTOR_B.Encoder < 0.6f )
		{
			any_printf(USART1,"电机B正常.\r\n");
		}
		else
		{
			errflag = 1; //出现错误
			
			if( robot.MOTOR_B.Encoder < - 0.2f ) 
			{
				any_printf(USART1,"检测到B电机编码器极性错误.\r\n");
				any_printf(USART1,"请检查：\r\n1.车型选择是否正确\r\n2.电机B的正负极是否存在线序错误\r\n3.电机B的编码器AB相线序是否错误\r\n");
				//对应接口出现负值,请检查：1.车型是否正确 2.电机的正负级是否反接 3.编码器AB相的线序是否正确
			}
			else if( fabs(robot.MOTOR_A.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机B时,检测到编码器接口A的信号.\r\n自检时电机B是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口A、B是否存在调换\r\n否: 1.检查驱动器的A、B接口是否存在调换\r\n");
				//编码器B有值,请检查：1.编码器接口B与接口A接线是否调换了 2.驱动器的B、A接口是否存在调换
			}
			else if( fabs(robot.MOTOR_C.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机B时,检测到编码器接口C的信号.\r\n自检时电机B是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口B、C是否存在调换\r\n否: 1.检查驱动器的B、C接口是否存在调换 2.检查板子右下角的两个牛角座是否存在调换\r\n");
				//编码器C有值,请检查：1.编码器B与C接口接线是否调换了 2.驱动器B、C接口是否存在调换 3.板子的牛角座是否存在调换
			}
			else if( fabs(robot.MOTOR_D.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机B时,检测到编码器接口D的信号.\r\n自检时电机B是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口B、D是否存在调换\r\n否: 1.检查驱动器的B、D接口是否存在调换 2.检查板子右下角的两个牛角座是否存在调换\r\n");
				//编码器D有值,请检查：1.编码器B与D接口接线是否调换了 2.驱动器B、D接口是否存在调换 3.板子的牛角座是否存在调换
			}
			else if( fabs(robot.MOTOR_B.Encoder) < 0.05f && PI_MotorB.Output > 5000 ) 
			{
				any_printf(USART1,"驱动电机B时,未检测到编码器信号.\r\n自检时电机B是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口B是否存在松动或未接\r\n否: 1.检查电机B的电源线是否存在松动或未接 2.检查板子右下角左边的牛角座是否松动或未接\r\n");
				//编码器没有值,对应接口PWM值较大,请检查：1.编码器接口是否松动或未接 2.电机正负极接头是否松动或者未接
			}
		}
		any_printf(USART1,"================= 电机B ================\r\n\r\n");
		//检查完毕,复位PI控制器,防止下一个电机检查受到干扰
		PI_Controller_Reset(&PI_MotorA);
		PI_Controller_Reset(&PI_MotorB);
		PI_Controller_Reset(&PI_MotorC);
		PI_Controller_Reset(&PI_MotorD);
	}
	
	#if defined OMNI_CAR || defined _4WD_CAR || defined MEC_CAR
	
	else if( timecore>RATE*4 && timecore<RATE*6 )//C轮控制
	{
		robot.MOTOR_A.Target = 0.0f;robot.MOTOR_B.Target = 0.0f;
		robot.MOTOR_C.Target = 0.5f;robot.MOTOR_D.Target = 0.0f;
		ResponseControl();
		PWMA = 0;PWMB = 0;PWMD = 0;
	} 
	else if( timecore==RATE*6 )//C轮检查
	{
		any_printf(USART1,"================= 电机C ================\r\n");
		
		if( robot.MOTOR_C.Encoder > 0.4f && robot.MOTOR_C.Encoder < 0.6f )
		{
			any_printf(USART1,"电机C正常.\r\n");
		}
		else
		{
			errflag = 1; //出现错误
			
			if( robot.MOTOR_C.Encoder < - 0.2f ) 
			{
				any_printf(USART1,"检测到C电机编码器极性错误.\r\n");
				any_printf(USART1,"请检查：\r\n1.车型选择是否正确\r\n2.电机C的正负极是否存在线序错误\r\n3.电机C的编码器AB相线序是否错误\r\n");
				//对应接口出现负值,请检查：1.车型是否正确 2.电机的正负级是否反接 3.编码器AB相的线序是否正确
			}
			else if( fabs(robot.MOTOR_A.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机C时,检测到编码器接口A的信号.\r\n自检时电机C是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口A、C是否存在调换\r\n否: 1.检查驱动器的A、C接口是否存在调换 2.检查板子右下角的两个牛角座是否存在调换\r\n");
				//编码器B有值,请检查：1.编码器接口C与接口A接线是否调换了 2.驱动器的C、A接口是否存在调换 3.板子的牛角座是否存在调换
			}
			else if( fabs(robot.MOTOR_B.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机C时,检测到编码器接口B的信号.\r\n自检时电机C是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口B、C是否存在调换\r\n否: 1.检查驱动器的B、C接口是否存在调换 2.检查板子右下角的两个牛角座是否存在调换\r\n");
				//编码器C有值,请检查：1.编码器C与B接口接线是否调换了 2.驱动器C、B接口是否存在调换 3.板子的牛角座是否存在调换
			}
			else if( fabs(robot.MOTOR_D.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机C时,检测到编码器接口D的信号.\r\n自检时电机C是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口C、D是否存在调换\r\n否: 1.检查驱动器的C、D接口是否存在调换\r\n");
				//编码器D有值,请检查：1.编码器C与D接口接线是否调换了 2.驱动器C、D接口是否存在调换 
			}
			else if( fabs(robot.MOTOR_C.Encoder) < 0.05f && PI_MotorC.Output > 5000 ) 
			{
				any_printf(USART1,"驱动电机C时,未检测到编码器信号.\r\n自检时电机C是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口C是否存在松动或未接\r\n否: 1.检查电机C的电源线是否存在松动或未接 2.检查板子右下角右边的牛角座是否松动或未接\r\n");
				//编码器没有值,对应接口PWM值较大,请检查：1.编码器接口是否松动或未接 2.电机正负极接头是否松动或者未接
			}
		}
		any_printf(USART1,"================= 电机C ================\r\n\r\n");
		//检查完毕,复位PI控制器,防止下一个电机检查受到干扰
		PI_Controller_Reset(&PI_MotorA);
		PI_Controller_Reset(&PI_MotorB);
		PI_Controller_Reset(&PI_MotorC);
		PI_Controller_Reset(&PI_MotorD);
	}
	
	#endif /* if defined OMNI_CAR _4WD_CAR MEC_CAR */
	
	#if defined _4WD_CAR || defined MEC_CAR
	
	else if( timecore>RATE*6 && timecore<RATE*8 )//D轮控制
	{
		robot.MOTOR_A.Target = 0.0f;robot.MOTOR_B.Target = 0.0f;
		robot.MOTOR_C.Target = 0.0f;robot.MOTOR_D.Target = 0.5f;
		ResponseControl();
		PWMA = 0;PWMB = 0;PWMC = 0;
	}
	else if( timecore==RATE*8  )//D轮检查
	{
		any_printf(USART1,"================= 电机D ================\r\n");
		
		if( robot.MOTOR_D.Encoder > 0.4f && robot.MOTOR_D.Encoder < 0.6f )
		{
			any_printf(USART1,"电机D正常.\r\n");
		}
		else
		{
			errflag = 1; //出现错误
			
			if( robot.MOTOR_D.Encoder < - 0.2f ) 
			{
				any_printf(USART1,"检测到D电机编码器极性错误.\r\n");
				any_printf(USART1,"请检查：\r\n1.车型选择是否正确\r\n2.电机D的正负极是否存在线序错误\r\n3.电机D的编码器AB相线序是否错误\r\n");
				//对应接口出现负值,请检查：1.车型是否正确 2.电机的正负级是否反接 3.编码器AB相的线序是否正确
			}
			else if( fabs(robot.MOTOR_A.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机D时,检测到编码器接口A的信号.\r\n自检时电机D是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口A、D是否存在调换\r\n否: 1.检查驱动器的A、D接口是否存在调换 2.检查板子右下角的两个牛角座是否存在调换\r\n");
				//编码器B有值,请检查：1.编码器接口D与接口A接线是否调换了 2.驱动器的D、A接口是否存在调换 3.板子的牛角座是否存在调换
			}
			else if( fabs(robot.MOTOR_B.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机D时,检测到编码器接口B的信号.\r\n自检时电机D是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口B、D是否存在调换\r\n否: 1.检查驱动器的B、D接口是否存在调换 2.检查板子右下角的两个牛角座是否存在调换\r\n");
				//编码器C有值,请检查：1.编码器D与B接口接线是否调换了 2.驱动器D、B接口是否存在调换 3.板子的牛角座是否存在调换
			}
			else if( fabs(robot.MOTOR_C.Encoder) > 0.2f ) 
			{
				any_printf(USART1,"驱动电机D时,检测到编码器接口C的信号.\r\n自检时电机D是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口C、D是否存在调换\r\n否: 1.检查驱动器的C、D接口是否存在调换\r\n");
				//编码器D有值,请检查：1.编码器C与D接口接线是否调换了 2.驱动器C、D接口是否存在调换 
			}
			else if( fabs(robot.MOTOR_D.Encoder) < 0.05f && PI_MotorD.Output > 5000 ) 
			{
				any_printf(USART1,"驱动电机D时,未检测到编码器信号.\r\n自检时电机D是否转动?\r\n");
				any_printf(USART1,"是: 1.检查编码器接口D是否存在松动或未接\r\n否: 1.检查电机D的电源线是否存在松动或未接 2.检查板子右下角右边的牛角座是否松动或未接\r\n");
				//编码器没有值,对应接口PWM值较大,请检查：1.编码器接口是否松动或未接 2.电机正负极接头是否松动或者未接
			}
		}
		
		any_printf(USART1,"================= 电机D ================\r\n");
		
		//检查完毕,复位PI控制器,防止下一个电机检查受到干扰
		PI_Controller_Reset(&PI_MotorA);
		PI_Controller_Reset(&PI_MotorB);
		PI_Controller_Reset(&PI_MotorC);
		PI_Controller_Reset(&PI_MotorD);
	}
	#endif /* if defined  _4WD_CAR MEC_CAR */
	
	else
	{
		check_ready = 1;
		robot.MOTOR_A.Target = 0.0f;
		robot.MOTOR_B.Target = 0.0f;
		robot.MOTOR_C.Target = 0.0f;
		robot.MOTOR_D.Target = 0.0f;
		Set_Pwm(0,0,0,0,0);
		any_printf(USART1,"\r\n\r\n");
		Buzzer_AddTask(1,20);//蜂鸣器提示表示已完成深度自检
		
		//深度自检结束且无报错,恢复挂起的任务.
		if( 0 == errflag  )
			if( data_TaskHandle!=NULL ) vTaskResume(data_TaskHandle);
		
		timecore = 0;
		errflag = 0;
	}

	return check_ready;
	
}
