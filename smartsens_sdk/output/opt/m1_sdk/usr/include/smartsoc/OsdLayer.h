#ifndef _OSD_Layer_H_
#define _OSD_Layer_H_

#include "OsdTypeDef.h"
#include "OsdObject.h"

/**
 * @brief layer基类
 * 
 */
class COsdBaseLayer
{
public:
    COsdBaseObject *m_pBaseObj;  //编码对象
    
    // int m_nPriority;    //layer优先级（序号）
    int m_nLayerType;   //layer编码类型
    // char m_cName[NAMESTRLEN]={0};   //layer name
    // int m_nStartX;  //layer起始坐标x
    // int m_nStartY;  //layer起始坐标y

    COsdBaseLayer()
    {
        m_pBaseObj = NULL;
        // m_nPriority = 0 ;
        m_nLayerType = -1;
        // m_nStartX = m_nStartY = 0;
    }
    virtual ~COsdBaseLayer()
    {
        if (m_pBaseObj != NULL)
            delete m_pBaseObj;
    } 

    /**
     * @brief 创建编码对象
     * 
     * @param nWidth 
     * @param nHeight 
     * @return true 
     * @return false 
     */
    virtual bool CreateBaseObj(int nWidth, int nHeight){return false;}
    /**
     * @brief 添加图像到runlength编码对象中，并设置透明度
     * 
     * @param pFileName     图像路径
     * @param nX            图像在layer图层中的起始坐标x            
     * @param nY            图像在layer图层中的起始坐标y
     * @param nAlpha        图像透明度
     * @return true 
     * @return false 
     */
    virtual bool AddEncodeObj(const char *pFileName, int nX , int nY, int nAlpha){return false;}
    /**
     * @brief 添加凸四边形到QuadRangle编码对象中
     * 
     * @param nAlpha        透明度
     * @param pRGB          rgb颜色
     * @param nSolidType    实心/空心
     * @param pOut          外四边形顶点
     * @param pIn           内四边形顶点
     * @return true 
     * @return false 
     */
    virtual bool AddEncodeObj(int nAlpha, int nColorIdx, int nSolidType, QUADRANGLE *pOut,QUADRANGLE *pIn){return false;}
    /**
     * @brief 对编码对象进行编码
     * 
     * @return true 
     * @return false 
     */
    virtual bool ObjEncode(){return false;}

    virtual bool Clean(){return false;}

    bool SaveObjToBinary(int layerIdx);
};

/**
 * @brief RunLength编码的layer派生类
 * 
 */
class COsdRLLayer: public COsdBaseLayer
{
public:

    COsdRLLayer(){ m_nLayerType = TYPE_RUNLENGTH; }
    virtual ~COsdRLLayer(){}

    bool CreateBaseObj(int nWidth, int nHeight); 
    bool AddEncodeObj(const char *pFileName, int nX , int nY, int nAlpha);
    bool ObjEncode();
    bool Clean();
};

/**
 * @brief  QuadRangle编码的layer派生类
 * 
 */
class COsdQRLayer: public COsdBaseLayer
{
public:

    COsdQRLayer(){m_nLayerType = TYPE_QUADRANGLE;}
    virtual ~COsdQRLayer(){}
    
     bool CreateBaseObj(int nWidth, int nHeight); 
     bool AddEncodeObj(int nAlpha, int nColorIdx, int nSolidType, QUADRANGLE *pOut,QUADRANGLE *pIn);
     bool ObjEncode();
     bool Clean();
};

#endif