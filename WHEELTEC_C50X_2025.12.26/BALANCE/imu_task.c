#include "imu_task.h"

//imu数据结构体
IMU_DATA_t imu;

void MPU6050_task(void *pvParameters)
{
    u32 lastWakeTime = getSysTickCnt();
    while(1)
    {
        //This task runs at 100Hz
        //此任务以100Hz的频率运行
        vTaskDelayUntil(&lastWakeTime, F2T(IMU_TASK_RATE));

        //Read the gyroscope zero before starting
        //开机前，读取陀螺仪零点
        if(SysVal.Time_count<CONTROL_DELAY)
        {	
            ImuData_copy(&imu.Deviation_gyro,&imu.gyro);
            ImuData_copy(&imu.Deviation_accel,&imu.accel);
        }
        //Get acceleration sensor data
        MPU6050_Get_Accelscope();

        //Get gyroscope data
        MPU6050_Get_Gyroscope(); //得到陀螺仪数据

    }
}

void ICM20948_task(void *pvParameters)
{
    u32 lastWakeTime = getSysTickCnt();
    while(1)
    {
        //This task runs at 100Hz
        //此任务以100Hz的频率运行
        vTaskDelayUntil(&lastWakeTime, F2T(IMU_TASK_RATE));

        //Read the gyroscope zero before starting
        //开机前，读取陀螺仪零点
        if(SysVal.Time_count<CONTROL_DELAY)
        {
            ImuData_copy(&imu.Deviation_gyro,&imu.gyro);
            ImuData_copy(&imu.Deviation_accel,&imu.accel);
        }

        //Get acceleration sensor data
        ICM20948_Get_Accel(); //得到加速度传感器数据

        //Get gyroscope data
        ICM20948_Get_Gyroscope(); //得到陀螺仪数据

#if 0 // 未使用磁力计数据,不开启采集
        static u8 mag_count=0;
        //Get magnetometer data
        mag_count++;
        if(mag_count>=13)	 //磁力计最大采样速率为8hz，因此每进入这个函数13次才采样一次磁力计信息
        {
            invMSMagRead(&magnet[0], &magnet[1], &magnet[2]); //得到磁力计数据
            mag_count=0;
        }
#endif
    }
}


/**************************************************************************
函数功能：复制imu的数值
入口参数：需要赋值的结构体变量，被复制的结构体变量
返回  值：无
作    者：WHEELTEC
**************************************************************************/
void ImuData_copy(IMU_BASE_t* req_val,const IMU_BASE_t* copied_val)
{
	req_val->x = copied_val -> x;
	req_val->y = copied_val -> y;
	req_val->z = copied_val -> z;
}


