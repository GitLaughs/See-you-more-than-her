#include "uartx_callback.h"
#include "data_task.h"

//用于接收数据的结构体
RECEIVE_DATA Receive_Data;


// 对外IP地址存储区
u8 Received_IP[4] = {0, 0, 0, 0};          // 存储最终确认的IP地址
// IP获取功能相关变量
u8 Last_IP[4] = {0, 0, 0, 0};              // 存储上一次接收的IP（用于连续性校验）
u8 IP_Frame_Valid = 0;                     // IP帧有效标志
u16 IP_Valid_Count = 0;                    // 连续相同IP计数器
u8 Debug_Flag=0;
/**************************************************************************、
Functionality: This function receives serial commands to control the movement of a robot.
Input Parameters:
Which serial port (e.g.,USART1,USART3,etc.)
The received data from the serial port
Return Value: None
Author: WHEELTEC
函数功能：接收串口命令用于控制机器人运动
入口参数：哪一个串口，接收到的数据
返回  值：无
作    者：WHEELTEC
**************************************************************************/
static void UartxControll_Callback(USART_TypeDef* USARTx,uint8_t Usart_Receive)
{
	static uint16_t Count = 0;
	static USART_TypeDef* Last_Uartx;
	//禁止多串口同时共用函数,一个时间只能有一个接口对机器人进行控制.
	if( USARTx != Last_Uartx && Count!=0 )
	{
		Count = 0;
	}
	Last_Uartx = USARTx;
	//Fill the array with serial data
	//串口数据填入数组
	Receive_Data.buffer[Count]=Usart_Receive;

	// Ensure that the first data in the array is FRAME_HEADER
	//确保数组第一个数据为FRAME_HEADER
	if(Usart_Receive == FRAME_HEADER||Count>0)
		Count++;
	else
		Count=0;

	if (Count == 11) //Verify the length of the packet //验证数据包的长度
	{
		Count=0; //Prepare for the serial port data to be refill into the array //为串口数据重新填入数组做准备
		if(Receive_Data.buffer[10] == FRAME_TAIL) //Verify the frame tail of the packet //验证数据包的帧尾
		{
			//Data exclusionary or bit check calculation, mode 0 is sent data check
			//数据异或位校验计算，模式0是发送数据校验
			if(Receive_Data.buffer[9] ==Check_BCC(Receive_Data.buffer,9))
			{	
				//串口、CAN控制命令丢失计数清零
				robot_control.command_lostcount = 0;

				if(Receive_Data.buffer[1]==0)
				{
					//根据串口的不同来确定是什么控制方式
					       if(USARTx==USART1) Set_Control_Mode(_USART_Control);
					else if( USARTx==USART3 ) Set_Control_Mode(_ROS_Control);
					
					charger.AllowRecharge = 0;
					//Calculate the target speed of three axis from serial data, unit m/s
					//从串口数据求三轴目标速度， 单位m/s
					robot_control.Vx = XYZ_Target_Speed_transition(Receive_Data.buffer[3],Receive_Data.buffer[4]);
					robot_control.Vy = XYZ_Target_Speed_transition(Receive_Data.buffer[5],Receive_Data.buffer[6]);
					robot_control.Vz = XYZ_Target_Speed_transition(Receive_Data.buffer[7],Receive_Data.buffer[8]);
					
					#if defined AKM_CAR 
					//Convert the Z-axis speed to the Ackerman left front wheel steering angle \
					for an Ackerman-type vehicle, and then input the calculated angle into the \
					inverse kinematics to control the vehicle.	
					//阿克曼车型将Z轴速度转换为阿克曼左前轮转角,最终再输入到运动学逆解中控制小车.
					robot_control.Vz = Akm_Vz_to_Angle( robot_control.Vx , robot_control.Vz );
					#endif
				}
				else if( Receive_Data.buffer[1]==1 || Receive_Data.buffer[1]==2 )
				{
					//开启自动回充
					charger.AllowRecharge = 1;
					
					//阿克曼机器人通过标志位判断导航控制
					#if defined AKM_CAR
						if(Receive_Data.buffer[1]==2) charger.NavWalk=1; 
						else charger.NavWalk=0;
					
					//其他机器人通过红外判断是否使用导航控制
					#else
						if(Receive_Data.buffer[1]==1 && charger.RED_STATE==0) charger.NavWalk = 1; 
					#endif
					
					//Calculate the target speed of three axis from serial data, unit m/s
					//从串口数据求三轴目标速度，单位m/s
					charger.Up_MoveX = XYZ_Target_Speed_transition(Receive_Data.buffer[3],Receive_Data.buffer[4]);
					charger.Up_MoveY = XYZ_Target_Speed_transition(Receive_Data.buffer[5],Receive_Data.buffer[6]);
					charger.Up_MoveZ = XYZ_Target_Speed_transition(Receive_Data.buffer[7],Receive_Data.buffer[8]);
					
					#if defined AKM_CAR 
					//Convert the Z-axis speed to the Ackerman letf front wheel steering angle \
					for an Ackerman-type vehicle, and then input the calculated angle into the \
					inverse kinematics to control the vehicle.	
					//阿克曼车型将Z轴速度转换为阿克曼左前轮转角,最终再输入到运动学逆解中控制小车.
					charger.Up_MoveZ = Akm_Vz_to_Angle( charger.Up_MoveX , charger.Up_MoveZ );
					#endif

				}
				else if( Receive_Data.buffer[1]==3 )
				{
					//Set the speed of the infrared interconnection, unit m/s
					//设置红外对接的速度大小，单位m/s
					charger.Dock_MoveX = XYZ_Target_Speed_transition(Receive_Data.buffer[3],Receive_Data.buffer[4]);
					charger.Dock_MoveY = XYZ_Target_Speed_transition(Receive_Data.buffer[5],Receive_Data.buffer[6]);
					charger.Dock_MoveZ = XYZ_Target_Speed_transition(Receive_Data.buffer[7],Receive_Data.buffer[8]);
				}
				 // ================== IP帧处理（新增） ================== //
                else if(Receive_Data.buffer[1] == 0xFF)  // IP帧标识
                {
                    uint8_t temp_ip[4];
                    
                    // 提取接收到的IP（索引3-6）
                    temp_ip[0] = Receive_Data.buffer[3];
                    temp_ip[1] = Receive_Data.buffer[4];
                    temp_ip[2] = Receive_Data.buffer[5];
                    temp_ip[3] = Receive_Data.buffer[6];
                    
                    // 检查本次IP是否与上一次相同
                    if(temp_ip[0] == Last_IP[0] &&
                       temp_ip[1] == Last_IP[1] &&
                       temp_ip[2] == Last_IP[2] &&
                       temp_ip[3] == Last_IP[3])
                    {
                        // IP连续相同，计数递增
                        IP_Valid_Count++;
                        
                        // 收到足够多次连续相同的IP，确认无误
                        if(IP_Valid_Count >= IP_CONFIRM_COUNT)
                        {
                            // 将确认的IP写入最终结果
                            Received_IP[0] = temp_ip[0];
                            Received_IP[1] = temp_ip[1];
                            Received_IP[2] = temp_ip[2];
                            Received_IP[3] = temp_ip[3];
                            IP_Frame_Valid = 1;          // 标记IP有效
                        }
                    }
                    else
                    {
                        // IP变化了，重置计数器，保存新的IP到Last_IP
                        IP_Valid_Count = 1;  // 从1开始计数
                        Last_IP[0] = temp_ip[0];
                        Last_IP[1] = temp_ip[1];
                        Last_IP[2] = temp_ip[2];
                        Last_IP[3] = temp_ip[3];
                        
                    }
                }
			}
		}
	}
}


/**************************************************************************
Function: Serial port 1 receives interrupted
Input   : none
Output  : none
函数功能：串口1接收中断
入口参数：无
返 回 值：无
**************************************************************************/
int USART1_IRQHandler(void)
{
    u8 Usart_Receive;

    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) //Check if data is received //判断是否接收到数据
    {
        Usart_Receive = USART_ReceiveData(USART1);//Read the data //读取数据
		
		// Data is not processed until 25 seconds after startup
		//开机 CONTROL_DELAY 秒前不处理数据
        if(SysVal.Time_count<CONTROL_DELAY)
            return 0;	//前期不进入中断

		//处理机器人串口控制数据
        UartxControll_Callback(USART1,Usart_Receive);
    }
    return 0;
}

/**************************************************************************
Function: Refresh the OLED screen
Input   : none
Output  : none
函数功能：串口4接收中断
入口参数：无
返回  值：无
**************************************************************************/
int UART4_IRQHandler(void)
{
    int Usart_Receive;
    if(USART_GetITStatus(UART4, USART_IT_RXNE) != RESET) //Check if data is received //判断是否接收到数据
    {
        static u8 Flag_PID,i,j,Receive[50],Last_Usart_Receive;
        static float Data;

        Usart_Receive=UART4->DR; //Read the data //读取数据
		
		//允许蓝牙复位系统,进入BootLoader烧录程序
        _System_Reset_(Usart_Receive);
		
		// Data is not processed until 10 seconds after startup
		//开机10秒前不处理数据
        if(SysVal.Time_count<CONTROL_DELAY)
            return 0;
		//调试模式
		RobotControl_SetDebugLevel(Usart_Receive);
		//捕获APP连接和断开时的AT指令,防止干扰
        if(AT_Command_Capture(Usart_Receive)) return 1;
		
		//10 seconds after startup, press the forward button of APP to enter APP control mode
		//The APP controls the flag position 1 and the other flag position 0
		//开机10秒之后，按下APP的前进键进入APP控制模式
        if(Usart_Receive==0x41&&Last_Usart_Receive==0x41&&(Get_Control_Mode(_APP_Control))==0)
            Set_Control_Mode(_APP_Control);

        Last_Usart_Receive=Usart_Receive;

        //Enter the APP steering control interface
        //进入APP转向控制界面
        if(Usart_Receive==0x4B)
            appkey.TurnPage = 1;
        else if(Usart_Receive==0x49||Usart_Receive==0x4A)
            appkey.TurnPage = 0;

        if( 0 == appkey.TurnPage )
        {
            //App rocker control interface command
            //APP摇杆控制界面命令
            if(Usart_Receive>=0x41&&Usart_Receive<=0x48)
            {
                appkey.DirectionFlag=Usart_Receive-0x40;
            }
            else	if(Usart_Receive<=8)
            {
                appkey.DirectionFlag=Usart_Receive;
            }
            else  appkey.DirectionFlag=0;
        }
        else if( 1 == appkey.TurnPage )
        {
            //APP steering control interface command
            //APP转向控制界面命令
            if     (Usart_Receive==0x43) appkey.TurnFlag = 2; //Right rotation //右自转
            else if(Usart_Receive==0x47) appkey.TurnFlag = 1; //Left rotation  //左自转
            else                         appkey.TurnFlag = 0;

            if     (Usart_Receive==0x41||Usart_Receive==0x45) appkey.DirectionFlag=Usart_Receive-0x40;
            else  appkey.DirectionFlag=0;
        }

        if(Usart_Receive==0x58)  robot_control.rc_speed+=100; //Accelerate the keys, +100mm/s //加速按键，+100mm/s
        if(Usart_Receive==0x59)  robot_control.rc_speed-=100; //Slow down buttons,   -100mm/s //减速按键，-100mm/s

        // The following is the communication with the APP debugging interface
        //以下是与APP调试界面通讯
        if(Usart_Receive==0x7B) Flag_PID=1;   //The start bit of the APP parameter instruction //APP参数指令起始位
        if(Usart_Receive==0x7D) Flag_PID=2;   //The APP parameter instruction stops the bit    //APP参数指令停止位
		
        if( Usart_Receive=='b' ) charger.AllowRecharge = !charger.AllowRecharge;
        else if(Usart_Receive=='m'  )
        {
            oled.page++;
            if(oled.page>oled.MAX_PAGE) oled.page=1;
        }

        if(Flag_PID==1) //Collect data //采集数据
        {
            Receive[i]=Usart_Receive;
            i++;
        }
        if(Flag_PID==2) //Analyze the data //分析数据
        {
            if(Receive[3]==0x50) 	 appkey.ParamSendflag = 1; //发送数据到app显示
            else if( Receive[3]==0x57 ) appkey.ParamSaveFlag = 1; //保存参数到stm32 flash
            else  if(Receive[1]!=0x23)
            {
                for(j=i; j>=4; j--)
                {
                    Data+=(Receive[j-1]-48)*pow(10,i-j);
                }
                switch(Receive[1])
                {
					case 0x30: robot_control.rc_speed = Data ; break; //修改机器人的遥控速度
					//PID参数设置
					case 0x31:
						Set_Robot_PI_Param(Data,-1);/* 使用-1代表相应的参数不设置 */
						break;
					case 0x32:
						Set_Robot_PI_Param(-1,Data);/* 使用-1代表相应的参数不设置 */
						break;
					
					case 0x33: robot_control.smooth_MotorStep = Data/1000 ; break; 
					case 0x34: robot_control.smooth_ServoStep = Data ;  break;         
					case 0x35: Akm_Servo.Max=Data;  break;        
					case 0x36: Akm_Servo.Min=Data; break;                
					case 0x37: Akm_Servo.Mid=Data; break;                
					case 0x38: robot_control.LineDiffParam = Data; break;       
                }
            }
            else if( Receive[1]==0x23 ) //APP上点击“发送所有数据”处理方法
            {
                float num;
                u8 dataIndex=0;
                float dataArray[9];

                if( i<=50 ) //数据在可接受范围
                {
                    Receive[i]='}'; //补充帧尾

                    for(u8 kk=0; Receive[kk]!='}'; kk++)
                    {
                        if( Receive[kk]>='0' && Receive[kk]<='9' )
                        {
                            num = num*10 + ( Receive[kk] - '0' );
                        }
                        else if( Receive[kk]==':' )
                        {
                            dataArray[dataIndex++] = num;
                            num = 0;
                        }

                    }
                    //处理最后一个数据
                    dataArray[dataIndex] = num;

                    //数据赋值
                    robot_control.rc_speed = dataArray[0];

                    //kp、ki
                    Set_Robot_PI_Param(dataArray[1],dataArray[2]);
					
                    //速度平滑系数
                    robot_control.smooth_MotorStep = dataArray[3]/1000;
                    robot_control.smooth_ServoStep = dataArray[4];

                    //阿克曼转向数据
                    Akm_Servo.Max = dataArray[5];
                    Akm_Servo.Min = dataArray[6];
                    Akm_Servo.Mid = dataArray[7];
					
					//纠偏系数
					robot_control.LineDiffParam = dataArray[8];
					
                    //主动发送已修改的数据到app显示
                    appkey.ParamSendflag=1;
                }
            }

            //Relevant flag position is cleared
            //相关标志位清零
            Flag_PID=0;
            i=0;
            j=0;
            Data=0;
            memset(Receive, 0, sizeof(u8)*50); //Clear the array to zero//数组清零
        }
		
		//对遥控的速度做最大值、最小值限制
		robot_control.rc_speed = target_limit_float(robot_control.rc_speed,0,robot_control.limt_max_speed*1000);
		
		//限制纠偏参数范围在0~100
		robot_control.LineDiffParam = robot_control.LineDiffParam > 100 ? 100 : robot_control.LineDiffParam;
    }
    return 0;
}
/**************************************************************************
Function: Serial port 3 receives interrupted
Input   : none
Output  : none
函数功能：串口3接收中断
入口参数：无
返回  值：无
**************************************************************************/
int USART3_IRQHandler(void)
{
    u8 Usart_Receive;

    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) //Check if data is received //判断是否接收到数据
    {
        Usart_Receive = USART_ReceiveData(USART3);//Read the data //读取数据

		// Data is not processed until 10 seconds after startup
		//开机10秒前不处理数据
        if(SysVal.Time_count<CONTROL_DELAY)
            return 0;

		//处理机器人串口控制数据
        UartxControll_Callback(USART3,Usart_Receive);
    }
    return 0;
}

/**************************************************************************
Function: After the top 8 and low 8 figures are integrated into a short type data, the unit reduction is converted
Input   : 8 bits high, 8 bits low
Output  : The target velocity of the robot on the X/Y/Z axis
函数功能：将上位机发过来的高8位和低8位数据整合成一个short型数据后，再做单位还原换算
入口参数：高8位，低8位
返回  值：机器人X/Y/Z轴的目标速度
**************************************************************************/
static float XYZ_Target_Speed_transition(u8 High,u8 Low)
{
    //Data conversion intermediate variable
    //数据转换的中间变量
    short transition;

    //将高8位和低8位整合成一个16位的short型数据
    //The high 8 and low 8 bits are integrated into a 16-bit short data
    transition=((High<<8)+Low);
    return
        transition/1000+(transition%1000)*0.001; //Unit conversion, mm/s->m/s //单位转换, mm/s->m/s
}


//蓝牙AT指令抓包，防止指令干扰到机器人正常的蓝牙通信
static u8 AT_Command_Capture(u8 uart_recv)
{
    /*
    蓝牙链接时发送的字符，00:11:22:33:44:55为蓝牙的MAC地址
    +CONNECTING<<00:11:22:33:44:55\r\n
    +CONNECTED\r\n
    共44个字符

    蓝牙断开时发送的字符
    +DISC:SUCCESS\r\n
    +READY\r\n
    +PAIRABLE\r\n
    共34个字符
    \r -> 0x0D
    \n -> 0x0A
    */

    static u8 pointer = 0; //蓝牙接受时指针记录器
    static u8 bt_line = 0; //表示现在在第几行
    static u8 disconnect = 0;
    static u8 connect = 0;

    //断开连接
    static char* BlueTooth_Disconnect[3]= {"+DISC:SUCCESS\r\n","+READY\r\n","+PAIRABLE\r\n"};

    //开始连接
    static char* BlueTooth_Connect[2]= {"+CONNECTING<<00:00:00:00:00:00\r\n","+CONNECTED\r\n"};


    //特殊标识符，开始警惕(使用时要-1)
    if(uart_recv=='+')
    {
        bt_line++,pointer=0; //收到‘+’，表示切换了行数
        disconnect++,connect++;
        return 1;//抓包，禁止控制
    }

    if(bt_line!=0)
    {
        pointer++;

        //开始追踪数据是否符合断开的特征，符合时全部屏蔽，不符合时取消屏蔽
        if(uart_recv == BlueTooth_Disconnect[bt_line-1][pointer])
        {
            disconnect++;
            if(disconnect==34) disconnect=0,connect=0,bt_line=0,pointer=0;
            return 1;//抓包，禁止控制
        }

        //追踪连接特征 (bt_line==1&&connect>=13)区段是蓝牙MAC地址，每一个蓝牙MAC地址都不相同，所以直接屏蔽过去
        else if(uart_recv == BlueTooth_Connect[bt_line-1][pointer] || (bt_line==1&&connect>=13) )
        {
            connect++;
            if(connect==44) connect=0,disconnect=0,bt_line=0,pointer=0;
            return 1;//抓包，禁止控制
        }

        //在抓包期间收到其他命令，停止抓包
        else
        {
            disconnect = 0;
            connect = 0;
            bt_line = 0;
            pointer = 0;
            return 0;//非禁止数据，可以控制
        }
    }

    return 0;//非禁止数据，可以控制
}



//软复位进BootLoader区域
static void _System_Reset_(u8 uart_recv)
{
    static u8 res_buf[5];
    static u8 res_count=0;

    res_buf[res_count]=uart_recv;

    if( uart_recv=='r'||res_count>0 )
        res_count++;
    else
        res_count = 0;

    if(res_count==5)
    {
        res_count = 0;
        //接受到上位机请求的复位字符“reset”，执行软件复位
        if( res_buf[0]=='r'&&res_buf[1]=='e'&&res_buf[2]=='s'&&res_buf[3]=='e'&&res_buf[4]=='t' )
        {
            NVIC_SystemReset();//进行软件复位，复位后执行 BootLoader 程序
        }
    }
}



//调试模式
void RobotControl_SetDebugLevel(char uart_recv)
{
	
	static char res_buf[4];
	static char res_count=0;
	
	res_buf[res_count]=uart_recv;
	
	if( uart_recv=='L'||res_count>0 )
		res_count++;
	else
		res_count = 0;
	
	if(res_count==4)
	{
		res_count = 0;
		
		if( res_buf[0]=='L'&&res_buf[1]=='O'&&res_buf[2]=='G')
		{
			switch( res_buf[3] )
			{
				case '3': Debug_Flag= 1; break;
				default: Debug_Flag= 0; break;
			}
			//操作提示音
		}
	}
}


