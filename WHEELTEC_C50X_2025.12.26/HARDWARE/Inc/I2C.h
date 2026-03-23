#ifndef _I2C_H_
#define _I2C_H_
#include "sys.h"
#include "delay.h"

enum
{
	I2C_ACK,
	I2C_NACK
};

/*--------simulate iic config--------*/
//模拟IIC 引脚配置
#define ENABLE_IIC_SCL_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE)
#define ENABLE_IIC_SDA_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE)

#define IIC_SCL_PORT      GPIOB
#define IIC_SCL_PIN       GPIO_Pin_9
#define IIC_SCL           PBout(9) //SCL

#define IIC_SDA_PORT      GPIOB
#define IIC_SDA_PIN       GPIO_Pin_8
#define IIC_SDA_PIN_NUM   8          //SDA的引脚号
#define IIC_SDA           PBout(8)   //输出SDA	 
#define READ_SDA          PBin(8)    //输入SDA 
/*----------------------------------*/

//根据用户配置自动适配，无需修改
#define SDA_IN()  {IIC_SDA_PORT->MODER&=~(3<<(IIC_SDA_PIN_NUM*2));IIC_SDA_PORT->MODER|=0<<IIC_SDA_PIN_NUM*2;}	
#define SDA_OUT() {IIC_SDA_PORT->MODER&=~(3<<(IIC_SDA_PIN_NUM*2));IIC_SDA_PORT->MODER|=1<<IIC_SDA_PIN_NUM*2;} 

void I2C_SDAMode(uint8_t Mode);
void I2C_Start(void);
void I2C_Stop(void);
uint8_t I2C_WaiteForAck(void);
void I2C_Ack(void);
void I2C_NAck(void);
uint8_t I2C_WriteOneBit(uint8_t DevAddr, uint8_t RegAddr, uint8_t BitNum, uint8_t Data);
uint8_t I2C_WriteBits(uint8_t DevAddr, uint8_t RegAddr, uint8_t BitStart, uint8_t Length, uint8_t Data);
void I2C_WriteByte(uint8_t Data);
uint8_t I2C_ReadByte(uint8_t Ack);
u8 I2C_WriteOneByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t Data);
uint8_t I2C_ReadOneByte(uint8_t DevAddr, uint8_t RegAddr);
uint8_t I2C_WriteBuff(uint8_t DevAddr, uint8_t RegAddr, uint8_t Num, uint8_t *pBuff);
uint8_t I2C_ReadBuff(uint8_t DevAddr, uint8_t RegAddr, uint8_t Num, uint8_t *pBuff);
void I2C_GPIOInit(void);

#endif

