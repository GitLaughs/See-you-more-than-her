#include "stmflash.h"
#include "delay.h"

//F407VE 共512kb FLash. 将数据保存在最后一个扇区7. 0x8000000+(512-128)*1024 = 0x806000
#define FLASH_SAVE_ADDR 0x08060000

//主存储器的解锁键值
#define STM32_FLASH_KEY1    ((uint32_t)0x45670123)
#define STM32_FLASH_KEY2    ((uint32_t)0xCDEF89AB)

//OPT用户选项字节的解锁键值
#define STM32_FLASH_OPTKEY1 ((uint32_t)0x08192A3B)
#define STM32_FLASH_OPTKEY2 ((uint32_t)0x4C5D6E7F)

//解锁主存储区Flash
void stmflash_unlock(void)
{
	//注：如果在LOCK位为0的情况下再次写入key,将触发硬件错误，单片机卡死
	if( (FLASH->CR & (uint32_t)1<<31) != 0)
	{
		FLASH->KEYR = STM32_FLASH_KEY1;
		FLASH->KEYR = STM32_FLASH_KEY2;
	}
	else
	{
		DEBUG_INFO("LOCK位当前是0，无需解锁\r\n");
	}
}

//解锁OPT用户选项字节区Flash
static void stmflash_opt_unlock(void)
{
	if( (FLASH->OPTCR & (uint32_t)1<<0)!=0 )
	{
		/* FLASH 写入解锁序列 */
		FLASH->OPTKEYR = STM32_FLASH_OPTKEY1;     
		FLASH->OPTKEYR = STM32_FLASH_OPTKEY2;
	}
	else
	{
		DEBUG_INFO("OPT LOCK位当前是0，无需解锁");
	}
}

//主存储区Flash上锁
static void stmflash_lock(void)
{
	/* FLASH 上锁 */
    FLASH->CR |= (uint32_t)1 << 31;     
}

//选项字节OPT区Flash上锁
static void stmflash_opt_lock(void)
{
	/* FLASH 上锁 */
    FLASH->OPTCR |= (uint32_t)1 << 0;     
}

//Flash基础设置，需要根据自己的时钟频率和硬件情况来对应设置
void stmflash_init(void)
{	
	DEBUG_INFO("1.写入key解锁\r\n");
	stmflash_unlock();
	
	delay_us(100);
	
	//设置Flash的等待周期(LATENCY)
	//与芯片供电电压、芯片CPU时钟频率配置密切相关
	//本芯片供电电压3.3V,CPU频率168M,需设置为5 WS
	DEBUG_INFO("2.ACR设置，将LATENCY设置为5ws\r\n");
	FLASH->ACR &= ~(7 << 0); //清除原来的配置
	FLASH->ACR |= 5 << 0;    //设置LATENCY为5 ws
	
	delay_us(100);
	//设置Flash操作单位PSIZE
	//与芯片供电电压相关，电压在2.7~3.6V时，需要设置并行位数为32
	
	DEBUG_INFO("3.CR设置，设置PSIZE宽度为32位\r\n");
	FLASH->CR &= ~(3 << 8); //清除原来的配置
	FLASH->CR |= 2<<8;      //设置PSIZE宽度为32位
	
	DEBUG_INFO("4.初始化完毕，上锁\r\n");
	stmflash_lock();
	
	/*
	 * 通过访问ACR寄存器默认值，F4芯片上的ART加速器 ①预取使能、
	 * ②指令缓存使能、 ③数据缓存使能 是默认打开的
	*/
}

//指定地址读取flash数据
uint32_t stmflash_read_word(uint32_t faddr)
{
	return  *(volatile uint32_t *)faddr;
}

//指定地址指定长度读取flash数据
void read_flash(uint32_t addr,uint32_t* p,u8 len)
{
	u8 i=0;
	for(i=0;i<len;i++)
	{
		p[i] = stmflash_read_word(addr);
		addr += 4;
	}
}

//获取Flash的读写状态
static FlashState stmflash_get_state(void)
{
    uint32_t res = 0;
	uint8_t error_count = 0;//错误个数统计
	
	//Flash状态，先标记无错误
	FlashState state = flash_no_error ;

	res = FLASH->SR; //读取SR寄存器获取状态
	
	//获取Flash的状态并标记
	if(Get_FlashState(res,FLASH_WRPERR)) 
	{
		error_count++,state = flash_wrperr;
		DEBUG_INFO("要写入的地址处于写保护状态\r\n");
	}
	if(Get_FlashState(res,FLASH_PGPERR)) 
	{
		error_count++,state = flash_pgperr;
		DEBUG_INFO("发生Flash编程并行位错误\r\n");
	}
	if(Get_FlashState(res,FLASH_PGSERR)) 
	{
		error_count++,state = flash_pgserr;
		DEBUG_INFO("发生Flash编程顺序错误\r\n");
	}
	if(Get_FlashState(res,FLASH_BSY)   )
	{
		error_count++,state = flash_busy;
		DEBUG_INFO("Flash总线等待中...\r\n");
	}
	
	/* EOP和OPERR在开启中断时才会生效，这里默认不使用 */
	//if(Get_FlashState(res,FLASH_EOP)   ) error_count++,state = flash_eop;
	//if(Get_FlashState(res,FLASH_OPERR) ) error_count++,state = flash_operr;
	//标记存在多个错误
	if(error_count>1)
	{
		state  = flash_more_error;
		DEBUG_INFO("Flash总线上存在多个错误...\r\n");
	}
    
    return state;
}

//等待Flash完成操作
static FlashState stmflash_wait_done(uint32_t time)
{
    FlashState state;
    do
    {
        state = stmflash_get_state();//获取Flash状态

        if (state != flash_busy)
        {
            break;      /* 非忙, 无需等待了, 直接退出 */
        }
        
        time--;
    } while (time);

    if (time == 0) 
	{
		DEBUG_INFO("Flash总线等待超时\r\n");
		state = flash_overtime;   /* 超时 */
	}
	
	//返回状态情况
    return state;
}

//擦除扇区操作
//只有主存储区才要擦除，OTP区无法擦除，选项字节区会自动擦除
FlashState stmflash_erase_sector(uint8_t saddr)
{
    FlashState res;

    res = stmflash_wait_done(0XFFFFFFFF);   /* 等待上次操作结束 */
	
    if (res == flash_no_error)
    {
		stmflash_unlock();
        FLASH->CR &= ~(0X1F << 3);           /* 清除原来的扇区设置 */
        FLASH->CR |= saddr << 3;             /* 设置要擦除的扇区 */
		
		FLASH->CR &= ~(7 << 0);              /* 清除原来选择编程、扇区擦除、片擦除的配置 */
        FLASH->CR |= 1 << 1;                 /* 配置为扇区擦除 */
		
        FLASH->CR |= 1 << 16;                /* 开始擦除 */
        res = stmflash_wait_done(0XFFFFFFFF);/* 等待操作结束 */
		stmflash_lock();
		//注：STRT位会自动清0(位16)， SER位不会(位1),所以 SER位操作前清空之前的配置即可             
    }
    return res;
}


//在FLASH指定位置写入一个字
//入口参数：地址，要写入的数据
//地址必须是4的倍数，因为PSIZE要根据电压来配置，硬件电压是3.3V，所以PSIZE配置为32位 \
  Flash是128位宽的，地址不是4的倍数时，将会发生编程对齐错误
static FlashState stmflash_write_word(uint32_t faddr, uint32_t data)
{
    FlashState res;

    res = stmflash_wait_done(0XFFFFF);

    if (res == flash_no_error)
    {
		FLASH->CR &= ~(7 << 0);             /* 清除原来选择编程、扇区擦除、片擦除的配置 */
        FLASH->CR |= 1 << 0;                /* 编程使能 */
        *(volatile uint32_t *)faddr = data; /* 写入数据 */
        res = stmflash_wait_done(0XFFFFF);  /* 等待操作完成,一个字编程 */
		//注意：编程时与STRT位无关，这个位是擦除时用的
    }

    return res;
}


/**
获取某个地址所在的flash扇区
faddr   : flash地址
0~11, 即addr所在的扇区
*/
static uint8_t stmflash_get_flash_sector(uint32_t addr)
{
    if (addr < ADDR_FLASH_SECTOR_1)return 0;
    else if (addr < ADDR_FLASH_SECTOR_2)return 1;
    else if (addr < ADDR_FLASH_SECTOR_3)return 2;
    else if (addr < ADDR_FLASH_SECTOR_4)return 3;
    else if (addr < ADDR_FLASH_SECTOR_5)return 4;
    else if (addr < ADDR_FLASH_SECTOR_6)return 5;
    else if (addr < ADDR_FLASH_SECTOR_7)return 6;
    else if (addr < ADDR_FLASH_SECTOR_8)return 7;
    else if (addr < ADDR_FLASH_SECTOR_9)return 8;
    else if (addr < ADDR_FLASH_SECTOR_10)return 9;
    else if (addr < ADDR_FLASH_SECTOR_11)return 10;

    return 11;
}


//指定位置，写入指定长度的32位数据
//入口参数：地址、要写入的数据、要写入的数据总量
//允许写入的区域：主存储区、OTP区（一次性编程区域）
FlashState stmflash_write(uint32_t waddr, uint32_t *pbuf, uint32_t length)
{

    FlashState status = flash_no_error;
    uint32_t endaddr = 0;
	
	/* 非法地址：
	 * ①大于本芯片最大Flash容量地址
	 * ②小于存储区首地址
	 * ③地址不是4的倍数 
	 * ④注意，这些地址保护比较严格，不支持OTP的地址，如果要写入OTP区域，可以注释地址保护
	*/
    if (waddr < STM32_FLASH_BASE ||  waddr > FLASH_END_ADDR ||  waddr % 4 ) 
    {
		status = flash_AddrErr;//非法地址
		DEBUG_INFO("写入的地址非法\r\n");
        return status;
    }
	
    endaddr = waddr + length*4;
	if(endaddr>FLASH_END_ADDR) //写入的内容会超出内存总容量，不合法
	{
		DEBUG_INFO("写入的内容未来会超出flash总容量\r\n");
		status = flash_AddrWillout;
		return status;
	}
    
    
	stmflash_unlock();       //Flash解锁
    FLASH->ACR &= ~(1 << 9); //禁止指令缓存
	FLASH->ACR &= ~(1 << 10);//禁止数据缓存
	FLASH->ACR |= 1 << 11;   //复位(清空)指令缓存
	FLASH->ACR |= 1 << 12;   //复位(清空)数据缓存
	__disable_irq();         //关闭所有中断，保证Flash写入无误
	
	//等待上一次操作完成
	status = stmflash_wait_done(0xffff);
	
	//Flash状态无错误，开始写入数据
	if(status == flash_no_error)
	{
		DEBUG_INFO("开始从 0x%x 处写入数据\r\n",waddr);
		while(waddr < endaddr)
		{
			status = stmflash_write_word(waddr , *pbuf);
			if(status!=flash_no_error) break;
			
			waddr += 4;
			pbuf ++ ;
		}	
		
	}

	FLASH->ACR |= 1<<9;    //使能指令缓存
	FLASH->ACR |= 1<<10;   //使能数据缓存
	FLASH->ACR &= ~(1<<11);//清除指令缓存复位标志位
	FLASH->ACR &= ~(1<<12);//清除数据缓存复位标志位
	stmflash_lock();       //Flash上锁
	__enable_irq();        //Flash写入完毕，恢复中断
	
	return status;
}


//对指定的扇区 设置/取消 写保护
//入口参数：扇区、设置或取消（1：设置  0：取消）
FlashState set_sector_WriteProtect(uint8_t sector,uint8_t enable)
{
	FlashState state = flash_no_error;
	uint8_t set_sector;
	
	
	if(sector > stmflash_get_flash_sector(FLASH_END_ADDR))
	{
		state = flash_sector_out;
		return state;
	}
	state = stmflash_wait_done(0xFFFF);//等待上一次操作结束
	
	//Flash总线上没有问题
	if(state==flash_no_error)
	{
		stmflash_opt_unlock(); //解锁选项字节的寄存器保护
		
		if(sector<=11) //F40x系列芯片，最多只有11个扇区
		{
			set_sector = sector + 16 ; //16位寄存器设置位置的偏移量
			
			//设置写保护
			if(enable) FLASH->OPTCR &= ~(1<<set_sector);
			
			//取消写保护
			else FLASH->OPTCR |= 1<<set_sector;
				
		}
		else // F42x、F43x,扇区最多可到23
		{
			set_sector = sector-12 + 16 ; //16位寄存器设置位置的偏移量,-12是相对前面11个扇区的偏移
			
			//设置写保护
			if(enable) FLASH->OPTCR1 &= ~(1<<set_sector);
			
			//取消写保护
			else FLASH->OPTCR1 |= 1<<set_sector;
		}
		
		//开始写入选项字节
		FLASH->OPTCR |= 1<<1;
		
		//等待操作结束
		state = stmflash_wait_done(0xFFFF);
		
		//清除写入选项字节标志位
		//注：这一步其实不需要，无论对Flash操作有没有成功，这个位会自动置0
		//FLASH->OPTCR &= ~(1<<1);
		
		//对选项字节区上锁
		stmflash_opt_lock();
	}
	
	return state;
	
}


u8 get_sector_WriteProtect(u8 sector)
{
	
	u16 temp;
	
	if(sector<=11)
	{
		temp = stmflash_read_word(0x1fffc008);
		
	}
	else
	{
		temp = stmflash_read_word(0x1ffec008);
		
		sector = sector-12; //减去偏差
	}
	
	return !(temp>>sector&0x01);

}

//对外函数,向Flash写入数据.
//入口参数：32位数据的地址,写入的数据长度.
uint8_t Write_Flash(uint32_t* data,uint16_t datalen)
{
	FlashState res;
	
	static u8 init=0;
	if(init==0) 
	{
		init =1;
		stmflash_init();//初始化FLash
	}
	
	//写入数据前需要先擦除
	res = stmflash_erase_sector( stmflash_get_flash_sector(FLASH_SAVE_ADDR) );//对将要写入的FLash片区擦除
	
	if( res==flash_no_error )
	{
		res = stmflash_write(FLASH_SAVE_ADDR,data,datalen);//写入数值
	}

	return res;
}

//对外函数,读取Flash内容.
//入口参数：读取数据的索引号
int Read_Flash(uint16_t index)
{
	int tmp;
	tmp = stmflash_read_word( FLASH_SAVE_ADDR+(4*index) );
	return tmp;
}

