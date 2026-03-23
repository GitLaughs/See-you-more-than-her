#include "MPU6050.h"
#include "imu_task.h"
#include "I2C.h"

/**************************************************************************
Function: Initialize TIM2 as the encoder interface mode
Input   : Ax, Ay, Az: The original reading of the accelerometer on the x,y, and z axes (plus or minus)
Output  : 0: success, others: error code
函数功能：得到加速度值(原始值)
**************************************************************************/
void MPU6050_Get_Accelscope(void)
{
    imu.accel.x =(I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_XOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_XOUT_L); //读取X轴加速度计
    imu.accel.y =(I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_YOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_YOUT_L); //读取X轴加速度计
    imu.accel.z =(I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_ZOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_ZOUT_L); //读取Z轴加速度计

    if(SysVal.Time_count<CONTROL_DELAY) // 10 seconds before starting //开机前10秒
    {
		
    }
    else //10 seconds after starting //开机10秒后
    {
        //Save the raw data to update zero by clicking the user button
        //保存原始数据用于单击用户按键更新零点
		imu.Original_accel.x = imu.accel.x;
		imu.Original_accel.y = imu.accel.y;
		imu.Original_accel.z = imu.accel.z;

        //Removes zero drift data
        //去除零点漂移的数据
        imu.accel.x = imu.Original_accel.x - imu.Deviation_accel.x ;
        imu.accel.y = imu.Original_accel.y - imu.Deviation_accel.y ;
        imu.accel.z = imu.Original_accel.z - imu.Deviation_accel.z + 16384;
    }

}

/**************************************************************************
Function: Initialize TIM2 as the encoder interface mode
Input   : Gx, Gy, Gz: raw readings (plus or minus) of the x,y, and z axes of the gyroscope
Output  : 0: success, others: error code
函数功能：获得陀螺仪值(原始值)
**************************************************************************/
void MPU6050_Get_Gyroscope(void)
{
    imu.gyro.x = (I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_XOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_XOUT_L);    //读取X轴陀螺仪
    imu.gyro.y = (I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_YOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_YOUT_L);    //读取Y轴陀螺仪
    imu.gyro.z = (I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_ZOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_ZOUT_L);    //读取Z轴陀螺仪

    if(SysVal.Time_count<CONTROL_DELAY) // 10 seconds before starting //开机前10秒
    {

    }
    else //10 seconds after starting //开机10秒后
    {

        //Save the raw data to update zero by clicking the user button
        //保存原始数据用于单击用户按键更新零点
		imu.Original_gyro.x = imu.gyro.x;
		imu.Original_gyro.y = imu.gyro.y;
		imu.Original_gyro.z = imu.gyro.z;

        //Removes zero drift data
        //去除零点漂移的数据
		imu.gyro.x = imu.Original_gyro.x - imu.Deviation_gyro.x;
		imu.gyro.y = imu.Original_gyro.y - imu.Deviation_gyro.y;
		imu.gyro.z = imu.Original_gyro.z - imu.Deviation_gyro.z;
    }
}

/**************************************************************************
Function: The new ADC data is updated to FIFO array for filtering
Input   : ax，ay，az：x，y, z-axis acceleration data；gx，gy，gz：x. Y, z-axis angular acceleration data
Output  : none
函数功能：将新的ADC数据更新到 FIFO数组，进行滤波处理
入口参数：ax，ay，az：x，y，z轴加速度数据；gx，gy，gz：x，y，z轴角加速度数据
返回  值：无
**************************************************************************/
int16_t  MPU6050_FIFO[6][11];
void  MPU6050_newValues(int16_t ax,int16_t ay,int16_t az,int16_t gx,int16_t gy,int16_t gz)
{
    unsigned char i ;
    int32_t sum=0;
    for(i=1; i<10; i++) {	//FIFO 操作
        MPU6050_FIFO[0][i-1]=MPU6050_FIFO[0][i];
        MPU6050_FIFO[1][i-1]=MPU6050_FIFO[1][i];
        MPU6050_FIFO[2][i-1]=MPU6050_FIFO[2][i];
        MPU6050_FIFO[3][i-1]=MPU6050_FIFO[3][i];
        MPU6050_FIFO[4][i-1]=MPU6050_FIFO[4][i];
        MPU6050_FIFO[5][i-1]=MPU6050_FIFO[5][i];
    }
    MPU6050_FIFO[0][9]=ax;//将新的数据放置到 数据的最后面
    MPU6050_FIFO[1][9]=ay;
    MPU6050_FIFO[2][9]=az;
    MPU6050_FIFO[3][9]=gx;
    MPU6050_FIFO[4][9]=gy;
    MPU6050_FIFO[5][9]=gz;

    sum=0;
    for(i=0; i<10; i++) {	//求当前数组的合，再取平均值
        sum+=MPU6050_FIFO[0][i];
    }
    MPU6050_FIFO[0][10]=sum/10;

    sum=0;
    for(i=0; i<10; i++) {
        sum+=MPU6050_FIFO[1][i];
    }
    MPU6050_FIFO[1][10]=sum/10;

    sum=0;
    for(i=0; i<10; i++) {
        sum+=MPU6050_FIFO[2][i];
    }
    MPU6050_FIFO[2][10]=sum/10;

    sum=0;
    for(i=0; i<10; i++) {
        sum+=MPU6050_FIFO[3][i];
    }
    MPU6050_FIFO[3][10]=sum/10;

    sum=0;
    for(i=0; i<10; i++) {
        sum+=MPU6050_FIFO[4][i];
    }
    MPU6050_FIFO[4][10]=sum/10;

    sum=0;
    for(i=0; i<10; i++) {
        sum+=MPU6050_FIFO[5][i];
    }
    MPU6050_FIFO[5][10]=sum/10;
}

/**************************************************************************
Function: Setting the clock source of mpu6050
Input   : source：Clock source number
Output  : none
函数功能：设置  MPU6050 的时钟源
入口参数：source：时钟源编号
返回  值：无
 * CLK_SEL | Clock Source
 * --------+--------------------------------------
 * 0       | Internal oscillator
 * 1       | PLL with X Gyro reference
 * 2       | PLL with Y Gyro reference
 * 3       | PLL with Z Gyro reference
 * 4       | PLL with external 32.768kHz reference
 * 5       | PLL with external 19.2MHz reference
 * 6       | Reserved
 * 7       | Stops the clock and keeps the timing generator in reset
**************************************************************************/
void MPU6050_setClockSource(uint8_t source) {
    I2C_WriteBits(devAddr, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_BIT, MPU6050_PWR1_CLKSEL_LENGTH, source);
}

/** Set full-scale gyroscope range.
 * @param range New full-scale gyroscope range value
 * @see getFullScaleRange()
 * @see MPU6050_GYRO_FS_250
 * @see MPU6050_RA_GYRO_CONFIG
 * @see MPU6050_GCONFIG_FS_SEL_BIT
 * @see MPU6050_GCONFIG_FS_SEL_LENGTH
 */
void MPU6050_setFullScaleGyroRange(uint8_t range) {
    I2C_WriteBits(devAddr, MPU6050_RA_GYRO_CONFIG, MPU6050_GCONFIG_FS_SEL_BIT, MPU6050_GCONFIG_FS_SEL_LENGTH, range);
}

/**************************************************************************
Function: Setting the maximum range of mpu6050 accelerometer
Input   : range：Acceleration maximum range number
Output  : none
函数功能：设置 MPU6050 加速度计的最大量程
入口参数：range：加速度最大量程编号
返回  值：无
**************************************************************************/
//#define MPU6050_ACCEL_FS_2          0x00  		//===最大量程+-2G
//#define MPU6050_ACCEL_FS_4          0x01			//===最大量程+-4G
//#define MPU6050_ACCEL_FS_8          0x02			//===最大量程+-8G
//#define MPU6050_ACCEL_FS_16         0x03			//===最大量程+-16G
void MPU6050_setFullScaleAccelRange(uint8_t range) {
    I2C_WriteBits(devAddr, MPU6050_RA_ACCEL_CONFIG, MPU6050_ACONFIG_AFS_SEL_BIT, MPU6050_ACONFIG_AFS_SEL_LENGTH, range);
}

/**************************************************************************
Function: Set mpu6050 to sleep mode or not
Input   : enable：1，sleep；0，work；
Output  : none
函数功能：设置 MPU6050 是否进入睡眠模式
入口参数：enable：1，睡觉；0，工作；
返回  值：无
**************************************************************************/
void MPU6050_setSleepEnabled(uint8_t enabled) {
    I2C_WriteOneBit(devAddr, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_SLEEP_BIT, enabled);
}

/**************************************************************************
Function: Read identity
Input   : none
Output  : 0x68
函数功能：读取  MPU6050 WHO_AM_I 标识
入口参数：无
返回  值：0x68
**************************************************************************/
uint8_t MPU6050_getDeviceID(void) {
    return I2C_ReadOneByte(devAddr,MPU6050_RA_WHO_AM_I);
}

/**************************************************************************
Function: Check whether mpu6050 is connected
Input   : none
Output  : 1：Connected；0：Not connected
函数功能：检测MPU6050 是否已经连接
入口参数：无
返回  值：1：已连接；0：未连接
**************************************************************************/
uint8_t MPU6050_testConnection(void) {
    if(MPU6050_getDeviceID() == 0x68)  //0b01101000;
        return 1;
    else return 0;
}

/**************************************************************************
Function: Setting whether mpu6050 is the host of aux I2C cable
Input   : enable：1，yes；0;not
Output  : none
函数功能：设置 MPU6050 是否为AUX I2C线的主机
入口参数：enable：1，是；0：否
返回  值：无
**************************************************************************/
void MPU6050_setI2CMasterModeEnabled(uint8_t enabled) {
    I2C_WriteOneBit(devAddr, MPU6050_RA_USER_CTRL, MPU6050_USERCTRL_I2C_MST_EN_BIT, enabled);
}

/**************************************************************************
Function: Setting whether mpu6050 is the host of aux I2C cable
Input   : enable：1，yes；0;not
Output  : none
函数功能：设置 MPU6050 是否为AUX I2C线的主机
入口参数：enable：1，是；0：否
返回  值：无
**************************************************************************/
void MPU6050_setI2CBypassEnabled(uint8_t enabled) {
    I2C_WriteOneBit(devAddr, MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_I2C_BYPASS_EN_BIT, enabled);
}

/**************************************************************************
Function: Read mpu6050 built-in temperature sensor data
Input   : none
Output  : Centigrade temperature
函数功能：读取MPU6050内置温度传感器数据
入口参数：无
返回  值：摄氏温度
**************************************************************************/
int Read_Temperature(void)
{
    float Temp;
    Temp=(I2C_ReadOneByte(devAddr,MPU6050_RA_TEMP_OUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_TEMP_OUT_L);
    if(Temp>32768) Temp-=65536;	//数据类型转换
    Temp=(36.53f+Temp/340)*10;	  //温度放大十倍存放
    return (int)Temp;
}

/**************************************************************************
Function: Initialize TIM2 as the encoder interface mode
Input   : LPF: Digital low-pass filtering frequency (Hz)
Output  : 0: Settings successful, others: Settings failed
函数功能：设置MPUrobot_select_init.h的数字低通滤波器
入口参数：lpf:数字低通滤波频率(Hz)
返回  值：0:设置成功, 其他:设置失败
**************************************************************************/
unsigned char MPU6050_Set_LPF(u16 lpf)
{
    u8 data=0;
    if(lpf>=188)data=1;
    else if(lpf>=98)data=2;
    else if(lpf>=42)data=3;
    else if(lpf>=20)data=4;
    else if(lpf>=10)data=5;
    else data=6;
    return I2C_WriteOneByte(devAddr,MPU6050_RA_CONFIG,data); //Set the digital lowpass filter//设置数字低通滤波器
}
/**************************************************************************
Function: Initialize TIM2 as the encoder interface mode
Input   : rate:4~1000(Hz)
Output  : 0: Settings successful, others: Settings failed
函数功能：设置MPUrobot_select_init.h的采样率(假定Fs=1KHz)
入口参数：rate:4~1000(Hz)
返回  值：0:设置成功, 其他:设置失败
**************************************************************************/
unsigned char MPU6050_Set_Rate(u16 rate)
{
    u8 data;
    if(rate>1000)rate=1000;
    if(rate<4)rate=4;
    data=1000/rate-1;
    data=I2C_WriteOneByte(devAddr,MPU6050_RA_SMPLRT_DIV,data);	//Set the digital lowpass filter//设置数字低通滤波器
    return MPU6050_Set_LPF(rate/2);	//Automatically sets LPF to half of the sampling rate //自动设置LPF为采样率的一半
}

/**************************************************************************
Function: initialization Mpu6050 to enter the available state
Input   : none
Output  : none
函数功能：初始化	MPU6050 以进入可用状态
入口参数：无
返回  值：无
**************************************************************************/
u8 MPU6050_initialize(void) {
//    MPU6050_setClockSource(MPU6050_CLOCK_PLL_YGYRO); //设置时钟
//    MPU6050_setFullScaleGyroRange(MPU6050_GYRO_FS_2000);//陀螺仪量程设置
//    MPU6050_setFullScaleAccelRange(MPU6050_ACCEL_FS_2);	//加速度度最大量程 +-2G
//    MPU6050_setSleepEnabled(0); //进入工作状态
//	  MPU6050_setI2CMasterModeEnabled(0);	 //不让MPU6050 控制AUXI2C
//	  MPU6050_setI2CBypassEnabled(0);	 //主控制器的I2C与	MPU6050的AUXI2C	直通关闭

    u8 res;
    //IIC_Init();  //Initialize the IIC bus //初始化IIC总线
    I2C_WriteOneByte(devAddr,MPU6050_RA_PWR_MGMT_1,0X80);	//Reset MPUrobot_select_init.h //复位MPUrobot_select_init.h
    delay_ms(200); //Delay 200 ms //延时200ms
    I2C_WriteOneByte(devAddr,MPU6050_RA_PWR_MGMT_1,0X00);	//Wake mpurobot_select_init.h //唤醒MPUrobot_select_init.h

    //MPU6050_Set_Gyro_Fsr(1);  //Gyroscope sensor              //陀螺仪传感器,±500dps=±500°/s ±32768 (gyro/32768*500)*PI/180(rad/s)=gyro/3754.9(rad/s)
    MPU6050_setFullScaleGyroRange(MPU6050_GYRO_FS_500);
    //MPU6050_Set_Accel_Fsr(0);	//Acceleration sensor           //加速度传感器,±2g=±2*9.8m/s^2 ±32768 accel/32768*19.6=accel/1671.84
    MPU6050_setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
    MPU6050_Set_Rate(50);			//Set the sampling rate to 50Hz //设置采样率50Hz

    I2C_WriteOneByte(devAddr,MPU6050_RA_INT_ENABLE,0X00);	  //Turn off all interrupts //关闭所有中断
    I2C_WriteOneByte(devAddr,MPU6050_RA_USER_CTRL,0X00);	//The I2C main mode is off //I2C主模式关闭
    I2C_WriteOneByte(devAddr,MPU6050_RA_FIFO_EN,0X00);	  //Close the FIFO //关闭FIFO
    //The INT pin is low, enabling bypass mode to read the magnetometer directly
    //INT引脚低电平有效，开启bypass模式，可以直接读取磁力计
    I2C_WriteOneByte(devAddr,MPU6050_RA_INT_PIN_CFG,0X80);
    //Read the ID of MPU6050
    //读取MPU6050的ID
    res=I2C_ReadOneByte(devAddr,MPU6050_RA_WHO_AM_I);
    if(res==MPU6050_DEFAULT_ADDRESS) //The device ID is correct, The correct device ID depends on the AD pin //器件ID正确, 器件ID的正确取决于AD引脚
    {
        I2C_WriteOneByte(devAddr,MPU6050_RA_PWR_MGMT_1,0X01);	//Set CLKSEL,PLL X axis as reference //设置CLKSEL,PLL X轴为参考
        I2C_WriteOneByte(devAddr,MPU6050_RA_PWR_MGMT_2,0X00);	//Acceleration and gyroscope both work //加速度与陀螺仪都工作
        MPU6050_Set_Rate(50);	                      //Set the sampling rate to 50Hz //设置采样率为50Hz
    } else return 1;
    return 0;
}


//------------------End of File----------------------------
