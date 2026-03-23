#ifndef __ENABLE_KEY_H
#define __ENABLE_KEY_H
#include "sys.h"

/*--------EnableKey config--------*/
#define ENABLE_EnableKEY_PIN_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE)
#define ENABLE_KEY_PORT              GPIOE
#define ENABLE_KEY_PIN               GPIO_Pin_4
#define EN     PEin(4)  
/*----------------------------------*/


/*--------EnableKey Interface Fun --------*/
void EnableKey_Init(void);
/*----------------------------------*/


#endif
