#include "ICM20948.h"
#include "imu_task.h"

ICM20948_ST_SENSOR_DATA gstGyroOffset = {0,0,0};

//磁力计数据
short magnet[3];

/**************************************************************************
Function: Initialize TIM2 as the encoder interface mode
Input   : Gx, Gy, Gz: raw readings (plus or minus) of the x,y, and z axes of the gyroscope
Output  : 0: success, others: error code
函数功能：获得陀螺仪值(原始值)
入口参数：gx,gy,gz:陀螺仪x,y,z轴的原始读数(带正负)
返回  值：0:成功, 其他:错误代码
**************************************************************************/
u8 ICM20948_Get_Gyroscope(void)
{
    u8 res;
    invMSGyroRead(&imu.gyro.x, &imu.gyro.y, &imu.gyro.z);
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

    return res;
}


u8 ICM20948_Get_Accel(void)
{
    u8 res;
    invMSAccelRead(&imu.accel.x, &imu.accel.y, &imu.accel.z); //得到加速度传感器数据
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

    return res;
}



uint8_t ICM20948_getDeviceID(void)
{
    return I2C_ReadOneByte(I2C_ADD_ICM20948, REG_ADD_WIA);
}

static bool invmsICM20948Check(void)
{
    bool bRet = false;
    if(REG_VAL_WIA == I2C_ReadOneByte(I2C_ADD_ICM20948, REG_ADD_WIA))
    {
        bRet = true;
    }
    return bRet;
}

void invMSInit(void)
{
    if(invmsICM20948Check())//检测是否能识别到ICM20498器件
    {
        invmsICM20948Init(); //ICM20498初始化
    }
    return;
}

static void invmsICM20948Init(void)
{
    /* user bank 0 register */
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0);
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_PWR_MIGMT_1,  REG_VAL_ALL_RGE_RESET);
    delay_ms(10);
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_PWR_MIGMT_1,  REG_VAL_RUN_MODE);

    /* user bank 2 register */
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_2);

    I2C_WriteOneByte( I2C_ADD_ICM20948, REG_ADD_GYRO_SMPLRT_DIV, 0x07);
    I2C_WriteOneByte( I2C_ADD_ICM20948, REG_ADD_GYRO_CONFIG_1,REG_VAL_BIT_GYRO_DLPCFG_6 | REG_VAL_BIT_GYRO_FS_500DPS | REG_VAL_BIT_GYRO_DLPF);

    I2C_WriteOneByte( I2C_ADD_ICM20948, REG_ADD_ACCEL_SMPLRT_DIV_2,  0x07);
    I2C_WriteOneByte( I2C_ADD_ICM20948, REG_ADD_ACCEL_CONFIG,REG_VAL_BIT_ACCEL_DLPCFG_6 | REG_VAL_BIT_ACCEL_FS_2g | REG_VAL_BIT_ACCEL_DLPF);

    /* user bank 0 register */
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0);

    delay_ms(100);
    /* offset */
    invmsICM20948GyroOffset();

    invmsICM20948MagCheck();

    invmsICM20948WriteSecondary( I2C_ADD_ICM20948_AK09916|I2C_ADD_ICM20948_AK09916_WRITE,REG_ADD_MAG_CNTL2, REG_VAL_MAG_MODE_100HZ);
    return;
}

//角速度读取的底层实现
static void invmsICM20948GyroRead(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
{
    uint8_t u8Buf[6];
    int16_t s16Buf[3] = {0};
    uint8_t i;
    int32_t s32OutBuf[3] = {0};
    static ICM20948_ST_AVG_DATA sstAvgBuf[3];
    static int16_t ss16c = 0;
    ss16c++;

    u8Buf[0]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_GYRO_XOUT_L);
    u8Buf[1]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_GYRO_XOUT_H);
    s16Buf[0]=	(u8Buf[1]<<8)|u8Buf[0];

    u8Buf[0]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_GYRO_YOUT_L);
    u8Buf[1]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_GYRO_YOUT_H);
    s16Buf[1]=	(u8Buf[1]<<8)|u8Buf[0];

    u8Buf[0]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_GYRO_ZOUT_L);
    u8Buf[1]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_GYRO_ZOUT_H);
    s16Buf[2]=	(u8Buf[1]<<8)|u8Buf[0];

#if 1
    for(i = 0; i < 3; i ++)
    {
        invmsICM20948CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
    }
    *ps16X = s32OutBuf[0] - gstGyroOffset.s16X;
    *ps16Y = s32OutBuf[1] - gstGyroOffset.s16Y;
    *ps16Z = s32OutBuf[2] - gstGyroOffset.s16Z;
#else
    *ps16X = s16Buf[0];
    *ps16Y = s16Buf[1];
    *ps16Z = s16Buf[2];
#endif
    return;
}

//加速度读取的底层实现
static void invmsICM20948AccelRead(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
{
    uint8_t u8Buf[2];
    int16_t s16Buf[3] = {0};
    uint8_t i;
    int32_t s32OutBuf[3] = {0};
    static ICM20948_ST_AVG_DATA sstAvgBuf[3];

    u8Buf[0]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_ACCEL_XOUT_L);
    u8Buf[1]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_ACCEL_XOUT_H);
    s16Buf[0]=	(u8Buf[1]<<8)|u8Buf[0];

    u8Buf[0]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_ACCEL_YOUT_L);
    u8Buf[1]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_ACCEL_YOUT_H);
    s16Buf[1]=	(u8Buf[1]<<8)|u8Buf[0];

    u8Buf[0]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_ACCEL_ZOUT_L);
    u8Buf[1]=I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_ACCEL_ZOUT_H);
    s16Buf[2]=	(u8Buf[1]<<8)|u8Buf[0];

#if 1
    for(i = 0; i < 3; i ++)
    {
        invmsICM20948CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
    }
    *ps16X = s32OutBuf[0];
    *ps16Y = s32OutBuf[1];
    *ps16Z = s32OutBuf[2];

#else
    *ps16X = s16Buf[0];
    *ps16Y = s16Buf[1];
    *ps16Z = s16Buf[2];
#endif
    return;

}

//磁力计读取的底层实现
//static void invmsICM20948MagRead(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
//{
//    uint8_t counter = 20;
//    uint8_t u8Data[MAG_DATA_LEN];
//    int16_t s16Buf[3] = {0};
//    uint8_t i;
//    int32_t s32OutBuf[3] = {0};
//    static ICM20948_ST_AVG_DATA sstAvgBuf[3];
//    while( counter>0 )
//    {
//        delay_ms(10);
//        invmsICM20948ReadSecondary( I2C_ADD_ICM20948_AK09916|I2C_ADD_ICM20948_AK09916_READ,
//                                    REG_ADD_MAG_ST2, 1, u8Data);

//        if ((u8Data[0] & 0x01) != 0)
//            break;

//        counter--;
//    }

//    if(counter != 0)
//    {
//        invmsICM20948ReadSecondary( I2C_ADD_ICM20948_AK09916|I2C_ADD_ICM20948_AK09916_READ,
//                                    REG_ADD_MAG_DATA,
//                                    MAG_DATA_LEN,
//                                    u8Data);
//        s16Buf[0] = ((int16_t)u8Data[1]<<8) | u8Data[0];
//        s16Buf[1] = ((int16_t)u8Data[3]<<8) | u8Data[2];
//        s16Buf[2] = ((int16_t)u8Data[5]<<8) | u8Data[4];
//    }
//    else
//    {
////        printf("\r\n Mag is bussy \r\n");
//    }
//#if 1
//    for(i = 0; i < 3; i ++)
//    {
//        invmsICM20948CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
//    }

//    *ps16X =  s32OutBuf[0];
//    *ps16Y = -s32OutBuf[1];
//    *ps16Z = -s32OutBuf[2];
//#else
//    *ps16X = s16Buf[0];
//    *ps16Y = -s16Buf[1];
//    *ps16Z = -s16Buf[2];
//#endif
//    return;
//}

static bool invmsICM20948MagCheck(void)
{
    bool bRet = false;
    uint8_t u8Ret[2];

    invmsICM20948ReadSecondary( I2C_ADD_ICM20948_AK09916|I2C_ADD_ICM20948_AK09916_READ,
                                REG_ADD_MAG_WIA1, 2,u8Ret);
    if( (u8Ret[0] == REG_VAL_MAG_WIA1) && ( u8Ret[1] == REG_VAL_MAG_WIA2) )
    {
        bRet = true;
    }

    return bRet;
}

static void invmsICM20948ReadSecondary(uint8_t u8I2CAddr, uint8_t u8RegAddr, uint8_t u8Len, uint8_t *pu8data)
{
    uint8_t i;
    uint8_t u8Temp;

    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL,  REG_VAL_REG_BANK_3); //swtich bank3
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV0_ADDR, u8I2CAddr);
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV0_REG,  u8RegAddr);
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV0_CTRL, REG_VAL_BIT_SLV0_EN|u8Len);

    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); //swtich bank0

    u8Temp = I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_USER_CTRL);
    u8Temp |= REG_VAL_BIT_I2C_MST_EN;
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_USER_CTRL, u8Temp);
    delay_ms(5);
    u8Temp &= ~REG_VAL_BIT_I2C_MST_EN;
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_USER_CTRL, u8Temp);

    for(i=0; i<u8Len; i++)
    {
        *(pu8data+i) = I2C_ReadOneByte(I2C_ADD_ICM20948, REG_ADD_EXT_SENS_DATA_00+i);

    }
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_3); //swtich bank3

    u8Temp = I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_I2C_SLV0_CTRL);
    u8Temp &= ~((REG_VAL_BIT_I2C_MST_EN)&(REG_VAL_BIT_MASK_LEN));
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV0_CTRL,  u8Temp);

    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); //swtich bank0

}

static void invmsICM20948WriteSecondary(uint8_t u8I2CAddr, uint8_t u8RegAddr, uint8_t u8data)
{
    uint8_t u8Temp;
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL,  REG_VAL_REG_BANK_3); //swtich bank3
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV1_ADDR, u8I2CAddr);
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV1_REG,  u8RegAddr);
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV1_DO,   u8data);
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV1_CTRL, REG_VAL_BIT_SLV0_EN|1);

    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); //swtich bank0

    u8Temp = I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_USER_CTRL);
    u8Temp |= REG_VAL_BIT_I2C_MST_EN;
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_USER_CTRL, u8Temp);
    delay_ms(5);
    u8Temp &= ~REG_VAL_BIT_I2C_MST_EN;
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_USER_CTRL, u8Temp);

    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_3); //swtich bank3

    u8Temp = I2C_ReadOneByte(I2C_ADD_ICM20948,REG_ADD_I2C_SLV0_CTRL);
    u8Temp &= ~((REG_VAL_BIT_I2C_MST_EN)&(REG_VAL_BIT_MASK_LEN));
    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_I2C_SLV0_CTRL,  u8Temp);

    I2C_WriteOneByte(I2C_ADD_ICM20948, REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); //swtich bank0

    return;
}

static void invmsICM20948CalAvgValue(uint8_t *pIndex, int16_t *pAvgBuffer, int16_t InVal, int32_t *pOutVal)
{
    uint8_t i;

    *(pAvgBuffer + ((*pIndex) ++)) = InVal;
    *pIndex &= 0x07;

    *pOutVal = 0;
    for(i = 0; i < 8; i ++)
    {
        *pOutVal += *(pAvgBuffer + i);
    }
    *pOutVal >>= 3;
}

static void invmsICM20948GyroOffset(void)
{
    uint8_t i;
    int16_t	s16Gx = 0, s16Gy = 0, s16Gz = 0;
    int32_t	s32TempGx = 0, s32TempGy = 0, s32TempGz = 0;
    for(i = 0; i < 32; i ++)
    {
        invmsICM20948GyroRead(&s16Gx, &s16Gy, &s16Gz);
        s32TempGx += s16Gx;
        s32TempGy += s16Gy;
        s32TempGz += s16Gz;
        delay_ms(10);
    }
    gstGyroOffset.s16X = s32TempGx >> 5;
    gstGyroOffset.s16Y = s32TempGy >> 5;
    gstGyroOffset.s16Z = s32TempGz >> 5;
    return;
}


//读取三轴加速度
static void invMSAccelRead(int16_t* ps16AccelX, int16_t* ps16AccelY, int16_t* ps16AccelZ)
{
    invmsICM20948AccelRead(ps16AccelX, ps16AccelY, ps16AccelZ);
    return;
}

//读取三轴角速度
static void invMSGyroRead(int16_t* ps16GyroX, int16_t* ps16GyroY, int16_t* ps16GyroZ)
{
    invmsICM20948GyroRead(ps16GyroX, ps16GyroY, ps16GyroZ);
    return;
}

//读取磁力计.若需要使用直接取消注释即可
//static void invMSMagRead(int16_t* ps16MagnX, int16_t* ps16MagnY, int16_t* ps16MagnZ)
//{
//    invmsICM20948MagRead(ps16MagnX, ps16MagnY, ps16MagnZ);
//    return;
//}
