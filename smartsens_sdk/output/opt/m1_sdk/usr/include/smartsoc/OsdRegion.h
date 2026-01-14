#ifndef _OSD_REGION_H_
#define _OSD_REGION_H_

#include "OsdTypeDef.h"
#include "OsdLayer.h"

using namespace std;


// typedef void* RGN_HANDLE;

#define RGN_COORD_MIN_X  -32768 
#define RGN_COORD_MAX_X  32767
#define RGN_COORD_MIN_Y  -32768
#define RGN_COORD_MAX_Y  32767


/*
*   Regin区域类，区域可单独创建
*/
class COsdRegion
{
private:
    mutex m_rgn_mutex;          /* Region互斥锁，防止多线程同时进行非原子操作*/

    vector<BITMAP_INFO_S> m_bmInfo;             /* 需要添加的内存位图信息 */
    vector<COVER_ATTR_S>  m_coverInfo;          /* 添加的遮挡区域信息 */
    

    /*两个向量叉乘符号*/
    inline int vec_cross_sign( VECTOR_INT_S* pVec1,  VECTOR_INT_S* pVec2);

    /*     
    判断内四边形四个点是否在外四边形内部；
    外四边形可以是凸/凹四边形；
    外四边形坐标需要顺/逆时针排列；
     */
    bool is_in_vertex_out(VERTEXS_S* pVertex_in, VERTEXS_S* pVertex_out);

    /* 处理非法点，将凹四边形/三角形变为有两个点相同的四边形 */
    bool proc_illegal_point(VERTEXS_S* pVertex);

    /* 共线点变成平行四边形 */
    bool proc_line_to_PARA(VERTEXS_S* pVertex);

    /*四边形坐标整理为顺时针排列*/
    bool sort_clockwise_coord(VERTEXS_S* pVertex);

    /*判断四边形顶点状态，是否四点共线、是否三角形、是否凸四边形、是否凹四边形*/
    int  check_cover_type(VERTEXS_S* pVertex);

    /*检查图形区域四边形坐标属性*/
    bool check_cover_attr(COVER_ATTR_S* pCover_S);

    /* 检查图像区域添加的图像信息 */
    bool check_ssbmp_info(BITMAP_INFO_S* pInfo);

    bool create_layer(RGNTYPE eRgnType);



    bool delete_all_layer();

    bool rgn_image_encode();

    bool rgn_graphic_encode();



public:

    int  m_nLayerNum;   /* Region当前需要用于绘制的Layer数量 */
    int m_nAttached;     /* Region状态位，是否已添加至Frame */
    bool m_bEncoded;      /* Region状态位，是否已完成编码    */
    /* region attr */
    RGN_ATTR_S m_sAttr;
    COsdBaseLayer *m_pLayerObjs;      

    COsdRegion()
    {
        m_nLayerNum = 0;
        m_bEncoded = false;
        m_nAttached = 0;
        m_pLayerObjs = nullptr;
    }
    virtual ~COsdRegion()
    {
        if (m_pLayerObjs != nullptr){
            delete m_pLayerObjs;
            m_pLayerObjs = nullptr;
        }
    }
    bool Clean();

    bool Release();

    COsdBaseLayer* GetLayer(){
        return m_pLayerObjs;
    }
    
    /* 设置区域属性信息 */
    bool SetRGNAttr(RGN_ATTR_S Attr_s);

    /* 获取区域属性信息 */
    RGN_ATTR_S GetRGNAttr();
    
    /* 区域添加位图图像 */
    bool SetBitMap(BITMAP_INFO_S sBmpInfo);

    /* 区域添加图形 */
    bool SetCover(COVER_ATTR_S sCoverInfo);

    /* 区域信息已设置完毕，开始编码 */
    bool Encode();

    bool SetAttach();

    bool SetDetach();

    BYTE *GetEncodeData();

    int GetEncodeLen();

};

#endif
