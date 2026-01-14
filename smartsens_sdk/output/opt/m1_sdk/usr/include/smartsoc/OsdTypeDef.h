#ifndef _OSD_TYPE_DEF_H_
#define _OSD_TYPE_DEF_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <vector>
#include <mutex>

#include <sszlog.h>
#include "osd_lib_types.h"

using namespace fdevice;

typedef struct tagBITMAPFILEHEADER
{
    WORD bfType; 
    DWORD bfSize; 
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits; 
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;          
    DWORD biWidth;         
    DWORD biHeight;       
    WORD biPlanes;     
    WORD biBitCount;   
    DWORD biCompression; 
    DWORD biSizeImage;   
    DWORD biXPelsPerMeter; 
    DWORD biYPelsPerMeter; 
    DWORD biClrUsed;      
    DWORD biClrImportant;  
} BITMAPINFOHEADER;           

typedef struct tagRGBQUAD {
    BYTE   rgbBlue;
    BYTE   rgbGreen;
    BYTE   rgbRed;
    BYTE   rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER    bmiHeader;
    RGBQUAD             bmiColors[1];
} BITMAPINFO;

#pragma pack()

#define DEFAULT_ALPHA 0x3
#define DEFAULT_OVERLAY 0x1f
#define DEFAULT_REVERT 0x1e

#define LINE_MAX_QR_NUM 4 
#define LAYER_MAX_QR_NUM 32
#define LAYER_MAX_WIDTH      3840
#define LAYER_MAX_HEIGHT     2160

#define RLTYPE_SHORT    0
#define RLTYPE_LONG    1

#define LAYERNUM 8
#define NAMESTRLEN 64
#define CACHE_MAX_LEN   512

#define BINFORMAT  "Layer%01d_encode"


#ifndef SWITCH_ON
#define SWITCH_ON      1
#endif

#ifndef SWITCH_OFF
#define SWITCH_OFF     0
#endif

#ifndef max2
#define max2(a,b)  a>b?a:b
#endif
#ifndef min2
#define min2(a,b)  a<b?a:b
#endif

#ifndef max3
#define max3(a,b,c)  (a>b?a:b)>c?(a>b?a:b):c 
#endif
#ifndef min3
#define min3(a,b,c)  (a<b?a:b)<c?(a<b?a:b):c 
#endif


#ifndef max4
#define max4(a,b,c,d)  (a>b?a:b)>(c>d?c:d)?(a>b?a:b):(c>d?c:d) 
#endif
#ifndef min4
#define min4(a,b,c,d)  (a<b?a:b)<(c<d?c:d)?(a<b?a:b):(c<d?c:d) 
#endif

#ifndef bitctl
#define setbit(x,y) x|=(1<<y) 
#define clrbit(x,y) x&=~(1<<y) 
#endif

#define  mypause  std::cout << "pause!" << std::endl;

template <class T>
__inline int round(T value)
{
    if(value > 0 ) 
        return int(value + 0.5);
    else 
        return int(value - 0.5);
}   

//osd module structur declaration
#pragma pack(1)
typedef struct tagOSDCOORD
{
    short x ;
    short y ;
} OSDCOORD;

typedef struct tagQUADRANGLE
{
    OSDCOORD vertex[4];
} QUADRANGLE;


typedef struct tagQRE
{
    QUADRANGLE QRIn;
    QUADRANGLE QROut;
    BYTE solidType : 1;
    BYTE colorIndex : 5;
    BYTE transparency : 2;
} QRE;

typedef struct tagQREUNIT
{
    WORD   vertexByte[13];
    uint8_t solidType : 1;
    uint8_t colorIndex : 5;
    uint8_t transparency : 2;
    uint8_t reserve : 8 ;
} QREUNIT;



typedef struct tagRLEUNIT
{
    BYTE alpha : 2;
    BYTE colorIndex : 5;   // 0x3e: revert   0x3f: overlay
    BYTE lenFlag : 1; 
    BYTE compressSIze : 8; // maxSize 256
} RLEUNIT;

typedef struct tagOSDHEAD
{
    BYTE YUVcolorLut[90];
    BYTE layerSwitch;
    BYTE newFrame;
    // BYTE newFrame : 1 ;
    // BYTE reserve :7 ;
    DWORD layerAddr[LAYERNUM];
    DWORD dataLen[LAYERNUM];
    OSDCOORD startCoord[LAYERNUM];
    BYTE sramLen[LAYERNUM];
} OSDHEAD;

typedef struct tagLAYERHEAD
{
    DWORD width : 15;
    DWORD height : 15;
    DWORD empty :1 ;
    DWORD encodeType : 1; // 0: RLE  1: QRE
    DWORD reserve; //reserve byte for AXI(64bit width) read;
} LAYERHEAD;

typedef struct tagRGB24_S
{
    BYTE R;
    BYTE G;
    BYTE B;
}RGB24_S;

typedef struct tagBITMAP_ATTR_S
{
    DWORD head;
    DWORD width;
    DWORD height;
    DWORD colorNum;
}BITMAP_ATTR_S;

typedef struct tagCOLORLUT_S
{
    DWORD head;
    DWORD colorNum;
}COLORLUT_S;

typedef enum tagENCODETYPE
{
    TYPE_RUNLENGTH = 0,
    TYPE_QUADRANGLE
} ENCODETYPE;



#pragma pack()


#endif //_OSD_TYPE_DEF_H_