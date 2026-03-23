#ifndef __KEY_H
#define __KEY_H	 
#include "sys.h"


/*--------KEY config--------*/
#define ENABLE_USER_KEY_PIN_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE)
#define USER_KEY_PORT              GPIOC
#define USER_KEY_PIN               GPIO_Pin_5
#define KEY                        PCin(5)
/*----------------------------------*/


/*--------KEY Interface Fun --------*/
enum { //°´¼ü×´Ì¬Ã¶¾Ù
	key_stateless,
	single_click,
	double_click,
	long_click
};
void KEY_Init(void);
u8 KEY_Scan(u16 Frequency,u16 filter_times);
/*---------------------------------*/



#endif 
