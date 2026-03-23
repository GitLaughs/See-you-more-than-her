#ifndef __LED_TASK_H
#define __LED_TASK_H

#include "system.h"

#define LED_TASK_PRIO		2     //Task priority //任务优先级
#define LED_STK_SIZE 		128   //Task stack size //任务堆栈大小

void led_task(void *pvParameters);

#endif

