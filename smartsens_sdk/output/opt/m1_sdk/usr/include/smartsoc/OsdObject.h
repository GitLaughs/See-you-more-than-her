#ifndef _OSD_OBJECT_H_
#define _OSD_OBJECT_H_
#include "OsdImage.h"
#include "OsdTypeDef.h"

/**
 * @brief Layer编码对象基类 
 * 
 */
class COsdBaseObject
{
private:
    int m_nEncodeType;      //编码类型
    BYTE *m_pEncoded;       //编码后的数据
    int m_nEncodeLen;       //编码长度 
public:

    COsdBaseObject()
    {
        m_nEncodeType = -1 ;
        m_nEncodeLen = 0;
        m_pEncoded = nullptr;
    }
    virtual ~COsdBaseObject()
    {
        if (m_pEncoded != nullptr)
            delete[] m_pEncoded;
    }

    inline BYTE *GetEncodeData()
    {
        return m_pEncoded; 
    }
    inline bool SetEncodeData(BYTE *ptr)
    {
        m_pEncoded = ptr;
        return true; 
    }
    inline int  GetEncodeType()
    {
        return m_nEncodeType;
    }
    inline bool SetEncodeType(int nType)
    {
        m_nEncodeType = nType;
        return true;
    }
    inline int  GetEncodeLen()
    {
        return m_nEncodeLen;
    }
    inline bool SetEncodeLen(int nLen)
    {
        m_nEncodeLen = nLen;
        return true;
    }


    /**
     * @brief 保存编码后的数据到bin文件
     * 
     * @param pFileName     保存的文件名
     * @return true 
     * @return false 
     */
    bool SaveEncodedBinaryFile(const char *pFileName);

    /**
     * @brief 保存編碼后的數據到txt文件，1 byte 1 line
     * 
     * @param pFileName 
     * @return true 
     * @return false 
     */
    bool SaveEncodedTxtFile(const char *pFileName);

    virtual bool Clean_encode(){return false; printf("base virtual bool Clean_encode\n");}

};

/**
 * @brief 派生类，包含runlength编码方法
 * 
 */
class COsdRLObject : public COsdBaseObject
{
private:
    int m_nWidth;       //layer编码对象宽
    int m_nHeight;      //layer编码对象高

    CIdxAImage * m_pIdxAImg;      //idx+a 四通道分量layer数据，用于叠加图像，再进行编码
    
    /**
     * @brief 整理透明度分量，将0-255 聚类到 25% 50%  75%  100% 四种层级
     * 
     * @param nAlpha    透明度分量
     * @return int      聚类后的透明度
     */
    inline  int sortAlpha(int nAlpha);
   
 
    /**
     * @brief 保存编码纹理步长小于256的2 BYTE编码
     * 
     * @param nCount        步长计数
     * @param nOffset       编码保存偏移地址
     * @param SUnit         编码单元
     * @param pEncoded      编码保存首地址
     * @return true 
     * @return false 
     */
    bool EncodeSmallTexture(int& nCount , int& nOffset, RLEUNIT& SUnit, BYTE*& pEncoded);
    /**
     * @brief 保存编码纹理步长大于256的4 BYTE编码
     * 
     * @param nCount        步长计数
     * @param nOffset       编码保存偏移地址
     * @param SUnit         编码单元
     * @param pEncoded      编码保存首地址
     * @return true 
     * @return false 
     */
    bool EncodeLongTexture(int& nCount , int& nOffset, RLEUNIT& SUnit,  BYTE*& pEncoded);

    bool check_encode_len(int nLen);
    

public: 
    COsdRLObject()
    {
        m_nHeight = m_nWidth = 0;
        m_pIdxAImg = nullptr;
    }
    virtual ~COsdRLObject()
    {
        if(m_pIdxAImg != NULL) 
            delete m_pIdxAImg;   
    }

    /**
     * @brief 初始化Runlength编码对象空间，设置宽高，分配内存
     * 
     * @param nWidth 
     * @param nHeight 
     * @return true 
     * @return false 
     */
    bool InitRLObject(int nWidth, int nHeight);
    /**
     * @brief 添加bmp图像到layer编码对象 yuv+a 图层上
     * 
     * @param pFileName bmp图像文件名
     * @param nX        图像添加到layer图层上的起始位置x
     * @param nY        图像添加到layer图层上的起始位置y
     * @param nAlpha    图像叠加透明度分量
     * @return true 
     * @return false 
     */
    // bool AddBMPImage(const char *pFileName, int nX , int nY, int nAlpha);
    bool AddSsbmpImage(const char *pFileName, int nX , int nY, int nAlpha);
    /**
     * @brief 开始RunLength编码
     * 
     * @return true 
     * @return false 
     */
    bool RLEncode();   

    bool Clean_encode();
};

/**
 * @brief 派生类，包含quadRangle编码方法
 * 
 */
class COsdQRObject : public COsdBaseObject
{   
private:
    int m_nWidth;  //layer 宽
    int m_nHeight;  //layer 高
    QRE m_QRInfo[LAYER_MAX_QR_NUM] = {0}; //凸四边形信息，设置一个layer上限32
    std::vector<QRE> m_QRInfo_vec;
    int m_nQuadRangleNum;   //凸四边形个数

    // //凸四边形y轴共线范围
    // int m_Ymin = 0; 
    // int m_Ymax = 0;

    bool check_intersection(QUADRANGLE qr);

    bool check_encode_len(int nLen);

    bool qr_info_to_encode_data(QRE *pInfo, QREUNIT* pUnit);
    

public:
    COsdQRObject()
    {
        m_nWidth = 0 ;
        m_nHeight = 0;
        m_nQuadRangleNum  = 0;
    }
    virtual ~COsdQRObject(){}
    
    /**
     * @brief 初始化QuadRangle编码对象，分配空间，设置大小
     * 
     * @param nWidth 
     * @param nHeight 
     * @return true 
     * @return false 
     */
    bool InitQRObject(int nWidth, int nHeight);
    
    /**
     * @brief 添加凸四边形信息到编码对象中
     * 
     * @param nAlpha        透明度
     * @param pRGB          rgb颜色
     * @param nSolidType    实心/空心 类型
     * @param Out           外四边形信息
     * @param In            内四边形信息
     * @return true 
     * @return false 
     */
    bool AddQuadRangle( int nAlpha,  int nColorIdx, int nSolidType, QUADRANGLE Out,QUADRANGLE In);
    /**
     * @brief       开始QuadRangle编码
     * 
     * @return true 
     * @return false 
     */
    bool QREncode();
    bool Clean_encode();

    bool QRSort();
};
#endif