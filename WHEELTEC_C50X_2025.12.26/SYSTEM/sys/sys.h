#ifndef __SYS_H
#define __SYS_H	 
#include "stm32f4xx.h" 

//TODO:用于debug调试变量
typedef struct{
	float   f_val1;
	float   f_val2;
	int     int_val;
	short   s_val;
	uint8_t  u8_val;
	uint16_t u16_val;
	
	//TODO:阿克曼调试新增
//	float akm_left_angle;
//	float akm_right_angle;
//	float akm_turnR;
//	
//	float odom_x;
//	float odom_y;
//	float odom_z;
//	
//	uint16_t set_turnangle;
	//1.一定要注意除法相关语句,分母变量一定要初始化,不能为0
}DEBUG_t;


//硬件版本,以V1.0开始
typedef enum{
	HW_NONE = 0,
	V1_0 = 0x0001,  //Ver1.0：C50C,陀螺仪MPU6050,PS2手柄为普通插座版本
	V1_1 = 0x0002,  //Ver1.1：C50C,陀螺仪ICM20948,PS2手柄升级为USB手柄
}HARDWARE_VERSION;

//软件版本,以V1.00开始.
typedef enum{
	SW_NONE = 0,
	V1_00 = 0x0001, //Ver1.00：软件版本更新请查看 Updatalog/更新记录.txt
}SOFTWARE_VERSION;

//机器人系统级相关变量
typedef struct{
	HARDWARE_VERSION HardWare_Ver;  //硬件版本
	SOFTWARE_VERSION SoftWare_Ver;  //软件版本
	int Time_count;      // 系统上电计时
	u8 HardWare_charger; // 系统是否存在充电装备硬件 1:存在 0:不存在
	u8 SecurityLevel;    // 系统安全等级 默认为1    0:最高等级,对机器人控制命令持续监测,若丢失则自动停止. 1:不对控制命令监测,命令丢失保留最后一个速度指令运动.  2~255：...待定
	u16 LED_delay;       // 系统状态指示灯闪烁时间 单位 ms
}SYS_VAL_t;

extern SYS_VAL_t SysVal;
void SYS_VAL_t_Init(SYS_VAL_t* p);
const uint8_t* getSW_Ver(SOFTWARE_VERSION ver);
const uint8_t* getHW_Ver(HARDWARE_VERSION ver);

//extern DEBUG_t debug;

//math abs接口
//abs() :  用于 int类型
//fabs():  用于 float类型
//labs()： 用于 long int 类型的数据。
//llabs()：用于 long long int 类型的数据。
//fabsf()：用于 float 类型的数据。
//fabsl()：用于 long double 类型的数据。

//0,不支持os
//1,支持os
#define SYSTEM_SUPPORT_OS		1		//定义系统文件夹是否支持OS
																	    
	 
//位带操作,实现51类似的GPIO控制功能
//具体实现思想,参考<<CM3权威指南>>第五章(87页~92页).M4同M3类似,只是寄存器地址变了.
//IO口操作宏定义
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 
//IO口地址映射
#define GPIOA_ODR_Addr    (GPIOA_BASE+20) //0x40020014
#define GPIOB_ODR_Addr    (GPIOB_BASE+20) //0x40020414 
#define GPIOC_ODR_Addr    (GPIOC_BASE+20) //0x40020814 
#define GPIOD_ODR_Addr    (GPIOD_BASE+20) //0x40020C14 
#define GPIOE_ODR_Addr    (GPIOE_BASE+20) //0x40021014 
#define GPIOF_ODR_Addr    (GPIOF_BASE+20) //0x40021414    
#define GPIOG_ODR_Addr    (GPIOG_BASE+20) //0x40021814   
#define GPIOH_ODR_Addr    (GPIOH_BASE+20) //0x40021C14    
#define GPIOI_ODR_Addr    (GPIOI_BASE+20) //0x40022014     

#define GPIOA_IDR_Addr    (GPIOA_BASE+16) //0x40020010 
#define GPIOB_IDR_Addr    (GPIOB_BASE+16) //0x40020410 
#define GPIOC_IDR_Addr    (GPIOC_BASE+16) //0x40020810 
#define GPIOD_IDR_Addr    (GPIOD_BASE+16) //0x40020C10 
#define GPIOE_IDR_Addr    (GPIOE_BASE+16) //0x40021010 
#define GPIOF_IDR_Addr    (GPIOF_BASE+16) //0x40021410 
#define GPIOG_IDR_Addr    (GPIOG_BASE+16) //0x40021810 
#define GPIOH_IDR_Addr    (GPIOH_BASE+16) //0x40021C10 
#define GPIOI_IDR_Addr    (GPIOI_BASE+16) //0x40022010 
 
//IO口操作,只对单一的IO口!
//确保n的值小于16!
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //输出 
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //输入 

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //输出 
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //输入 

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //输出 
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //输入 

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //输出 
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //输入 

#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  //输出 
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  //输入

#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  //输出 
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  //输入

#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  //输出 
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  //输入

#define PHout(n)   BIT_ADDR(GPIOH_ODR_Addr,n)  //输出 
#define PHin(n)    BIT_ADDR(GPIOH_IDR_Addr,n)  //输入

#define PIout(n)   BIT_ADDR(GPIOI_ODR_Addr,n)  //输出 
#define PIin(n)    BIT_ADDR(GPIOI_IDR_Addr,n)  //输入

//以下为汇编函数
void WFI_SET(void);		//执行WFI指令
void INTX_DISABLE(void);//关闭所有中断
void INTX_ENABLE(void);	//开启所有中断
void MSR_MSP(u32 addr);	//设置堆栈地址 
#endif











