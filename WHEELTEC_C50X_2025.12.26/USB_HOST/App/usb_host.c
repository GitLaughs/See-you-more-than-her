/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file            : usb_host.c
  * @version         : v1.0_Cube
  * @brief           : This file implements the USB Host
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/

#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"

/* USER CODE BEGIN Includes */
#include "bsp_gamepad.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Host core handle declaration */
USBH_HandleTypeDef hUsbHostFS;
ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*
 * user callback declaration
 */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * Init USB host library, add supported class and start the library
  * @retval None
  */
void MX_USB_HOST_Init(void)
{
	//手柄默认接口
	GamePadInterface = &GamePadDefalut;
	
	extern USBH_ClassTypeDef  GamePad_HID_Class;
	extern USBH_ClassTypeDef  GamePad_NonStdHID_Class;
	if (USBH_Init(&hUsbHostFS, USBH_UserProcess, HOST_FS) != USBH_OK)
	{
		Error_Handler();
	}
	if (USBH_RegisterClass(&hUsbHostFS, &GamePad_HID_Class) != USBH_OK) //注册ps2 hid类
	{
		Error_Handler();
	}
	if (USBH_RegisterClass(&hUsbHostFS, &GamePad_NonStdHID_Class) != USBH_OK) //注册无线手柄ps2 hid类
	{
		Error_Handler();
	}

	if (USBH_Start(&hUsbHostFS) != USBH_OK)
	{
		Error_Handler();
	}
}

/*
 * user callback definition
 */
static void USBH_UserProcess  (USBH_HandleTypeDef *phost, uint8_t id)
{
  /* USER CODE BEGIN CALL_BACK_1 */
  switch(id)
  {
  case HOST_USER_SELECT_CONFIGURATION:
  break;

  case HOST_USER_DISCONNECTION:
  Appli_state = APPLICATION_DISCONNECT;
  break;

  case HOST_USER_CLASS_ACTIVE:
  Appli_state = APPLICATION_READY;
  break;

  case HOST_USER_CONNECTION:
  Appli_state = APPLICATION_START;
  break;

  default:
  break;
  }
  /* USER CODE END CALL_BACK_1 */
}

#include "system.h"

extern USBH_StatusTypeDef USBH_HID_PS2_Decode(USBH_HandleTypeDef *phost);
extern uint8_t g_usbhost_connect; 


//usb数据读取后,最终进入此回调函数.数据解码在此函数进行(任务环境)
void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
	//游戏手柄数据解码
	USBH_HID_PS2_Decode(phost);

	g_usbhost_connect=0;
	
	//游戏手柄模式启动
	if( GamePadInterface->StartFlag == 1 && Get_Control_Mode(_PS2_Control)==0 && SysVal.Time_count>=CONTROL_DELAY && GamePadInterface->LY > 150 )
		Set_Control_Mode(_PS2_Control);
	
}

/**
  * @}
  */

/**
  * @}
  */

