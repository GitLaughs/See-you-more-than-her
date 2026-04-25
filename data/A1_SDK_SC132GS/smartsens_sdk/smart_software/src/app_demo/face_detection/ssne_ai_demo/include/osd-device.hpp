/*
 * @Author: Jingwen Bai
 * @Date: 2024-07-04 11:07:00
 * @Description:
 * @Filename: osd-device.hpp
 */
#ifndef SST_OSD_DEVICE_HPP_
#define SST_OSD_DEVICE_HPP_

#include <vector>
#include <string>

#include "osd_lib_api.h"
#include "common.hpp"

#define BUFFER_TYPE_DMABUF  0x1
#define OSD_LAYER_SIZE 3  // 只使用3个图层：0(检测框), 1(固定正方形), 2(位图)

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

    void Initialize(int width, int height, const char* bitmap_lut_path = nullptr);
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
    std::string m_osd_lut_path = "/app_demo/app_assets/colorLUT.sscl";
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
