#ifndef __OSD_DATA_ARRAY_H_
#define __OSD_DATA_ARRAY_H_

#include "OsdTypeDef.h"

/**
 * @brief 图像数据模板类，定义长，宽，像素数据深度（rgb-> 3  yuv422-> 2  rgba -> 4）
 * 
 * @tparam T 
 */
template <class T>
class CImageDataArray
{
protected:
    int m_nColorNum; //图像包含的颜色数量
    int m_nWidth;  //图像宽
    int m_nHeight;  //图像高
    int m_nDim;     //pix 深度
    int m_nLineSize;  //一行的数据长度
    T *m_pData;

public:
    CImageDataArray()
    {
        m_nColorNum = 0 ;
        m_nWidth = m_nHeight = m_nDim = 0;
        m_nLineSize = 0;
        m_pData = nullptr;
    }
    virtual ~CImageDataArray()
    {
        if (m_pData != NULL)
            delete[] m_pData;
    }

    /**
     * @brief 根据width ，height ， dimension， 开辟图像数据空间；
     * 
     * @param W     width
     * @param H     heigth
     * @param D     dimension
     * @return true 
     * @return false 
     */
    bool SetSize(int W, int H, int D)
    {
        if (m_pData != NULL)
        {
            delete[] m_pData;
            m_nWidth = m_nHeight = m_nDim = 0;
        }
        if (W == 0 || H == 0 )
            return false;
        if ((m_pData = new T[W * H * D]()) == nullptr)
            return false;
        m_nWidth = W;
        m_nHeight = H;
        m_nDim = D;
        m_nLineSize = W * D;
        return true;
    }
    /**
     * @brief   清除图像数据空间；
     * 
     */
    void Clear()
    {
        if (m_pData != nullptr)
        {
            delete[] m_pData;
            m_pData = nullptr;
        }
        m_nHeight = m_nWidth = m_nDim = 0;
    }
    
    /**
     * @brief       图像数据行反转  
     * 
     * @return true 
     * @return false 
     */
    bool ImageDataLineFlip()
    {
        if (m_pData == nullptr)
            return false;

        int nP;
        T *pTmpLine;
        nP = m_nWidth * m_nDim;

        if ((pTmpLine = new T[nP]) == nullptr)
            return false;

        for (int y = 0; y < m_nHeight / 2; y++)
        {
            memcpy(pTmpLine, m_pData + y * nP, nP * sizeof(T));
            memcpy(m_pData + y * nP, m_pData + (m_nHeight - 1 - y) * nP, nP * sizeof(T));
            memcpy(m_pData + (m_nHeight - 1 - y) * nP, pTmpLine, nP * sizeof(T));
        }

        delete[] pTmpLine;
        return true;
    }
    inline int GetWidth() { return m_nWidth; }
    inline int GetHeight() { return m_nHeight; }
    inline int GetDim() { return m_nDim; }
    inline int GetPixNum() { return m_nWidth * m_nHeight; }
    inline T *GetData() { return m_pData; }
    inline T *GetScanLine(int nY)
    {
        if (nY < 0)
            nY = 0;
        if (nY >= m_nHeight)
            nY = m_nHeight - 1;
        return m_pData + nY * m_nLineSize;
    }
    inline T *GetScanPix(int nX, int nY)
    {
        int pos = (nY * m_nWidth + nX) * m_nDim;
        return m_pData + pos;
    }
    inline long GetDataSize()
    {
        long size = 0 ;
        size = GetPixNum() * GetDim();
        return size ;
    }
};

typedef CImageDataArray<BYTE> CImageDataArray_BYTE;
typedef CImageDataArray<WORD> CImageDataArray_WORD;
typedef CImageDataArray<DWORD> CImageDataArray_DWORD;

#endif