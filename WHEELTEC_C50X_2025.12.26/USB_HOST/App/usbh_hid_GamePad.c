#include "usbh_hid_GamePad.h"
#include "usbh_hid.h"

#include "FreeRTOS.h"
#include "timers.h"

//手柄支持包
#include "bsp_gamepad.h"

//xbox360手柄
#include "xbox360_gamepad.h"

//有线 USB PS2手柄
#include "WiredPS2_gamepad.h"

//游戏手柄的特征参数,当PID、VID相同时,依靠此类参数区分不同品牌
char GamePad_Manufacturer[100] = { 0 };
char GamePad_iSerialNum[100] = { 0 } ;

//GamePad_HID类相关函数声明
static USBH_StatusTypeDef USBH_HID_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_HID_SOFProcess(USBH_HandleTypeDef *phost);
static void USBH_HID_ParseHIDDesc(HID_DescTypeDef *desc, uint8_t *buf);

//usb手柄的初始化声明
extern USBH_StatusTypeDef USBH_HID_GamePadInit(USBH_HandleTypeDef *phost);

//特殊需要初始化的手柄类型
void SwitchPro_GamePad_Init(USBH_HandleTypeDef *phost);

//定义ps2的hid类
USBH_ClassTypeDef  GamePad_HID_Class =
{
  .Name = "HID",
  .ClassCode = USB_HID_CLASS, //默认为HID设备
  .Init = USBH_HID_InterfaceInit,
  .DeInit = USBH_HID_InterfaceDeInit,
  .Requests = USBH_HID_ClassRequest,
  .BgndProcess = USBH_HID_Process,
  .SOFProcess = USBH_HID_SOFProcess,
  .pData = NULL,
};

//非标准HID
USBH_ClassTypeDef  GamePad_NonStdHID_Class =
{
  .Name = "HID",
  .ClassCode = 0xff, //有些手柄的类并不是标准的HID设备,而是0xff
  .Init = USBH_HID_InterfaceInit,
  .DeInit = USBH_HID_InterfaceDeInit,
  .Requests = USBH_HID_ClassRequest,
  .BgndProcess = USBH_HID_Process,
  .SOFProcess = USBH_HID_SOFProcess,
  .pData = NULL,
};


/**
  * @brief  USBH_HID_InterfaceInit
  *         The function init the HID class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_InterfaceInit(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status;
  HID_HandleTypeDef *HID_Handle;
  uint16_t ep_mps;
  uint8_t max_ep;
  uint8_t num = 0U;
  uint8_t interface;

	USBH_UsrLog("start find interface now");
	
	//接口匹配,主要对下面的这个数组接口进行匹配
	//phost->device.CfgDesc.Itf_Desc[0,1,2,3...,n...].bInterfaceSubClass

	//0xff表示匹配所有类型
	interface = USBH_FindInterface(phost, 0xFFU, 0xFFU, 0xFFU);
	
	if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES)) /* No Valid Interface */
	{
		USBH_DbgLog("Cannot Find the interface for %s class.", phost->pActiveClass->Name);
		return USBH_FAIL;
	}

  status = USBH_SelectInterface(phost, interface);

  if (status != USBH_OK)
  {
    return USBH_FAIL;
  }

  phost->pActiveClass->pData = (HID_HandleTypeDef *)USBH_malloc(sizeof(HID_HandleTypeDef));
  HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  if (HID_Handle == NULL)
  {
    USBH_DbgLog("Cannot allocate memory for HID Handle");
    return USBH_FAIL;
  }

  /* Initialize hid handler */
  (void)USBH_memset(HID_Handle, 0, sizeof(HID_HandleTypeDef));

  HID_Handle->state = USBH_HID_ERROR;

  //根据HID设备的 PID、VID进行不同的设备类型识别
  if( phost->device.DevDesc.idProduct == Wired_PS2_PID && phost->device.DevDesc.idVendor==Wired_PS2_VID )
  {	  //有线PS2手柄的PID/VID
	  USBH_UsrLog("Wired PS2 device found!");
	  gamepad_brand = PS2_USB_Wired;
	  GamePadInterface = &Wired_USB_PS2Gamepad;
	  HID_Handle->Init = USBH_HID_GamePadInit;
  }
  else if( phost->device.DevDesc.idProduct == WiredV2_PS2_PID && phost->device.DevDesc.idVendor==WiredV2_PS2_VID )
  {
	  //第二代usb有线
	  USBH_UsrLog("Wired PS2 V2 device found!");
	  gamepad_brand = PS2_USB_WiredV2;
	  GamePadInterface = &Wired_USB_PS2Gamepad;
	  HID_Handle->Init = USBH_HID_GamePadInit;
  }
  
//  else if( phost->device.DevDesc.idProduct == Wireless_PC_PS2_PID && phost->device.DevDesc.idVendor==Wireless_PC_PS2_VID )
//  {	  //无线PS2手柄 PC模式 PID/VID
//	  USBH_UsrLog("Wireless PC PS2 device found!");
//	  gamepad_brand = PS2_Wiredless_PC;
//	  HID_Handle->Init = USBH_HID_GamePadInit;
//  }
  
  else if( phost->device.DevDesc.idProduct == Xbox360_GamePad_PID && phost->device.DevDesc.idVendor==Xbox360_GamePad_VID )
  {	
		//以制造商数据区分同样的PID、VID产品
		if( strcmp(GamePad_Manufacturer,"Flydigi") == 0 )
		{
			gamepad_brand = Xbox360;
		}
		else
		{
			gamepad_brand = PS2_USB_Wiredless;
		}
		//XBOX360平台
		USBH_UsrLog("XBox360 device found!");
		GamePadInterface = &Xbox360Gamepad;
		HID_Handle->Init = USBH_HID_GamePadInit;
  }
  
  else
  {
	gamepad_brand = UnKnown_Dev;//未知的设备类型
	USBH_UsrLog("Protocol not supported.PID:%X\tVID:%X\r\n",phost->device.DevDesc.idProduct,phost->device.DevDesc.idVendor);
    return USBH_FAIL;
  }

  HID_Handle->state     = USBH_HID_INIT;
  HID_Handle->ctl_state = USBH_HID_REQ_INIT;
  HID_Handle->ep_addr   = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
  HID_Handle->length    = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
  HID_Handle->poll      = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bInterval;
  
  if (HID_Handle->poll < HID_MIN_POLL)
  {
    HID_Handle->poll = HID_MIN_POLL;
  }

  /* Check of available number of endpoints */
  /* Find the number of EPs in the Interface Descriptor */
  /* Choose the lower number in order not to overrun the buffer allocated */
  max_ep = ((phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS) ?
            phost->device.CfgDesc.Itf_Desc[interface].bNumEndpoints : USBH_MAX_NUM_ENDPOINTS);


  /* Decode endpoint IN and OUT address from interface descriptor */
  for (num = 0U; num < max_ep; num++)
  {
//	USBH_UsrLog("Found endpoint %d: Address = 0x%02X, Type = 0x%02X", num,
//				phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bEndpointAddress,
//				phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bmAttributes);
	  
    if ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bEndpointAddress & 0x80U) != 0U)
    {
      HID_Handle->InEp = (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bEndpointAddress);
      HID_Handle->InPipe = USBH_AllocPipe(phost, HID_Handle->InEp);
      ep_mps = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].wMaxPacketSize;

      /* Open pipe for IN endpoint */
      (void)USBH_OpenPipe(phost, HID_Handle->InPipe, HID_Handle->InEp, phost->device.address,
                          phost->device.speed, USB_EP_TYPE_INTR, ep_mps);

      (void)USBH_LL_SetToggle(phost, HID_Handle->InPipe, 0U);
    }
    else
    {
      HID_Handle->OutEp = (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].bEndpointAddress);
      HID_Handle->OutPipe = USBH_AllocPipe(phost, HID_Handle->OutEp);
      ep_mps = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[num].wMaxPacketSize;

      /* Open pipe for OUT endpoint */
      (void)USBH_OpenPipe(phost, HID_Handle->OutPipe, HID_Handle->OutEp, phost->device.address,
                          phost->device.speed, USB_EP_TYPE_INTR, ep_mps);

      (void)USBH_LL_SetToggle(phost, HID_Handle->OutPipe, 0U);
    }
  }
  
  return USBH_OK;
}

/**
  * @brief  USBH_HID_InterfaceDeInit
  *         The function DeInit the Pipes used for the HID class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  if (HID_Handle->InPipe != 0x00U)
  {
    (void)USBH_ClosePipe(phost, HID_Handle->InPipe);
    (void)USBH_FreePipe(phost, HID_Handle->InPipe);
    HID_Handle->InPipe = 0U;     /* Reset the pipe as Free */
  }

  if (HID_Handle->OutPipe != 0x00U)
  {
    (void)USBH_ClosePipe(phost, HID_Handle->OutPipe);
    (void)USBH_FreePipe(phost, HID_Handle->OutPipe);
    HID_Handle->OutPipe = 0U;     /* Reset the pipe as Free */
  }

  if ((phost->pActiveClass->pData) != NULL)
  {
    USBH_free(phost->pActiveClass->pData);
    phost->pActiveClass->pData = 0U;
  }

  //设备拔出时,将进行反初始化
  USB_GamePad_PullOutCallback();
  
  return USBH_OK;
}

/**
  * @brief  USBH_HID_ClassRequest
  *         The function is responsible for handling Standard requests
  *         for HID class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_ClassRequest(USBH_HandleTypeDef *phost)
{

  USBH_StatusTypeDef status         = USBH_BUSY;
  USBH_StatusTypeDef classReqStatus = USBH_BUSY;
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  /* Switch HID state machine */
  switch (HID_Handle->ctl_state)
  {
    case USBH_HID_REQ_INIT:
    case USBH_HID_REQ_GET_HID_DESC:

      USBH_HID_ParseHIDDesc(&HID_Handle->HID_Desc, phost->device.CfgDesc_Raw);

      HID_Handle->ctl_state = USBH_HID_REQ_GET_REPORT_DESC;

      break;
    case USBH_HID_REQ_GET_REPORT_DESC:

      /* Get Report Desc */
      classReqStatus = USBH_HID_GetHIDReportDescriptor(phost, HID_Handle->HID_Desc.wItemLength);
      if (classReqStatus == USBH_OK)
      {
        /* The descriptor is available in phost->device.Data */
        HID_Handle->ctl_state = USBH_HID_REQ_SET_IDLE;
      }
      else if (classReqStatus == USBH_NOT_SUPPORTED)
      {
        USBH_ErrLog("Control error: HID: Device Get Report Descriptor request failed");
        status = USBH_OK;
      }
      else
      {
        /* .. */
      }

      break;

    case USBH_HID_REQ_SET_IDLE:

      classReqStatus = USBH_HID_SetIdle(phost, 0U, 0U);

      /* set Idle */
      if (classReqStatus == USBH_OK)
      {
        HID_Handle->ctl_state = USBH_HID_REQ_SET_PROTOCOL;
      }
      else
      {
        if (classReqStatus == USBH_NOT_SUPPORTED)
        {
          HID_Handle->ctl_state = USBH_HID_REQ_SET_PROTOCOL;
        }
      }
      break;

    case USBH_HID_REQ_SET_PROTOCOL:
      /* set protocol */
      classReqStatus = USBH_HID_SetProtocol(phost, 0U);
      if (classReqStatus == USBH_OK)
      {
        HID_Handle->ctl_state = USBH_HID_REQ_IDLE;

        /* all requests performed */
        phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
        status = USBH_OK;
      }
      else if (classReqStatus == USBH_NOT_SUPPORTED)
      {
		  USBH_ErrLog("TODO:Cannel:-->Control error: HID: Device Set protocol request failed");
        status = USBH_OK;
      }
      else
      {
        /* .. */
      }
      break;

    case USBH_HID_REQ_IDLE:
    default:
      break;
  }

  return status;
}

/**
  * @brief  USBH_HID_Process
  *         The function is for managing state machine for HID data transfers
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_Process(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_OK;
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;
  uint32_t XferSize;
	
  switch (HID_Handle->state)
  {
    case USBH_HID_INIT:
      status = HID_Handle->Init(phost);

      if (status == USBH_OK)
      {
        HID_Handle->state = USBH_HID_IDLE;
      }
      else
      {
        USBH_ErrLog("HID Class Init failed");
        HID_Handle->state = USBH_HID_ERROR;
        status = USBH_FAIL;
      }

#if (USBH_USE_OS == 1U)
      phost->os_msg = (uint32_t)USBH_URB_EVENT;
#if (osCMSIS < 0x20000U)
      (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
      (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
      break;

    case USBH_HID_IDLE:
		
	//发送GetReport请求，请求HID设备返回数据.在此之前 HID_Handle->pData 必须完成初始化,这一步通常在 HID_Handle->Init 函数完成,否则将无法请求成功
	
      status = USBH_HID_GetReport(phost, 0x01U, 0U, HID_Handle->pData, (uint8_t)HID_Handle->length);

      if (status == USBH_OK)
      {
        HID_Handle->state = USBH_HID_SYNC;
      }
      else if (status == USBH_BUSY)
      {
        HID_Handle->state = USBH_HID_IDLE;
        status = USBH_OK;
      }
      else if (status == USBH_NOT_SUPPORTED)
      {
        HID_Handle->state = USBH_HID_SYNC;
        status = USBH_OK;
      }
      else
      {
        HID_Handle->state = USBH_HID_ERROR;
        status = USBH_FAIL;
      }

#if (USBH_USE_OS == 1U)
      phost->os_msg = (uint32_t)USBH_URB_EVENT;
#if (osCMSIS < 0x20000U)
      (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
      (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
      break;

    case USBH_HID_SYNC:
      /* Sync with start of Even Frame */
      if ((phost->Timer & 1U) != 0U)
      {
        HID_Handle->state = USBH_HID_GET_DATA;
      }

#if (USBH_USE_OS == 1U)
      phost->os_msg = (uint32_t)USBH_URB_EVENT;
#if (osCMSIS < 0x20000U)
      (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
      (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
      break;

    case USBH_HID_GET_DATA:
		
	//接收HID设备的数据,保存在 HID_Handle->pData
      (void)USBH_InterruptReceiveData(phost, HID_Handle->pData,
                                      (uint8_t)HID_Handle->length,
                                      HID_Handle->InPipe);

      HID_Handle->state = USBH_HID_POLL;
      HID_Handle->timer = phost->Timer;
      HID_Handle->DataReady = 0U;
      break;

    case USBH_HID_POLL:
      if (USBH_LL_GetURBState(phost, HID_Handle->InPipe) == USBH_URB_DONE)
      {
        XferSize = USBH_LL_GetLastXferSize(phost, HID_Handle->InPipe);

        if ((HID_Handle->DataReady == 0U) && (XferSize != 0U) && (HID_Handle->fifo.buf != NULL))
        {
		  //将保存在HID_Handle->pData的HID设备数据写入到fifo中，后续通过fifo来获取hid设备的数据
          (void)USBH_HID_FifoWrite(&HID_Handle->fifo, HID_Handle->pData, HID_Handle->length); 
          HID_Handle->DataReady = 1U;
          USBH_HID_EventCallback(phost);
	  
#if (USBH_USE_OS == 1U)
          phost->os_msg = (uint32_t)USBH_URB_EVENT;
#if (osCMSIS < 0x20000U)
          (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
          (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
        }
      }
      else
      {
        /* IN Endpoint Stalled */
        if (USBH_LL_GetURBState(phost, HID_Handle->InPipe) == USBH_URB_STALL)
        {
          /* Issue Clear Feature on interrupt IN endpoint */
          if (USBH_ClrFeature(phost, HID_Handle->ep_addr) == USBH_OK)
          {
            /* Change state to issue next IN token */
            HID_Handle->state = USBH_HID_GET_DATA;
          }
        }
      }
      break;

    default:
      break;
  }

  return status;
}

/**
  * @brief  USBH_HID_SOFProcess
  *         The function is for managing the SOF Process
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_HID_SOFProcess(USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  if (HID_Handle->state == USBH_HID_POLL)
  {
    if ((phost->Timer - HID_Handle->timer) >= HID_Handle->poll)
    {
      HID_Handle->state = USBH_HID_GET_DATA;

#if (USBH_USE_OS == 1U)
      phost->os_msg = (uint32_t)USBH_URB_EVENT;
#if (osCMSIS < 0x20000U)
      (void)osMessagePut(phost->os_event, phost->os_msg, 0U);
#else
      (void)osMessageQueuePut(phost->os_event, &phost->os_msg, 0U, 0U);
#endif
#endif
    }
  }
  return USBH_OK;
}

/**
  * @brief  USBH_ParseHIDDesc
  *         This function Parse the HID descriptor
  * @param  desc: HID Descriptor
  * @param  buf: Buffer where the source descriptor is available
  * @retval None
  */
static void USBH_HID_ParseHIDDesc(HID_DescTypeDef *desc, uint8_t *buf)
{
  USBH_DescHeader_t *pdesc = (USBH_DescHeader_t *)buf;
  uint16_t CfgDescLen;
  uint16_t ptr;

  CfgDescLen = LE16(buf + 2U);

  if (CfgDescLen > USB_CONFIGURATION_DESC_SIZE)
  {
    ptr = USB_LEN_CFG_DESC;

    while (ptr < CfgDescLen)
    {
      pdesc = USBH_GetNextDesc((uint8_t *)pdesc, &ptr);

      if (pdesc->bDescriptorType == USB_DESC_TYPE_HID)
      {
        desc->bLength = *(uint8_t *)((uint8_t *)pdesc + 0U);
        desc->bDescriptorType = *(uint8_t *)((uint8_t *)pdesc + 1U);
        desc->bcdHID = LE16((uint8_t *)pdesc + 2U);
        desc->bCountryCode = *(uint8_t *)((uint8_t *)pdesc + 4U);
        desc->bNumDescriptors = *(uint8_t *)((uint8_t *)pdesc + 5U);
        desc->bReportDescriptorType = *(uint8_t *)((uint8_t *)pdesc + 6U);
        desc->wItemLength = LE16((uint8_t *)pdesc + 7U);
        break;
      }
    }
  }
}

///////////////////////// 初始化、数据解码 ///////////////////////////////
//手柄通用输入报告
static uint8_t GamePadReportData[128] = { 0 }; //用于存放HID设备发送过来的数据

//ps2初始化函数
USBH_StatusTypeDef USBH_HID_GamePadInit(USBH_HandleTypeDef *phost)
{
	HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;
	
	//HID_Handle->length在初始化设备时由HID设备确定
	if (HID_Handle->length > sizeof(GamePadReportData))
	{
		HID_Handle->length = (uint16_t)sizeof(GamePadReportData);
	}
	
	//初始化pData,必须步骤.
	HID_Handle->pData = GamePadReportData;
	
	if ((HID_QUEUE_SIZE * sizeof(GamePadReportData)) > sizeof(phost->device.Data))
	{	//sizeof(phost->device.Data) 大小由 USBH_MAX_DATA_BUFFER 配置
		return USBH_FAIL;
	}
	else
	{
		//初始化fifo
		USBH_HID_FifoInit(&HID_Handle->fifo, phost->device.Data, (uint16_t)(HID_QUEUE_SIZE * sizeof(GamePadReportData)));
	}
	
	//USB插入
	USB_GamePad_InsertCallback();
	
	//switch pro手柄,需要特殊初始化
	if( gamepad_brand == SwitchPro )
	{
		SwitchPro_GamePad_Init(phost);
	}
	
	return USBH_OK;
}

//ps2数据解码,入口参数为HID设备.运行环境为任务.
USBH_StatusTypeDef USBH_HID_PS2_Decode(USBH_HandleTypeDef *phost)
{
	HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;
	
	//检查是否成功识别到手柄类型
	if( gamepad_brand == UnKnown_Dev ) return USBH_FAIL;
	
	//检查hid设备是否有数据
	if ((HID_Handle->length == 0U) || (HID_Handle->fifo.buf == NULL))
	{
		return USBH_FAIL;
	}
	
	//从fifo中读取hid数据放入GamePadReportData，并进行解析
	if (USBH_HID_FifoRead(&HID_Handle->fifo, (uint8_t* )GamePadReportData, HID_Handle->length) == HID_Handle->length)
	{
		/* 根据实际接入的不同游戏手柄,调用不同的处理函数 */
		GamePadDebug.ready = 1;//手柄调试,表示有数据接收
		
		//xbox360格式解码
		//LX  GamePadReportData[7]  左段128-0,右段0-127
		//LY GamePadReportData[9]  下段128-0,上段0-127 
		//RX GamePadReportData[11]  左段128-0,右段0-127
		//RY GamePadReportData[13]  下段128-0,上段0-127 
		//LT GamePadReportData[4] 0-255
		//RT GamePadReportData[5] 0-255
		// GamePadReportData[3] 1:LB 2:RB 3:logo 4:? 5:A 6:B 7:X 8:Y
		// GamePadReportData[2] 1234:上下左右 5:start 6:seltec 7:Lrock 8:Rrock

		if( gamepad_brand == Xbox360 || gamepad_brand == PS2_USB_Wiredless )
		{
			Xbox360_gamepad_Decode(phost,GamePadReportData,HID_Handle->length);
		}
		else if( gamepad_brand == PS2_USB_Wired )
		{
			Wired_USB_PS2gamepad_Decode(phost,GamePadReportData,HID_Handle->length);
		}
		else if( gamepad_brand == PS2_USB_WiredV2 )
		{
			Wired_USB_V2_PS2gamepad_Decode(phost,GamePadReportData,HID_Handle->length);
		}
		
		return USBH_OK;
	}

	return   USBH_FAIL;

}

//SwitchPro手柄初始化
void SwitchPro_GamePad_Init(USBH_HandleTypeDef *phost)
{
	#define DelayTime 100
	HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;
	
	USBH_UsrLog("switch pro init now...");
	
	/* 以下数据来源于逆向工程抓包,不必理会含义 */
	
	uint8_t initcode[64] = { 0 };
	initcode[0] = 0x80; initcode[1] = 0x01;
	while( USBH_OK!= USBH_InterruptSendData(phost,initcode,64,HID_Handle->OutPipe) ) ; 
	USBH_Delay(DelayTime);
	
	initcode[1] = 0x02;
	while( USBH_OK!= USBH_InterruptSendData(phost,initcode,64,HID_Handle->OutPipe) ) ; 
	USBH_Delay(DelayTime);
	
	initcode[1] = 0x03;
	while( USBH_OK!= USBH_InterruptSendData(phost,initcode,64,HID_Handle->OutPipe) ) ; 
	USBH_Delay(DelayTime);
	
	initcode[1] = 0x02;
	while( USBH_OK!= USBH_InterruptSendData(phost,initcode,64,HID_Handle->OutPipe) ) ; 
	USBH_Delay(DelayTime);
	
	initcode[1] = 0x04;
	while( USBH_OK!= USBH_InterruptSendData(phost,initcode,64,HID_Handle->OutPipe) ) ;
	USBH_Delay(DelayTime);

	USBH_UsrLog("switch pro init ok!");
	
	/* 写入上述信息后,switch pro手柄才会激活数据输出 */
}

