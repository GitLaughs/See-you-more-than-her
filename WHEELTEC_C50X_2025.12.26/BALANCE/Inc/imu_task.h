#ifndef __IMU_TASK_H
#define __IMU_TASK_H

#include "system.h"

#define IMU_TASK_PRIO      3
#define IMU_STK_SIZE       256
#define IMU_TASK_RATE      RATE_100_HZ

typedef struct{
	short x;
	short y;
	short z;
}IMU_BASE_t;

//imu三轴数据,包含偏差值和原始值
typedef struct{
	IMU_BASE_t Deviation_accel;//偏差值,用于纠正误差.该值采集后不会发生改变
	IMU_BASE_t Deviation_gyro;
	IMU_BASE_t Original_accel;
	IMU_BASE_t Original_gyro;
	IMU_BASE_t gyro;
	IMU_BASE_t accel;
}IMU_DATA_t;

extern IMU_DATA_t imu;

//对外接口
void MPU6050_task(void *pvParameters);
void ICM20948_task(void *pvParameters);

void ImuData_copy(IMU_BASE_t* req_val,const IMU_BASE_t* copied_val);

#endif
