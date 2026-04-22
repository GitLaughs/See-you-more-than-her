#include "turn_on_wheeltec_robot/wheeltec_robot.h"
#include "rclcpp/rclcpp.hpp"
#include "turn_on_wheeltec_robot/Quaternion_Solution.h"
#include "wheeltec_robot_msg/msg/data.hpp"     // CHANGE

//sensor_msgs::Imu Mpu6050;//Instantiate an IMU object //实例化IMU对象 
sensor_msgs::msg::Imu Mpu6050;
using std::placeholders::_1;
using namespace std;
rclcpp::Node::SharedPtr node_handle = nullptr;

bool distance_flag=false;
bool check_AutoCharge_data = false;
/**************************************
Date: January 28, 2021
Function: The main function, ROS initialization, creates the Robot_control object through the Turn_on_robot class and automatically calls the constructor initialization
功能: 主函数，ROS初始化，通过turn_on_robot类创建Robot_control对象并自动调用构造函数初始化
***************************************/

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    turn_on_robot Robot_Control;
    Robot_Control.Control();
    return 0;
}
/**************************************
Date: January 28, 2021
Function: Data conversion function
功能: 数据转换函数
***************************************/

short turn_on_robot::IMU_Trans(uint8_t Data_High,uint8_t Data_Low)
{
  short transition_16;
  transition_16 = 0;
  transition_16 |=  Data_High<<8;   
  transition_16 |=  Data_Low;
  return transition_16;     
}
float turn_on_robot::Odom_Trans(uint8_t Data_High,uint8_t Data_Low)
{
  float data_return;
  short transition_16;
  transition_16 = 0;
  transition_16 |=  Data_High<<8;  //Get the high 8 bits of data   //获取数据的高8位
  transition_16 |=  Data_Low;      //Get the lowest 8 bits of data //获取数据的低8位
  data_return   =  (transition_16 / 1000)+(transition_16 % 1000)*0.001; // The speed unit is changed from mm/s to m/s //速度单位从mm/s转换为m/s
  return data_return;
}
/**************************************
Date: January 28, 2021
Function: The speed topic subscription Callback function, according to the subscribed instructions through the serial port command control of the lower computer
功能: 速度话题订阅回调函数Callback，根据订阅的指令通过串口发指令控制下位机
***************************************/
void turn_on_robot::Cmd_Vel_Callback(const geometry_msgs::msg::Twist::SharedPtr twist_aux)
{
  short  transition;  //intermediate variable //中间变量

  Send_Data.tx[0]=FRAME_HEADER; //frame head 0x7B //帧头0X7B
  Send_Data.tx[1] = 0; //set aside //预留位
  Send_Data.tx[2] = 0; //set aside //预留位

  //The target velocity of the X-axis of the robot
  //机器人x轴的目标线速度
  transition=0;
  transition = twist_aux->linear.x*1000; //将浮点数放大一千倍，简化传输
  Send_Data.tx[4] = transition;     //取数据的低8位
  Send_Data.tx[3] = transition>>8;  //取数据的高8位

  //The target velocity of the Y-axis of the robot
  //机器人y轴的目标线速度
  transition=0;
  transition = twist_aux->linear.y*1000;
  Send_Data.tx[6] = transition;
  Send_Data.tx[5] = transition>>8;

  //The target angular velocity of the robot's Z axis
  //机器人z轴的目标角速度
  transition=0;
  transition = twist_aux->angular.z*1000;
  Send_Data.tx[8] = transition;
  Send_Data.tx[7] = transition>>8;

  Send_Data.tx[9]=Check_Sum(9,SEND_DATA_CHECK); //For the BCC check bits, see the Check_Sum function //BCC校验位，规则参见Check_Sum函数
  Send_Data.tx[10]=FRAME_TAIL; //frame tail 0x7D //帧尾0X7D
  try
  {
    Stm32_Serial.write(Send_Data.tx,sizeof (Send_Data.tx)); //Sends data to the downloader via serial port //通过串口向下位机发送数据 
  }
  catch (serial::IOException& e)   
  {
    RCLCPP_ERROR(this->get_logger(),("Unable to send data through serial port")); //If sending data fails, an error message is printed //如果发送数据失败，打印错误信息
  }
}

/**************************************
Date: January 28, 2021
Function: Publish the IMU data topic
功能: 发布IMU数据话题
***************************************/
void turn_on_robot::Publish_ImuSensor()
{
  sensor_msgs::msg::Imu Imu_Data_Pub; //Instantiate IMU topic data //实例化IMU话题数据
  Imu_Data_Pub.header.stamp = rclcpp::Node::now(); 
  Imu_Data_Pub.header.frame_id = gyro_frame_id; //IMU corresponds to TF coordinates, which is required to use the robot_pose_ekf feature pack 
                                                //IMU对应TF坐标，使用robot_pose_ekf功能包需要设置此项
  Imu_Data_Pub.orientation.x = Mpu6050.orientation.x; //A quaternion represents a three-axis attitude //四元数表达三轴姿态
  Imu_Data_Pub.orientation.y = Mpu6050.orientation.y; 
  Imu_Data_Pub.orientation.z = Mpu6050.orientation.z;
  Imu_Data_Pub.orientation.w = Mpu6050.orientation.w;
  Imu_Data_Pub.orientation_covariance[0] = 1e6; //Three-axis attitude covariance matrix //三轴姿态协方差矩阵
  Imu_Data_Pub.orientation_covariance[4] = 1e6;
  Imu_Data_Pub.orientation_covariance[8] = 1e-6;
  Imu_Data_Pub.angular_velocity.x = Mpu6050.angular_velocity.x; //Triaxial angular velocity //三轴角速度
  Imu_Data_Pub.angular_velocity.y = Mpu6050.angular_velocity.y;
  Imu_Data_Pub.angular_velocity.z = Mpu6050.angular_velocity.z;
  Imu_Data_Pub.angular_velocity_covariance[0] = 1e6; //Triaxial angular velocity covariance matrix //三轴角速度协方差矩阵
  Imu_Data_Pub.angular_velocity_covariance[4] = 1e6;
  Imu_Data_Pub.angular_velocity_covariance[8] = 1e-6;
  Imu_Data_Pub.linear_acceleration.x = Mpu6050.linear_acceleration.x; //Triaxial acceleration //三轴线性加速度
  Imu_Data_Pub.linear_acceleration.y = Mpu6050.linear_acceleration.y; 
  Imu_Data_Pub.linear_acceleration.z = Mpu6050.linear_acceleration.z;  
  imu_publisher->publish(Imu_Data_Pub); //Pub IMU topic //发布IMU话题
}
/**************************************
Date: January 28, 2021
Function: Publish the odometer topic, Contains position, attitude, triaxial velocity, angular velocity about triaxial, TF parent-child coordinates, and covariance matrix
功能: 发布里程计话题，包含位置、姿态、三轴速度、绕三轴角速度、TF父子坐标、协方差矩阵
***************************************/

void turn_on_robot::Publish_Odom()
{
    //Convert the Z-axis rotation Angle into a quaternion for expression 
    //把Z轴转角转换为四元数进行表达
    tf2::Quaternion q;
    q.setRPY(0,0,Robot_Pos.Z);
    geometry_msgs::msg::Quaternion odom_quat=tf2::toMsg(q);
    
    nav_msgs::msg::Odometry odom; //Instance the odometer topic data //实例化里程计话题数据
    odom.header.stamp = rclcpp::Node::now(); ; 
    odom.header.frame_id = odom_frame_id; // Odometer TF parent coordinates //里程计TF父坐标
    odom.pose.pose.position.x = Robot_Pos.X; //Position //位置
    odom.pose.pose.position.y = Robot_Pos.Y;
    odom.pose.pose.position.z = Robot_Pos.Z;
    odom.pose.pose.orientation = odom_quat; //Posture, Quaternion converted by Z-axis rotation //姿态，通过Z轴转角转换的四元数

    odom.child_frame_id = robot_frame_id; // Odometer TF subcoordinates //里程计TF子坐标
    odom.twist.twist.linear.x =  Robot_Vel.X; //Speed in the X direction //X方向速度
    odom.twist.twist.linear.y =  Robot_Vel.Y; //Speed in the Y direction //Y方向速度
    odom.twist.twist.angular.z = Robot_Vel.Z; //Angular velocity around the Z axis //绕Z轴角速度 

    //There are two types of this matrix, which are used when the robot is at rest and when it is moving.Extended Kalman Filtering officially provides 2 matrices for the robot_pose_ekf feature pack
    //这个矩阵有两种，分别在机器人静止和运动的时候使用。扩展卡尔曼滤波官方提供的2个矩阵，用于robot_pose_ekf功能包
    if(Robot_Vel.X== 0&&Robot_Vel.Y== 0&&Robot_Vel.Z== 0)
      //If the velocity is zero, it means that the error of the encoder will be relatively small, and the data of the encoder will be considered more reliable
      //如果velocity是零，说明编码器的误差会比较小，认为编码器数据更可靠
      memcpy(&odom.pose.covariance, odom_pose_covariance2, sizeof(odom_pose_covariance2)),
      memcpy(&odom.twist.covariance, odom_twist_covariance2, sizeof(odom_twist_covariance2));
    else
      //If the velocity of the trolley is non-zero, considering the sliding error that may be brought by the encoder in motion, the data of IMU is considered to be more reliable
      //如果小车velocity非零，考虑到运动中编码器可能带来的滑动误差，认为imu的数据更可靠
      memcpy(&odom.pose.covariance, odom_pose_covariance, sizeof(odom_pose_covariance)),
      memcpy(&odom.twist.covariance, odom_twist_covariance, sizeof(odom_twist_covariance));       
      odom_publisher->publish(odom); //Pub odometer topic //发布里程计话题
}
/**************************************
Date: January 28, 2021
Function: Publish voltage-related information
功能: 发布电压相关信息
***************************************/
void turn_on_robot::Publish_Voltage()
{
    std_msgs::msg::Float32 voltage_msgs; //Define the data type of the power supply voltage publishing topic //定义电源电压发布话题的数据类型
    static float Count_Voltage_Pub=0;
    if(Count_Voltage_Pub++>10)
      {
        Count_Voltage_Pub=0;  
        voltage_msgs.data = Power_voltage; //The power supply voltage is obtained //电源供电的电压获取
        voltage_publisher->publish(voltage_msgs); //Post the power supply voltage topic unit: V, volt //发布电源电压话题单位：V、伏特
      }
}
/**************************************
Date: 
Function: 
功能: 发布超声波测量距离相关信息
***************************************/
void turn_on_robot::Publish_distance()
{
  wheeltec_robot_msg::msg::Supersonic distance_msg;
  distance_msg.header.stamp=rclcpp::Node::now();
  if(distance.A>=0||distance.A<=6) distance_msg.distance_a = distance.A;
  if(distance.B>=0||distance.B<=6) distance_msg.distance_b = distance.B;
  if(distance.C>=0||distance.C<=6) distance_msg.distance_c = distance.C;
  if(distance.D>=0||distance.D<=6) distance_msg.distance_d = distance.D;
  if(distance.E>=0||distance.E<=6) distance_msg.distance_e = distance.E;
  if(distance.F>=0||distance.F<=6) distance_msg.distance_f = distance.F;
  if(distance.G>=0||distance.G<=6) distance_msg.distance_g = distance.G;
  if(distance.H>=0||distance.H<=6) distance_msg.distance_h = distance.H;

  distance_publisher->publish(distance_msg);
}

/**************************************
Date: January 28, 2021
Function: Serial port communication check function, packet n has a byte, the NTH -1 byte is the check bit, the NTH byte bit frame end.Bit XOR results from byte 1 to byte n-2 are compared with byte n-1, which is a BCC check
Input parameter: Count_Number: Check the first few bytes of the packet
功能: 串口通讯校验函数，数据包n有个字节，第n-1个字节为校验位，第n个字节位帧尾。第1个字节到第n-2个字节数据按位异或的结果与第n-1个字节对比，即为BCC校验
输入参数： Count_Number：数据包前几个字节加入校验   mode：对发送数据还是接收数据进行校验
***************************************/
unsigned char turn_on_robot::Check_Sum(unsigned char Count_Number,unsigned char mode)
{
  unsigned char check_sum=0,k;
  
  if(mode==0) //Receive data mode //接收数据模式
  {
   for(k=0;k<Count_Number;k++)
    {
     check_sum=check_sum^Receive_Data.rx[k]; //By bit or by bit //按位异或
     }
  }
  if(mode==1) //Send data mode //发送数据模式
  {
   for(k=0;k<Count_Number;k++)
    {
     check_sum=check_sum^Send_Data.tx[k]; //By bit or by bit //按位异或
     }
  }

  if(mode==3)
  {
   for(k=0;k<Count_Number;k++)
    {
     check_sum=check_sum^Distance_Data.rx[k]; //By bit or by bit //按位异或
     }
  }
  return check_sum; //Returns the bitwise XOR result //返回按位异或结果
}


//自动回充专用校验位
unsigned char turn_on_robot::Check_Sum_AutoCharge(unsigned char Count_Number,unsigned char mode)
{
  unsigned char check_sum=0,k;
  if(mode==0) //Receive data mode //接收数据模式
  {
   for(k=0;k<Count_Number;k++)
    {
     check_sum=check_sum^Receive_AutoCharge_Data.rx[k]; //By bit or by bit //按位异或
    }
  }

  return check_sum;
}
/**************************************
Date: January 28, 2021
Function: The serial port reads and verifies the data sent by the lower computer, and then the data is converted to international units
功能: 通过串口读取并校验下位机发送过来的数据，然后数据转换为国际单位
***************************************/

//bool turn_on_robot::Get_Sensor_Data()Get_Sensor_Data_New
bool turn_on_robot::Get_Sensor_Data()
{ 
  short transition_16=0; //Intermediate variable //中间变量
  uint8_t check=0,check2=0,check3=0, error=1,error2=1,error3=1,Receive_Data_Pr[1]; //Temporary variable to save the data of the lower machine //临时变量，保存下位机数据
  static int count,count2,count3; //Static variable for counting //静态变量，用于计数
  static uint8_t Last_Receive;

  Stm32_Serial.read(Receive_Data_Pr,sizeof(Receive_Data_Pr)); //Read the data sent by the lower computer through the serial port //通过串口读取下位机发送过来的数据
  // printf("%x ",Receive_Data_Pr[0]);
  /*//View the received raw data directly and debug it for use//直接查看接收到的原始数据，调试使用
  ROS_INFO("%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x",
  Receive_Data_Pr[0],Receive_Data_Pr[1],Receive_Data_Pr[2],Receive_Data_Pr[3],Receive_Data_Pr[4],Receive_Data_Pr[5],Receive_Data_Pr[6],Receive_Data_Pr[7],
  Receive_Data_Pr[8],Receive_Data_Pr[9],Receive_Data_Pr[10],Receive_Data_Pr[11],Receive_Data_Pr[12],Receive_Data_Pr[13],Receive_Data_Pr[14],Receive_Data_Pr[15],
  Receive_Data_Pr[16],Receive_Data_Pr[17],Receive_Data_Pr[18],Receive_Data_Pr[19],Receive_Data_Pr[20],Receive_Data_Pr[21],Receive_Data_Pr[22],Receive_Data_Pr[23]);
  */  

  Receive_Data.rx[count] = Receive_Data_Pr[0]; //Fill the array with serial data //串口数据填入数组
  Receive_AutoCharge_Data.rx[count3] = Receive_Data_Pr[0];
  Distance_Data.rx[count2] = Receive_Data_Pr[0];

  Receive_Data.Frame_Header = Receive_Data.rx[0]; //The first part of the data is the frame header 0X7B //数据的第一位是帧头0X7B
  Receive_Data.Frame_Tail = Receive_Data.rx[23];  //The last bit of data is frame tail 0X7D //数据的最后一位是帧尾0X7D

  //接收到自动回充数据的帧头、上一个数据是超声波的帧尾，表明自动回充数据开始到来
  if((Receive_Data_Pr[0] == AutoCharge_HEADER && Last_Receive == Distance_TAIL)||count3>0)
    count3++;
  else
    count3=0;

  //接收到超声波的帧头、上一个24字节的帧尾，表明超声波数据开始到来
  if((Receive_Data_Pr[0] == Distance_HEADER && Last_Receive == FRAME_TAIL)||count2>0)
    count2++;
  else
    count2=0;

  //接受到24字节的帧头、自动回充数据的帧尾，表明24字节数据开始到来
  if((Receive_Data_Pr[0] == FRAME_HEADER && Last_Receive == AutoCharge_TAIL)||count>0) //Ensure that the first data in the array is FRAME_HEADER //确保数组第一个数据为FRAME_HEADER
    count++;
  else 
    count=0;

  Last_Receive = Receive_Data_Pr[0]; //保存本次接收到的数据

  //自动回充数据处理
  if(count3 == AutoCharge_DATA_SIZE)
  {
    count3=0;
    if(Receive_AutoCharge_Data.rx[AutoCharge_DATA_SIZE-1]==AutoCharge_TAIL) //确认帧尾   
    {
      check3 =  Check_Sum_AutoCharge(6,0);//校验位计算    
      if(check3 == Receive_AutoCharge_Data.rx[AutoCharge_DATA_SIZE-2]) //校验正确
      {
        error3=0;
      }
      if(error3 == 0)  //校验正确开始赋值
      {
        transition_16 = 0;
        transition_16   |=  Receive_AutoCharge_Data.rx[1]<<8;
        transition_16   |=  Receive_AutoCharge_Data.rx[2]; 
        Charging_Current = transition_16/1000+(transition_16 % 1000)*0.001; //充电电流 
        
        Red =  Receive_AutoCharge_Data.rx[3];    //红外接受状态
        Charging = Receive_AutoCharge_Data.rx[4];//小车充电状态

        check_AutoCharge_data = true; //数据成功接收标志位
      }
    }
  }

  //超声波数据
  if(count2 == Distance_DATA_size)
  {
    count2 = 0;
    if(Distance_Data.rx[Distance_DATA_size-1]==Distance_TAIL)
    {
      check2 = Check_Sum(17,3);

      //RCLCPP_INFO(get_logger(), "Received count2 %d",check2);
      if(check2 == Distance_Data.rx[Distance_DATA_size-2])
      {
        error2=0;

      }
      if(error2==0)
      {
        distance.A = ((short)((Distance_Data.rx[1]<<8) |(Distance_Data.rx[2] )))/1000.0f;
        distance.B = ((short)((Distance_Data.rx[3]<<8) |(Distance_Data.rx[4] )))/1000.0f;
        distance.C = ((short)((Distance_Data.rx[5]<<8) |(Distance_Data.rx[6] )))/1000.0f;
        distance.D = ((short)((Distance_Data.rx[7]<<8) |(Distance_Data.rx[8] )))/1000.0f;
        distance.E = ((short)((Distance_Data.rx[9]<<8) |(Distance_Data.rx[10])))/1000.0f;
        distance.F = ((short)((Distance_Data.rx[11]<<8)|(Distance_Data.rx[12])))/1000.0f;
        distance.G = ((short)((Distance_Data.rx[13]<<8)|(Distance_Data.rx[14])))/1000.0f;
        distance.H = ((short)((Distance_Data.rx[15]<<8)|(Distance_Data.rx[16])))/1000.0f;
        distance_flag=true;
      }
    }
  }

  if(count == 24) //Verify the length of the packet //验证数据包的长度
  {
    count=0;  //Prepare for the serial port data to be refill into the array //为串口数据重新填入数组做准备
    if(Receive_Data.Frame_Tail == FRAME_TAIL) //Verify the frame tail of the packet //验证数据包的帧尾
    {
      check=Check_Sum(22,READ_DATA_CHECK);  //BCC check passes or two packets are interlaced //BCC校验通过或者两组数据包交错

      if(check == Receive_Data.rx[22])  
      {
        error=0;  //XOR bit check successful //异或位校验成功
      }
      if(error == 0)
      {

        Receive_Data.Flag_Stop=Receive_Data.rx[1]; //set aside //预留位
        Robot_Vel.X = Odom_Trans(Receive_Data.rx[2],Receive_Data.rx[3]); //Get the speed of the moving chassis in the X direction //获取运动底盘X方向速度
          
        Robot_Vel.Y = Odom_Trans(Receive_Data.rx[4],Receive_Data.rx[5]); //Get the speed of the moving chassis in the Y direction, The Y speed is only valid in the omnidirectional mobile robot chassis
                                                                          //获取运动底盘Y方向速度，Y速度仅在全向移动机器人底盘有效
        Robot_Vel.Z = Odom_Trans(Receive_Data.rx[6],Receive_Data.rx[7]); //Get the speed of the moving chassis in the Z direction //获取运动底盘Z方向速度   
          
        //MPU6050 stands for IMU only and does not refer to a specific model. It can be either MPU6050 or MPU9250
        //Mpu6050仅代表IMU，不指代特定型号，既可以是MPU6050也可以是MPU9250
        Mpu6050_Data.accele_x_data = IMU_Trans(Receive_Data.rx[8],Receive_Data.rx[9]);   //Get the X-axis acceleration of the IMU     //获取IMU的X轴加速度  
        Mpu6050_Data.accele_y_data = IMU_Trans(Receive_Data.rx[10],Receive_Data.rx[11]); //Get the Y-axis acceleration of the IMU     //获取IMU的Y轴加速度
        Mpu6050_Data.accele_z_data = IMU_Trans(Receive_Data.rx[12],Receive_Data.rx[13]); //Get the Z-axis acceleration of the IMU     //获取IMU的Z轴加速度
        Mpu6050_Data.gyros_x_data = IMU_Trans(Receive_Data.rx[14],Receive_Data.rx[15]);  //Get the X-axis angular velocity of the IMU //获取IMU的X轴角速度  
        Mpu6050_Data.gyros_y_data = IMU_Trans(Receive_Data.rx[16],Receive_Data.rx[17]);  //Get the Y-axis angular velocity of the IMU //获取IMU的Y轴角速度  
        Mpu6050_Data.gyros_z_data = IMU_Trans(Receive_Data.rx[18],Receive_Data.rx[19]);  //Get the Z-axis angular velocity of the IMU //获取IMU的Z轴角速度  
        //Linear acceleration unit conversion is related to the range of IMU initialization of STM32, where the range is ±2g=19.6m/s^2
        //线性加速度单位转化，和STM32的IMU初始化的时候的量程有关,这里量程±2g=19.6m/s^2
        Mpu6050.linear_acceleration.x = Mpu6050_Data.accele_x_data / ACCEl_RATIO;
        Mpu6050.linear_acceleration.y = Mpu6050_Data.accele_y_data / ACCEl_RATIO;
        Mpu6050.linear_acceleration.z = Mpu6050_Data.accele_z_data / ACCEl_RATIO;
        //The gyroscope unit conversion is related to the range of STM32's IMU when initialized. Here, the range of IMU's gyroscope is ±500°/s
        //Because the robot generally has a slow Z-axis speed, reducing the range can improve the accuracy
        //陀螺仪单位转化，和STM32的IMU初始化的时候的量程有关，这里IMU的陀螺仪的量程是±500°/s
        //因为机器人一般Z轴速度不快，降低量程可以提高精度
        Mpu6050.angular_velocity.x =  Mpu6050_Data.gyros_x_data * GYROSCOPE_RATIO;
        Mpu6050.angular_velocity.y =  Mpu6050_Data.gyros_y_data * GYROSCOPE_RATIO;
        Mpu6050.angular_velocity.z =  Mpu6050_Data.gyros_z_data * GYROSCOPE_RATIO;

        //Get the battery voltage
        //获取电池电压
        transition_16 = 0;
        transition_16 |=  Receive_Data.rx[20]<<8;
        transition_16 |=  Receive_Data.rx[21];  
        Power_voltage = transition_16/1000+(transition_16 % 1000)*0.001; //Unit conversion millivolt(mv)->volt(v) //单位转换毫伏(mv)->伏(v)
          
        return true;

      }
      
    }

  }
  return false;
}
/**************************************
Date: January 28, 2021
Function: Loop access to the lower computer data and issue topics
功能: 循环获取下位机数据与发布话题
***************************************/

void turn_on_robot::Control()
{
  _Last_Time = rclcpp::Node::now();
  while(rclcpp::ok())
  {
  try
  {
    //Retrieves time interval, which is used to integrate velocity to obtain displacement (mileage) 
    //获取时间间隔，用于积分速度获得位移(里程)
    _Now = rclcpp::Node::now();
    Sampling_Time = (_Now - _Last_Time).seconds();  //Retrieves time interval, which is used to integrate velocity to obtain displacement (mileage) 
    
    //超声波数据
    if(distance_flag==true)  
    {
        Publish_distance();  //发布超声波数据
        distance_flag = false;
    }
    
    //The serial port reads and verifies the data sent by the lower computer, and then the data is converted to international units
    //通过串口读取并校验下位机发送过来的数据，然后数据转换为国际单位
    if (true == Get_Sensor_Data()) 
                                   //通过串口读取并校验下位机发送过来的数据，然后数据转换为国际单位
    {
      //Odometer error correction //里程计误差修正
      Robot_Vel.X = Robot_Vel.X*odom_x_scale;
      Robot_Vel.Y = Robot_Vel.Y*odom_y_scale;
      if( Robot_Vel.Z>=0 )
        Robot_Vel.Z = Robot_Vel.Z*odom_z_scale_positive;
      else
        Robot_Vel.Z = Robot_Vel.Z*odom_z_scale_negative;
      
      Robot_Pos.X+=(Robot_Vel.X * cos(Robot_Pos.Z) - Robot_Vel.Y * sin(Robot_Pos.Z)) * Sampling_Time; //Calculate the displacement in the X direction, unit: m //计算X方向的位移，单位：m
      Robot_Pos.Y+=(Robot_Vel.X * sin(Robot_Pos.Z) + Robot_Vel.Y * cos(Robot_Pos.Z)) * Sampling_Time; //Calculate the displacement in the Y direction, unit: m //计算Y方向的位移，单位：m
      Robot_Pos.Z+=Robot_Vel.Z * Sampling_Time; //The angular displacement about the Z axis, in rad //绕Z轴的角位移，单位：rad 

      //Calculate the three-axis attitude from the IMU with the angular velocity around the three-axis and the three-axis acceleration
      //通过IMU绕三轴角速度与三轴加速度计算三轴姿态
      Quaternion_Solution(Mpu6050.angular_velocity.x, Mpu6050.angular_velocity.y, Mpu6050.angular_velocity.z,\
                Mpu6050.linear_acceleration.x, Mpu6050.linear_acceleration.y, Mpu6050.linear_acceleration.z);

      Publish_Odom();      //Pub the speedometer topic //发布里程计话题
      Publish_ImuSensor(); //Pub the IMU topic //发布IMU话题    
      Publish_Voltage();   //Pub the topic of power supply voltage //发布电源电压话题

      _Last_Time = _Now; //Record the time and use it to calculate the time interval //记录时间，用于计算时间间隔
      
    }
    rclcpp::spin_some(this->get_node_base_interface());   //The loop waits for the callback function //循环等待回调函数
    }
    catch (const rclcpp::exceptions::RCLError & e )
    {
  RCLCPP_ERROR(this->get_logger(),"unexpectedly failed whith %s",e.what()); 
  }
    }
}

/**************************************
Date: January 28, 2021
Function: Constructor, executed only once, for initialization
功能: 构造函数, 只执行一次，用于初始化
***************************************/
turn_on_robot::turn_on_robot()
: rclcpp::Node ("wheeltec_robot")
{
  Sampling_Time=0;
  Power_voltage=0;
  //Clear the data
  //清空数据
  memset(&Robot_Pos, 0, sizeof(Robot_Pos));
  memset(&Robot_Vel, 0, sizeof(Robot_Vel));
  memset(&Receive_Data, 0, sizeof(Receive_Data)); 
  memset(&Send_Data, 0, sizeof(Send_Data));
  memset(&Mpu6050_Data, 0, sizeof(Mpu6050_Data));
  
  int serial_baud_rate = 115200;

  this->declare_parameter<int>("serial_baud_rate");
  this->declare_parameter<std::string>("usart_port_name", "/dev/wheeltec_controller");
  this->declare_parameter<std::string>("odom_frame_id", "odom");
  this->declare_parameter<std::string>("robot_frame_id", "base_footprint");
  this->declare_parameter<std::string>("gyro_frame_id", "gyro_link");
  this->declare_parameter<double>("odom_x_scale");
  this->declare_parameter<double>("odom_y_scale");
  this->declare_parameter<double>("odom_z_scale_positive");
  this->declare_parameter<double>("odom_z_scale_negative");

  this->get_parameter("serial_baud_rate", serial_baud_rate);//Communicate baud rate 115200 to the lower machine //和下位机通信波特率115200
  this->get_parameter("usart_port_name", usart_port_name);//Fixed serial port number //固定串口号
  this->get_parameter("odom_frame_id", odom_frame_id);//The odometer topic corresponds to the parent TF coordinate //里程计话题对应父TF坐标
  this->get_parameter("robot_frame_id", robot_frame_id);//The odometer topic corresponds to sub-TF coordinates //里程计话题对应子TF坐标
  this->get_parameter("gyro_frame_id", gyro_frame_id);//IMU topics correspond to TF coordinates //IMU话题对应TF坐标
  this->get_parameter("odom_x_scale", odom_x_scale);
  this->get_parameter("odom_y_scale", odom_y_scale);
  this->get_parameter("odom_z_scale_positive", odom_z_scale_positive);
  this->get_parameter("odom_z_scale_negative", odom_z_scale_negative);

  odom_publisher = create_publisher<nav_msgs::msg::Odometry>("odom", 2);//Create the odometer topic publisher //创建里程计话题发布者
  imu_publisher = create_publisher<sensor_msgs::msg::Imu>("imu/data_raw", 2); //Create an IMU topic publisher //创建IMU话题发布者
  voltage_publisher = create_publisher<std_msgs::msg::Float32>("PowerVoltage", 1);//Create a battery-voltage topic publisher //创建电池电压话题发布者
  distance_publisher = create_publisher<wheeltec_robot_msg::msg::Supersonic>("Distance", 1);//超声波数据


  //Set the velocity control command callback function
  //速度控制命令订阅回调函数设置
  Cmd_Vel_Sub = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", 2, std::bind(&turn_on_robot::Cmd_Vel_Callback, this, _1));

  RCLCPP_INFO(this->get_logger(),"wheeltec_robot Data ready"); //Prompt message //提示信息


  try
  { 
    //Attempts to initialize and open the serial port //尝试初始化与开启串口
    Stm32_Serial.setPort(usart_port_name); //Select the serial port number to enable //选择要开启的串口号
    Stm32_Serial.setBaudrate(serial_baud_rate); //Set the baud rate //设置波特率
    serial::Timeout _time = serial::Timeout::simpleTimeout(2000); //Timeout //超时等待
    Stm32_Serial.setTimeout(_time);
    Stm32_Serial.open(); //Open the serial port //开启串口
    Stm32_Serial.flushInput();
  }
  catch (serial::IOException& e)
  {
    RCLCPP_ERROR(this->get_logger(),"wheeltec_robot can not open serial port,Please check the serial port cable! "); //If opening the serial port fails, an error message is printed //如果开启串口失败，打印错误信息
  }
  if(Stm32_Serial.isOpen())
  {
    RCLCPP_INFO(this->get_logger(),"wheeltec_robot serial port opened"); //Serial port opened successfully //串口开启成功提示
  }
}
/**************************************
Date: January 28, 2021
Function: Destructor, executed only once and called by the system when an object ends its life cycle
功能: 析构函数，只执行一次，当对象结束其生命周期时系统会调用这个函数
***************************************/

turn_on_robot::~turn_on_robot()
{
  //Sends the stop motion command to the lower machine before the turn_on_robot object ends
  //对象turn_on_robot结束前向下位机发送停止运动命令
  Send_Data.tx[0]=FRAME_HEADER;
  Send_Data.tx[1] = 0;  
  Send_Data.tx[2] = 0; 

  //The target velocity of the X-axis of the robot //机器人X轴的目标线速度 
  Send_Data.tx[4] = 0;     
  Send_Data.tx[3] = 0;  

  //The target velocity of the Y-axis of the robot //机器人Y轴的目标线速度 
  Send_Data.tx[6] = 0;
  Send_Data.tx[5] = 0;  

  //The target velocity of the Z-axis of the robot //机器人Z轴的目标角速度 
  Send_Data.tx[8] = 0;  
  Send_Data.tx[7] = 0;    
  Send_Data.tx[9]=Check_Sum(9,SEND_DATA_CHECK); //Check the bits for the Check_Sum function //校验位，规则参见Check_Sum函数
  Send_Data.tx[10]=FRAME_TAIL; 

  try
  {
    Stm32_Serial.write(Send_Data.tx,sizeof (Send_Data.tx)); //Send data to the serial port //向串口发数据  
  }
  catch (serial::IOException& e)   
  {
    RCLCPP_ERROR(this->get_logger(),"Unable to send data through serial port"); //If sending data fails, an error message is printed //如果发送数据失败,打印错误信息
  }
  Stm32_Serial.close(); //Close the serial port //关闭串口  
  RCLCPP_INFO(this->get_logger(),"Shutting down"); //Prompt message //提示信息
  
}




