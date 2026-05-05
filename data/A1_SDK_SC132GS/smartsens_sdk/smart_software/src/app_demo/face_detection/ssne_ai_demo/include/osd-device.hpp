/**
 * osd-device.hpp — OSD 屏幕叠加层设备接口
 *
 * 5 层 OSD 布局：
 *   Layer 0：四边形图形层（TYPE_GRAPHIC）— 检测框
 *   Layer 1：四边形图形层（TYPE_GRAPHIC）— 固定正方形
 *   Layer 2：位图图层（TYPE_IMAGE）— 背景位图（透明开窗）
 *   Layer 3：位图图层（TYPE_IMAGE）— 分类结果贴图
 *   Layer 4：位图图层（TYPE_IMAGE）— 预留
 *
 * 图层 0-1：SS_TYPE_QUADRANGLE，支持四边形绘制（检测框等）
 * 图层 2-4：SS_TYPE_RLE，支持 .ssbmp 位图纹理叠加
 */

#ifndef SST_OSD_DEVICE_HPP_
#define SST_OSD_DEVICE_HPP_

#include <vector>
#include <string>

#include "osd_lib_api.h"
#include "common.hpp"

#define BUFFER_TYPE_DMABUF  0x1
#define OSD_LAYER_SIZE 5  // 使用5个图层：0(检测框), 1(固定正方形), 2(背景位图), 3(游戏位图), 4(win位图)

namespace sst{
namespace device{
namespace osd{

typedef struct {
    std::array<float, 4> box;
    int border;
    int layer_id;
    fdevice::QUADRANGLETYPE type;
    fdevice::ALPHATYPE alpha;
    int color;
}OsdQuadRangle;

class OsdDevice {
public:
    OsdDevice();
    ~OsdDevice();

    bool Initialize(int width, int height, const char* bitmap_lut_path = nullptr);
    void Release();

    void Draw(std::vector<OsdQuadRangle> &quad_rangle);
    void Draw(std::vector<std::array<float, 4>>& boxes, int border, int layer_id, fdevice::QUADRANGLETYPE type, fdevice::ALPHATYPE alpha, int color);
    void Draw(std::vector<OsdQuadRangle> &quad_rangle, int layer_id);
    /**
     * @brief 绘制位图到指定图层
     * @param bitmap_path 位图文件路径（.ssbmp格式）
     * @param lut_path LUT文件路径（.sscl格式），如果为空则使用默认LUT
     * @param layer_id 图层ID
     * @param pos_x 位图左上角X坐标（相对于画面，0为左上角）
     * @param pos_y 位图左上角Y坐标（相对于画面，0为左上角）
     * @param alpha 透明度
     * @description 在位图图层上绘制位图，位置在整个图像上
     */
    void DrawTexture(const char* bitmap_path, const char* lut_path, int layer_id, int pos_x = 0, int pos_y = 0, fdevice::ALPHATYPE alpha = fdevice::TYPE_ALPHA100);

private:
    int LoadLutFile(const char* filename);
    void GenQrangleBox(std::array<float, 4>& det, int border);

private:
    handle_t m_osd_handle;
    std::string m_osd_lut_path = "/app_demo/app_assets/background_colorLUT.sscl";
    // std::string m_texture_path = "/ai/imgs/test_24.ssbmp";
    uint8_t *m_pcolor_lut = nullptr;
    int m_file_size = 0;
    int m_height, m_width;
    
    fdevice::DMA_BUFFER_ATTR_S m_layer_dma[OSD_LAYER_SIZE];
    fdevice::VERTEXS_S m_qrangle_out={0}, m_qrangle_in={0};
};

} // namespace osd
} // namespace device
} // namespace sst

#endif // SST_OSD_DEVICE_HPP_
