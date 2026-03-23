/*
 * 为标准库+FreeRTOS环境下的USB Host功能提供支持文件
*/

#include "usbh_os_middleware.h"
#include "stm32f4xx_hal_hcd.h"

void osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout)
{
	QueueHandle_t hQueue = (QueueHandle_t)mq_id;

	BaseType_t yield;

	(void)msg_prio; /* Message priority is ignored */
	
	//ISR中断环境
	if (IS_IRQ())
	{
		if ((hQueue == NULL) || (msg_ptr == NULL) || (timeout != 0U))
		{
		  //参数合法检查
		}
		else
		{
			yield = pdFALSE;

			if (xQueueSendToBackFromISR (hQueue, msg_ptr, &yield) != pdTRUE)
			{
				//写入失败
			}
			else
			{
				portYIELD_FROM_ISR (yield);//写入成功,并根据情况主动发起任务调度
			}
		}
	}
	//普通任务环境
	else 
	{
		if ((hQueue == NULL) || (msg_ptr == NULL)) 
		{
			//检查参数是否合法
		}
		else 
		{
			xQueueSendToBack (hQueue, msg_ptr, (TickType_t)timeout);
		}
	}
}

int8_t osMessageQueueGet (osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
  QueueHandle_t hQueue = (QueueHandle_t)mq_id;
  int8_t stat = 0;
  BaseType_t yield;

  (void)msg_prio; /* Message priority is ignored */

  if (IS_IRQ()) {
    if ((hQueue == NULL) || (msg_ptr == NULL) || (timeout != 0U)) {
      stat = -4;
    }
    else {
      yield = pdFALSE;

      if (xQueueReceiveFromISR (hQueue, msg_ptr, &yield) != pdPASS) {
        stat = -3;
      } else {
        portYIELD_FROM_ISR (yield);
      }
    }
  }
  else {
    if ((hQueue == NULL) || (msg_ptr == NULL)) {
      stat = -4;
    }
    else {
      if (xQueueReceive (hQueue, msg_ptr, (TickType_t)timeout) != pdPASS) {
        if (timeout != 0U) {
          stat = -2;
        } else {
          stat = -3;
        }
      }
    }
  }

  return (stat);
}

/**
  * @brief This function handles USB On The Go FS global interrupt.
  */
extern HCD_HandleTypeDef hhcd_USB_OTG_FS;
void OTG_FS_IRQHandler(void)
{
  /* USER CODE BEGIN OTG_FS_IRQn 0 */

  /* USER CODE END OTG_FS_IRQn 0 */
  HAL_HCD_IRQHandler(&hhcd_USB_OTG_FS);
  /* USER CODE BEGIN OTG_FS_IRQn 1 */

  /* USER CODE END OTG_FS_IRQn 1 */
}

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}


