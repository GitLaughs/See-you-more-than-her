#ifndef __OSD_BGR_IMAGE_H_
#define __OSD_BGR_IMAGE_H_


/*forward reference*/
class CBGRImage;
#include "OsdDataArray.h"
/**
 * @brief yuv+alpha 四通道分量 图像数据类，单像素数据深度 4 Byte
 * 
 */
class CYUVAImage : public CImageDataArray_BYTE
{
public:
    bool Create(int nWidth, int nHeight);
};

/**
 * @brief yuv444 1obit 图像数据类，单像素数据深度 6 Byte
 * 
 */
class CYUV444Image_10bit: public  CImageDataArray_WORD
{
public:
    bool Create(int nWidth, int nHeight);
    bool SaveYUVComponentToFile(const char *pFileName);
};


/**
 * @brief yuv422 8bit 图像数据类，单像素数据深度 2 Byte
 * 
 */
class CYUV422Image : public CImageDataArray_BYTE
{
public:
    bool Create(int nWidth, int nHeight);
    bool YUYV2BGR(CBGRImage *pOutImg);

    //for test 
    bool SaveYUYVFile(const char *pFileName);
    bool SaveYUYVToTxtFile(const char *pFileName);
    bool SaveYUYVToBmpFile(const char * pFileName);
};


/**
 * @brief yuv422 10bit 图像数据类，单像素数据深度 4 Byte
 * 
 */
class CYUV422Image_10bit : public CImageDataArray_WORD
{
public:
    bool Create(int nWidth, int nHeight);
    bool YUYV2BGR(CBGRImage *pOutImg);
    bool YUYV422ToYUV444(CYUV444Image_10bit *pYUV444Image);

    //for test 
    bool SaveYUYV444Component(const char *pFileName);
    bool SaveYUYVToTxtFile(const char *pFileName);
    bool SaveYUYVToBmpFile(const char * pFileName);
};


/**
 * @brief yuv444 图像数据类，单像素数据深度 3 Byte
 * 
 */
class CYUV444Image: public  CImageDataArray_BYTE
{
public:
    bool Create(int nWidth, int nHeight);
    bool SaveYUV444File(const char *pFileName);
};

/**
 * @brief bgr 图像数据类，单像素数据深度 3 Byte
 * 
 */
class CBGRImage : public CImageDataArray_BYTE
{
public:
    static int pixBGR24ToYUV444(BYTE* pBGR , BYTE* pYUV444);
    bool Create(int nWidth, int nHeight);
    bool BGR2YUYV(CYUV422Image *pOutImage);
    bool BGR2YUYV10bit(CYUV422Image_10bit *pOutImage);
    bool BGR2YUV444(CYUV444Image *pOutImage);
    bool BGR2YUVA(CYUVAImage *pOutImage , int nAlpha);
    bool LoadBMPFile(const char *pFileName);
    bool SaveBmpFile(const char *pFileName);
    // bool Copy(CBGRImage *pInImage);
};



/**
 * @brief 索引颜色单通道数据类，1pix 1byte  数据为颜色索引,像素数据深度1 
 * 
 */
class CIdxImage : public CImageDataArray_BYTE
{
public:
    bool Create(int nWidth, int nHeight);
    bool LoadSsbmpFile(const char *pFileName);
};


/**
 * @brief 颜色索引 + ALPHA 双通道数据类，1pix 2byte ,像素数据深度2
 * 
 */
class CIdxAImage : public CImageDataArray_BYTE
{
public:
    bool Create(int nWidth, int nHeight);
    bool Clean();
};




#endif
