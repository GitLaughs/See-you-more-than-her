#ifndef __BUZZER_H
#define __BUZZER_H
#include "sys.h"

enum{
	BEEP_OFF = 0,
	BEEP_ON  = 1
};

/*--------Buzzer config--------*/
#define ENABLE_Buzzer_PIN_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE)
#define Buzzer_PORT              GPIOD
#define Buzzer_PIN               GPIO_Pin_11
#define Buzzer                   PDout(11)
/*----------------------------------*/


/*--------Buzzer Interface Fun --------*/
void Buzzer_Init(void); 

uint8_t Buzzer_AddTask(uint8_t num_time,uint8_t times);
void Buzzer_task(uint8_t rate);
/*----------------------------------*/


#endif
