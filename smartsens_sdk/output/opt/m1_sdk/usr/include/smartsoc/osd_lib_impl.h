/*
 * @FileName: osd_lib_impl.h
 * @Author: Jingwen Bai
 * @Date: 2024-07-09 14:39:55
 * @Description: 
 */


#ifndef SS_OSD_LIB_IMPL_H
#define SS_OSD_LIB_IMPL_H

#include <map>
#include <vector>
#include "osd_lib_types.h"
#include "OsdLayer.h"
#include "OsdRegion.h"

namespace fdevice{

class OsdSdkImpl{
public:
    OsdSdkImpl();
    ~OsdSdkImpl();

    class OsdLayer{
    private:
        int             m_fd;
        LAYER_HANDLE    handle;
        LAYER_ATTR_S    m_layer_attr;
        COsdRegion*     m_pRgn = nullptr;
        OsdSdkImpl      *m_context = nullptr;
        DMA_BUFFER_ATTR_S m_dma;
        int             m_swap_flag;
        bool            m_lock_status = false;
    public:
        friend class OsdSdkImpl;
        // int SetAttr();
        int SetCroodAttr();
        int SetCodeDataAttr();
        int GetAttr(LAYER_ATTR_S *pstLayer);
        int SetRLEinfo(const RLE_S *pstRLEinfo);
        int SetQuadRangle(QUADRANGLE_S *pstQuadrangle);
        int SetSramLen(unsigned char len);
        COsdRegion *GetRegionPtr();
        void Flush(int layer_id);
        void SetBuffer(DMA_BUFFER_ATTR_S dma_attr);
        int SetLock(bool lock);

        OsdLayer(OsdSdkImpl *context);
        ~OsdLayer();
    };

private:
    std::map<LAYER_HANDLE, OsdLayer> m_layer_map;
    std::map<LAYER_HANDLE, OsdLayer> m_layer_lock_map;
    int         m_osd_layer_en = 0;
    int         m_ip_layer_update;
    int         m_ip_status;
    int         m_ip_master;
    int         m_ip_reset;

    int         m_osd_fd;
    int         m_osd_status;
    int         m_osd_layer_cnt;

    unsigned char m_ip_start[32];
    unsigned char m_ip_sram_lenth[8];
    unsigned char m_ip_color_lut[120];
    unsigned char m_ip_paddr_start[32];
    unsigned char m_ip_paddr_lenth[32];
    // vector<COVER_ATTR_S>  m_coverInfo;

    std::vector<COsdRegion*> m_region_vec; 

public:
    int Open();

    int Close();

    int Init(int layer_cnt, char *p_color_lut);

    int Release();

    int GetStatus(unsigned char *status);

    int CleanLayer(LAYER_HANDLE layer_index);

    int CleanLayer();
    
    int EnableLayer(LAYER_HANDLE layer_index, bool enable);

    int CreateLayer(LAYER_HANDLE layer_index,LAYER_ATTR_S *pstLayer);

    int DestroyLayer(LAYER_HANDLE layer_index);

    int LockLayer(LAYER_HANDLE layer_index);

    int UnlockLayer(LAYER_HANDLE layer_index);

    int GetLayerAtter(LAYER_HANDLE layer_index, LAYER_ATTR_S *pstLayer);

    int SetLayerAtter(LAYER_HANDLE layer_index, LAYER_ATTR_S *pstLayer);

    int AllocDmaBuffer(void * &buffer_handle, int buf_size);

    int DeleteDmaBuffer(void * buffer_handle);

    void *GetDmaBuffer(void * buffer_handle);

    int GetDmaBufferFd(void * buffer_handle);

    int SetLayerBuffer(LAYER_HANDLE layer_index, DMA_BUFFER_ATTR_S dma);

    int AddQuadRangle(COVER_ATTR_S *attr);

    int AddQuadRangle(LAYER_HANDLE layer_index, COVER_ATTR_S *attr);

    int AddTexture(BITMAP_INFO_S *info);

    int AddTexture(LAYER_HANDLE layer_index,  BITMAP_INFO_S *info);

    int FlushQuadRandle();

    int FlushTexture();

    int FlushQuadRandle(LAYER_HANDLE layer_index);

    int FlushTexture(LAYER_HANDLE layer_index);
};

} // namespace fdevice

#endif // SS_OSD_LIB_IMPL_H