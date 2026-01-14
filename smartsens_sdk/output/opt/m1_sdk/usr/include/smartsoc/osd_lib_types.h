/*
 * @Filename: osd_ioctl.h
 * @Author: Jingwen Bai
 * @Date: 2024-07-08 19:03:44
 * @Description: osd device iotcl
 */

#ifndef __OSD_TYPES_H__
#define __OSD_TYPES_H__

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

typedef unsigned long handle_t;
#define INVALID_HANDLE (0)

typedef uint8_t BYTE;
typedef uint8_t *PBYTE;
typedef uint16_t WORD;
typedef uint16_t *PWORD;
typedef uint32_t  DWORD;

typedef struct buf_handle {
    uint32_t buf_type;
    union {
        int fd_dmabuf;
        uint32_t sram_buf_handle;
    } buf;
}buf_handle;

namespace fdevice{

/**
 * @name: LAYER_HANDLE
 * @desc：OSD IP 共8个Layer可控制
 */
typedef enum ssLAYER_HANDLE
{    
    LAYER0_HANDLE,
    LAYER1_HANDLE,
    LAYER2_HANDLE,
    LAYER3_HANDLE,
    LAYER4_HANDLE,
    LAYER5_HANDLE,
    LAYER6_HANDLE,
    LAYER7_HANDLE,
    LAYER_HANDLE_MAX
}LAYER_HANDLE;

/**
 * @name: LAYER_OPACITY
 * @desc：OSD IP 共有5个透明度可供设置
 */
typedef enum ssLAYER_OPACITY
{
    LAYER_OPACITY_00,//全透明，
    LAYER_OPACITY_25,//不透明度25%
    LAYER_OPACITY_50,//不透明度50%
    LAYER_OPACITY_75,//不透明度75%
    LAYER_OPACITY_100,//不透明度100%
    LAYER_BUTT
}LAYER_OPACITY;

/**
 * @name: LAYER_TYPE
 * @desc：LAYER 编码方式,目前支持两种，行程编码和四边形编码
 */
typedef enum ssLAYER_TYPE
{
  SS_TYPE_RLE,//行程编码
  SS_TYPE_QUADRANGLE,//四边形编码
  SS_TYPE_BUTT
}LAYER_TYPE;


/**
 * @name: BITMAP_S
 * @desc：行程编码时，位图信息
 * @para1: 编码后数据的内存首地址
 * @para2: 编码后数据的总长度
 */
typedef struct ssRLE_S
{
    char*       pdata_addr;//位图行程编码后数据首地址
    int         data_length;//位图行程编码数据长度
    buf_handle  osd_buf;
}RLE_S;


/**
 * @name: QUADRANGLE_S
 * @desc： QUADRANGLE编码时的编码信息
 * @para1: 编码后数据的内存首地址
 * @para2: 编码后数据的总长度
 */
typedef struct ssQUADRANGLE_S
{
    char*       pdata_addr;//QUADRANGLE编码后数据首地址
    int         data_length;//QUADRANGLE编码数据长度
    buf_handle  osd_buf;
}QUADRANGLE_S;

/**
 * @name: LAYER_COORD
 * @desc： 定义LAYER的起始坐标，坐标系以图像中心为原点
 * @para1: layer_start_x 
 * @para2: layer_start_y
 */
typedef struct ssLAYER_COORD
{
    int16_t layer_start_x;
    int16_t layer_start_y;
}LAYER_COORD;

/**
 * @name: ssLAYER_SIZE
 * @desc： layer 宽 高{int w, int h}; 超出layer size 部分的图像将不会被绘制
 * @para1: layer_width 
 * @para2: layer_height
 */
typedef struct ssLAYER_SIZE
{
    int layer_width;
    int layer_height;
}LAYER_SIZE;





/********************************** Region ***************************/
typedef enum tagQUADRANGLETYPE
{
    TYPE_HOLLOW = 0, 
    TYPE_SOLID  
} QUADRANGLETYPE;

typedef enum tagALPHATYPE
{
    TYPE_ALPHA25 = 0 ,
    TYPE_ALPHA50,
    TYPE_ALPHA75,
    TYPE_ALPHA100 
} ALPHATYPE;

typedef enum tagSTEPSIZE
{
    TYPE_STEP_SHORT = 0,
    TYPE_STEP_LONG
} STEPSIZE;

/**
 * @brief 顶点排列情形枚举量
 */
enum VERTEXTYPE
{
    TYPE_LINE = 0,  //四点共线
    TYPE_TRIANGLE,  //三点共线
    TYPE_CONVERX,   //凸四边形
    TYPE_CONCAVE    //凹四边形
};

/**
 * @brief Region区域枚举量
 */
enum RGNTYPE
{
    TYPE_IMAGE = 0, //图像区域
    TYPE_GRAPHIC    //图形区域
};

/**
 * @brief 优先级枚举量
 */
enum RGNPRIORITY
{
    PRIORITY_0 = 0,
    PRIORITY_1 ,
    PRIORITY_2 ,
    PRIORITY_3 , 
    PRIORITY_4 
};


/*坐标点*/
typedef struct tagPOINT_S
{
    int x;
    int y;
}POINT_S;

/*大小*/
typedef struct tagSIZE_S
{
    int w;
    int h;
}SIZE_S;

/* 整型向量 */
typedef struct tagVECTOR_INT_S
{
    int x;
    int y;
}VECTOR_INT_S;

/*位图信息数据结构*/
typedef struct tagSSBITMAP_ATTR_S
{
    DWORD bmHead;  //head  0x5353424d ‘SSBM’
    DWORD bmWidth; 
    DWORD bmHeigth;
    DWORD bmColorNum;
    BYTE* bmData;  // ptr of data area
}SSBITMAP_ATTR_S;

/*四边形顶点*/
typedef struct tagVERTEXS_S
{
    POINT_S points[4];
}VERTEXS_S;

/*凸四边形信息*/
typedef struct tagCOVER_ATTR_S
{
    int  colorIdx; 
    QUADRANGLETYPE  eSolid;  
    ALPHATYPE       alpha;
    VERTEXS_S       vertex_out;
    VERTEXS_S       vertex_in;
}COVER_ATTR_S;

/* 添加的位图信息 */
typedef struct tagBITMAP_INFO_S
{
    const char*       pSSbmpFile;
    ALPHATYPE   alpha;
    POINT_S     position;
}BITMAP_INFO_S;

/* region attr */
typedef struct tagRGN_ATTR_S
{ 
    RGNTYPE enType;        
    // int nSwitch;
    // RGNPRIORITY nPriority;      /* 区域优先级 */
    SIZE_S size_s;      /* 区域大小 */
    // POINT_S start_s;    /* 区域起始坐标 */
}RGN_ATTR_S;


/**
 * @name: LAYER_ATTR_S
 * @desc： 定义LAYER属性的结构体
 * @para: layerStart 图层的开始坐标 ，根据OSD IP SPEC，坐标轴原点在图像中心，x,y取值范围[16-bit]:{-32768，32767}
 * @para: ...后续的可能扩展
 */
typedef struct ssLAYER_ATTR_S
{
    LAYER_TYPE      codeTYPE;
    LAYER_COORD     layerStart;
    LAYER_SIZE      layerSize;
    int             sensor_flag;
    QUADRANGLE_S    layer_data_QR;
    RLE_S           layer_data_RLE;
    RGN_ATTR_S      layer_rgn;
}LAYER_ATTR_S;


typedef struct ssDMA_BUFFER_ATTR_S{
	void* dma = nullptr;
	void* dma_2 = nullptr;
}DMA_BUFFER_ATTR_S;



#pragma pack()


} // namespace fdevice



#endif