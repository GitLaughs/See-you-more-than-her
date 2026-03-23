#ifndef __PSTWO_H
#define __PSTWO_H
#include "sys.h"

/*--------PS2 config--------*/
//PS2手柄引脚
#define ENABLE_PS2_DI_PIN_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE)
#define ENABLE_PS2_DO_PIN_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE)
#define ENABLE_PS2_CS_PIN_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE)
#define ENABLE_PS2_CLK_PIN_CLOCK RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE)

#define PS2_DI_PORT  GPIOE
#define PS2_DO_PORT  GPIOE
#define PS2_CS_PORT  GPIOE
#define PS2_CLK_PORT GPIOE

#define PS2_DI_PIN  GPIO_Pin_0
#define PS2_DO_PIN  GPIO_Pin_1
#define PS2_CS_PIN  GPIO_Pin_2
#define PS2_CLK_PIN GPIO_Pin_3

#define DI   PEin(0)     //Input pin //输入引脚

#define DO_H PEout(1)=1   //Command height //命令位高
#define DO_L PEout(1)=0   //Command low //命令位低

#define CS_H PEout(2)=1  //Cs pull up //CS拉高
#define CS_L PEout(2)=0  //Cs drawdown //CS拉低

#define CLK_H PEout(3)=1 //Clock lift //时钟拉高
#define CLK_L PEout(3)=0 //Clock down //时钟拉低
/*----------------------------------*/

//对外接口
void PS2_Init(void);
void PS2_Read(void);
void PS2_Key_Param_Init(void);

#endif





