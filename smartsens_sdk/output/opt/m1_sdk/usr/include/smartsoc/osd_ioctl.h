/*
 * @Filename: osd_ioctl.h
 * @Author: Jingwen Bai
 * @Date: 2024-07-08 19:03:44
 * @Description: osd device iotcl
 */

#ifndef __OSD_IOCTL__
#define __OSD_IOCTL__

#include "osd_lib_types.h"

#define OSD_DEVICE "/dev/osddev"
#define MAGIC_NUMBER 'o'
#define IPREG_W_COLOR_LUT			_IO(MAGIC_NUMBER , 0)
#define IPREG_W_LAYER_SWITCH		_IO(MAGIC_NUMBER , 1)
#define IPREG_W_NEWFRM_FLAG			_IO(MAGIC_NUMBER , 2)
#define IPREG_R_NEWFRM_FLAG			_IO(MAGIC_NUMBER , 3)
#define IPREG_W_DATA_ADDR			_IO(MAGIC_NUMBER , 4)
#define IPREG_W_DATA_LEN			_IO(MAGIC_NUMBER , 5)
#define IPREG_W_START_COORD			_IO(MAGIC_NUMBER , 6)
#define IPREG_W_SRAM_LEN			_IO(MAGIC_NUMBER , 7)
#define IPREG_R_IP_STATUS			_IO(MAGIC_NUMBER , 8)
#define IPREG_W_IP_CONFIG_PARAM     _IO(MAGIC_NUMBER , 9)
#define IPREG_W_DATA_BY_LAYER		_IO(MAGIC_NUMBER , 10)


typedef volatile unsigned int __offset;


typedef struct osd_param
{
    u_int8_t    osd_color_lut[120];
    u_int32_t   osd_data_addr_start[8];
    u_int32_t   osd_data_addr_len[8];
    u_int8_t    osd_layer_xy[32];
    u_int8_t    osd_sram_len[8];
    u_int8_t    osd_layer_en;
    u_int8_t    osd_layer_ready;
    u_int8_t    osd_ip_status;
    u_int8_t    osd_ip_master;
    u_int8_t    osd_ip_reset;

    u_int32_t   data_addr_phy;
    u_int32_t   data_len_;
    buf_handle  osd_buf;

}osd_param_t;

typedef enum layer_ctrl_type
{
    OSD_COLOR_LUT = 0,
    OSD_CODE_DATA,
    OSD_LAYER_CROOD,
    OSD_SRAM_LEN,
    OSD_CTRL_BUTT
}ctrl_type;

typedef struct osd_ioctl_param
{
    ctrl_type   type;
    u_int32_t   reg_data;
    u_int8_t    data_lenth;
    u_int8_t    offset;
    u_int32_t   code_data_addr;
    u_int32_t   code_data_len;
    buf_handle  osd_buf;
}osd_ioctl_param_t;

#endif