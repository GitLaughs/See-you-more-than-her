#ifndef __OLED_H
#define __OLED_H			  	 
#include "sys.h"

#define OLED_CMD  0	//Command //Ð´ÃüÁî
#define OLED_DATA 1	//Data //Ð´Êý¾Ý

#define CNSizeWidth  16
#define CNSizeHeight 16

/*--------OLED config--------*/
#define ENABLE_OLED_PIN_CLOCK  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE)

#define OLED_SCL_PORT     GPIOD //port
#define OLED_SDA_PORT     GPIOD
#define OLED_RES_PORT     GPIOD
#define OLED_DC_PORT      GPIOD

#define OLED_SCL_PIN      GPIO_Pin_7 //pin
#define OLED_SDA_PIN      GPIO_Pin_6
#define OLED_RES_PIN      GPIO_Pin_5
#define OLED_DC_PIN       GPIO_Pin_4

#define OLED_SCLK_Clr()  PDout(7)=0   //SCL
#define OLED_SCLK_Set()  PDout(7)=1   //SCL

#define OLED_SDIN_Clr()  PDout(6)=0   //SDA
#define OLED_SDIN_Set()  PDout(6)=1   //SDA

#define OLED_RST_Clr()   PDout(5)=0   //RES
#define OLED_RST_Set()   PDout(5)=1   //RES

#define OLED_RS_Clr()    PDout(4)=0   //DC
#define OLED_RS_Set()    PDout(4)=1   //DC
/*----------------------------------*/



/*--------OLED Interface Fun--------*/
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh_Gram(void);		
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 size,u8 mode);
void OLED_ShowNumber(u8 x,u8 y,u32 num,u8 len,u8 size);
void OLED_ShowString(u8 x,u8 y,const u8 *p);
void oled_showfloat(const float needtoshow,u8 show_x,u8 show_y,u8 zs_num,u8 xs_num);
void OLED_ShowCHinese(u8 x,u8 y,u8 no,u8 font_width,u8 font_height);	\
void OLED_DrawBMP(unsigned char x0, unsigned char y0,unsigned char x1, unsigned char y1,const unsigned char BMP[]);
extern const unsigned char gImage_usb_bmp[];
void OLED_Refresh_Line(void);
void OLED_ClearBuf(void);
/*----------------------------------*/


#endif  
	 
