#ifndef __STMFLASH_H
#define __STMFLASH_H

#include "sys.h"

//#define use_usart_debug 

#ifdef use_usart_debug

/*
__FILE__       //当前程序源文件，类型char*
__FUNCCTIONN__ //当前运行的函数，类型char*
__LINE__       //当前的函数行,类型int
__VA_ARGS__    //代表宏里面的可变参数，跟...配合使用

#  ： 将#后面的内容转换成字符串
## ： 连接操作符，test##i  根据传入的i不同，实现字符串test与字符串i的连接
      另外，在可变参数的宏里面，##还有消去前面一个逗号的作用
args... :代表可变参数列表

*/

#define DEBUG_INFO(fmt,...) \
	any_printf(USART1,"[File:%s][Function:%s][Line:%d]" fmt,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define DEBUG_INFO(fmt,...) __nop()
#endif


#define OTP_ADDR_TEST 0x1FFF7800 //OTP区域块 0 的首地址
#define OTP_LOCK_ADDR 0x1FFF7A00 //OTP区域锁定块配置的首地址

//Flash编程报错枚举类型
enum{
	FLASH_EOP    = ( 1 << 0 ) , //Flash 编程/擦除 操作完成（该位只有在配置操作结束中断使能时(EOPIE=1)才会生效）
	FLASH_OPERR  = ( 1 << 1 ) , //Flash操作错误：存在Flash操作请求，但存在并行位数错误、对齐错误或写保 （该位只有在使能操作错误中断时(ERRIE=1)才会生效）
	FLASH_WRPERR = ( 1 << 4 ) , //要擦除/编程的地址属于 Flash 中处于写保护状态的区域
	FLASH_PGAERR = ( 1 << 5 ) , //编程对齐错误，Flash是128位宽的，写入的数据跨越了这个宽度
	FLASH_PGPERR = ( 1 << 6 ) , //编程并行位错误，PSIZE的设置 与 写入Flash的数据宽度不匹配
	FLASH_PGSERR = ( 1 << 7 ) , //编程顺序错误 ， 控制寄存器时步骤顺序不对
	FLASH_BSY    = ( 1 << 16) , //当前存在Flash操作正在进行
};
#define Get_FlashState(res,mask) ( res & mask) //获取Flash枚举报错类型

//Flash报错枚举
typedef enum
{
	flash_no_error = 0 , //Flash无错误
	flash_busy ,     //Flash操作忙
	flash_more_error,//Flash操作存在多个错误
	flash_eop,       //Flash操作完成
	flash_operr,     //Flash操作错误
	flash_wrperr,    //Flash写保护错误
	flash_pgperr,    //Flash并行错误
	flash_pgserr,    //Flash对齐错误
	flash_overtime,   //Flash等待超时
	flash_AddrErr,   //需要操作的Flash地址非法
	flash_AddrWillout,//Flash操作到末尾会超出容量范围
	flash_sector_out, //要设置的扇区超出芯片内拥有的扇区
}FlashState;


//Flash基地址
#define STM32_FLASH_BASE    0x08000000

//所使用的芯片Flash容量，单位 kB
#define STM32_FLASH_SIZE 512

//Flash末尾地址
#define FLASH_END_ADDR  STM32_FLASH_BASE + STM32_FLASH_SIZE*1024 - 1

//RAM基地址
#define STM32_RAM_BASE 0x20000000
#define STM32_RAM_SIZE 192
#define RAM_END_ADDR  STM32_RAM_BASE + STM32_RAM_SIZE*1024 - 1

/* FLASH 扇区的起始地址 */
#define ADDR_FLASH_SECTOR_0     ((uint32_t )0x08000000)     /* 扇区0起始地址, 16 Kbytes */
#define ADDR_FLASH_SECTOR_1     ((uint32_t )0x08004000)     /* 扇区1起始地址, 16 Kbytes */
#define ADDR_FLASH_SECTOR_2     ((uint32_t )0x08008000)     /* 扇区2起始地址, 16 Kbytes */
#define ADDR_FLASH_SECTOR_3     ((uint32_t )0x0800C000)     /* 扇区3起始地址, 16 Kbytes */
#define ADDR_FLASH_SECTOR_4     ((uint32_t )0x08010000)     /* 扇区4起始地址, 64 Kbytes */
#define ADDR_FLASH_SECTOR_5     ((uint32_t )0x08020000)     /* 扇区5起始地址, 128 Kbytes */
#define ADDR_FLASH_SECTOR_6     ((uint32_t )0x08040000)     /* 扇区6起始地址, 128 Kbytes */
#define ADDR_FLASH_SECTOR_7     ((uint32_t )0x08060000)     /* 扇区7起始地址, 128 Kbytes */
#define ADDR_FLASH_SECTOR_8     ((uint32_t )0x08080000)     /* 扇区8起始地址, 128 Kbytes */
#define ADDR_FLASH_SECTOR_9     ((uint32_t )0x080A0000)     /* 扇区9起始地址, 128 Kbytes */
#define ADDR_FLASH_SECTOR_10    ((uint32_t )0x080C0000)     /* 扇区10起始地址,128 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t )0x080E0000)     /* 扇区11起始地址,128 Kbytes */


//对外接口
uint8_t Write_Flash(uint32_t* data,uint16_t datalen);
int Read_Flash(uint16_t index);

#endif

