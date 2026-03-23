#ifndef __SHOW_TASK_H
#define __SHOW_TASK_H
#include "system.h"

#define SHOW_TASK_PRIO		2
#define SHOW_STK_SIZE 		512  
#define SHOW_TASK_RATE      RATE_10_HZ
extern TaskHandle_t show_TaskHandle;

#define OLED_MAX_PAGE 3 //OLED的页数

typedef struct{
	u8 page;       //当前页（页数从1开始）
	u8 last_page;  //上一页
	u8 refrsh;     //刷新标志位
	u8 MAX_PAGE;   //OLED最大页数
}OLED_t;

extern OLED_t oled;

//对外接口
void show_task(void *pvParameters);
void OLED_Param_Init(OLED_t* p);

//内部使用函数
static float VolMean_Filter(float data);
static void APP_ShowTask(void);
static void OLED_ShowTask(void);

static void oled_akm_show(void);
static void oled_diff_show(void);
static void oled_mec_4wd_omni_show(void);


#endif

