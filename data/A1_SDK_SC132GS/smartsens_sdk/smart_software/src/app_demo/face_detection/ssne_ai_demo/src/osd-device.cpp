/*
 * @Author: Jingwen Bai
 * @Date: 2024-07-04 11:07:00
 * @Description: osd device
 * @Filename: osd-device.cpp
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/osd-device.hpp"
#include "log.hpp"
using namespace fdevice;
namespace sst{
namespace device{
namespace osd{

OsdDevice::OsdDevice()
    :m_height(0),
    m_width(0){

}

OsdDevice::~OsdDevice(){
    std::cout << "OsdDevice Destructor" << std::endl;
}

void OsdDevice::Initialize(int width, int height, const char* bitmap_lut_path){
    m_width = width;
    m_height = height;

    // load osd color lut
    // 如果提供了位图LUT路径，优先使用位图LUT；否则使用默认LUT
    if (bitmap_lut_path != nullptr && strlen(bitmap_lut_path) > 0) {
        if (LoadLutFile(bitmap_lut_path) == 0) {
            std::cout << "[OsdDevice] Using bitmap LUT: " << bitmap_lut_path << std::endl;
        } else {
            // 如果位图LUT加载失败，回退到默认LUT
            std::cerr << "[OsdDevice] Warning: Failed to load bitmap LUT, using default LUT" << std::endl;
            LoadLutFile(m_osd_lut_path.c_str());
        }
    } else {
        LoadLutFile(m_osd_lut_path.c_str());
    }

    // open osd device
    m_osd_handle = osd_open_device();
    // init osd (必须在创建图层前调用)
    osd_init_device(m_osd_handle, OSD_LAYER_SIZE, (char*)m_pcolor_lut);

    // init quad-rangle layer (TYPE_GRAPHIC) for layers 0-1
    int dma_size = 1024;
    for(int layer_index = 0; layer_index < 2; layer_index++){
        osd_alloc_buffer(m_osd_handle, m_layer_dma[layer_index].dma, dma_size);sleep(0.25);
        osd_alloc_buffer(m_osd_handle, m_layer_dma[layer_index].dma_2, dma_size);
        int dma_fd = osd_get_buffer_fd(m_osd_handle, m_layer_dma[layer_index].dma);

        LAYER_ATTR_S osd_layer;
        osd_layer.codeTYPE = SS_TYPE_QUADRANGLE;
        osd_layer.layer_data_QR.osd_buf.buf_type = BUFFER_TYPE_DMABUF;
        osd_layer.layer_data_QR.osd_buf.buf.fd_dmabuf = dma_fd;
        osd_layer.layerStart.layer_start_x = 0;
        osd_layer.layerStart.layer_start_y = 0;
        osd_layer.layerSize.layer_width = m_width;
        osd_layer.layerSize.layer_height = m_height;
        osd_layer.layer_rgn = {TYPE_GRAPHIC, {m_width, m_height}};
        osd_create_layer(m_osd_handle, (ssLAYER_HANDLE)layer_index, &osd_layer);
        osd_set_layer_buffer(m_osd_handle, (ssLAYER_HANDLE)layer_index, m_layer_dma[layer_index]);
    }

    // init image layer (TYPE_IMAGE) for layer 2 (bitmap)
    {
        int layer_index = 2;
        // DMA size for texture layer: 1920*1080 bytes (full HD resolution)
        int texture_dma_size = 0x20000;
        osd_alloc_buffer(m_osd_handle, m_layer_dma[layer_index].dma, texture_dma_size);
        sleep(0.25);  // 等待DMA分配完成
        osd_alloc_buffer(m_osd_handle, m_layer_dma[layer_index].dma_2, texture_dma_size);
        int dma_fd = osd_get_buffer_fd(m_osd_handle, m_layer_dma[layer_index].dma);

        LAYER_ATTR_S osd_layer;
        osd_layer.codeTYPE = SS_TYPE_RLE;
        osd_layer.layer_data_RLE.osd_buf.buf_type = BUFFER_TYPE_DMABUF;
        osd_layer.layer_data_RLE.osd_buf.buf.fd_dmabuf = dma_fd;
        osd_layer.layerStart.layer_start_x = 0;
        osd_layer.layerStart.layer_start_y = 0;
        osd_layer.layerSize.layer_width = m_width;
        osd_layer.layerSize.layer_height = m_height;
        osd_layer.layer_rgn = {TYPE_IMAGE, {m_width, m_height}};

        std::cout << "[OsdDevice] Creating layer " << layer_index << " with TYPE_IMAGE" << std::endl;
        std::cout << "[OsdDevice] Layer region type: " << (int)osd_layer.layer_rgn.enType
                  << " (0=TYPE_IMAGE, 1=TYPE_GRAPHIC)" << std::endl;
        std::cout << "[OsdDevice] Layer region size: " << osd_layer.layer_rgn.size_s.w
                  << "x" << osd_layer.layer_rgn.size_s.h << std::endl;

        int ret = osd_create_layer(m_osd_handle, (ssLAYER_HANDLE)layer_index, &osd_layer);
        if (ret != 0) {
            std::cerr << "[OsdDevice] ERROR: osd_create_layer failed! ret=" << ret
                      << ", layer_index=" << layer_index << std::endl;
        } else {
            std::cout << "[OsdDevice] Layer " << layer_index << " created successfully" << std::endl;
        }

        ret = osd_set_layer_buffer(m_osd_handle, (ssLAYER_HANDLE)layer_index, m_layer_dma[layer_index]);
        if (ret != 0) {
            std::cerr << "[OsdDevice] ERROR: osd_set_layer_buffer failed! ret=" << ret
                      << ", layer_index=" << layer_index << std::endl;
        } else {
            std::cout << "[OsdDevice] Layer " << layer_index << " buffer set successfully" << std::endl;
        }
    }

    // 图层0-1用于quad-rangle，图层2用于位图
    // 图层3-4未使用，已删除以节省内存

    // // init run-length layer
    // {
    //     // run-length 最后一层画标定线
    //     int layer_index = OSD_LAYER_SIZE - 1;
    //     osd_alloc_buffer(m_osd_handle, m_layer_dma[layer_index].dma, 0x20000);
    //     int dma_fd = osd_get_buffer_fd(m_osd_handle, m_layer_dma[layer_index].dma);

    //     LAYER_ATTR_S osd_layer;
    //     osd_layer.codeTYPE = SS_TYPE_RLE;
    //     osd_layer.layer_data_RLE.osd_buf.buf_type = BUFFER_TYPE_DMABUF;
    //     osd_layer.layer_data_RLE.osd_buf.buf.fd_dmabuf = dma_fd;
    //     osd_layer.layerStart.layer_start_x = 0;
    //     osd_layer.layerStart.layer_start_y = 0;
    //     osd_layer.layerSize.layer_width = m_width;
    //     osd_layer.layerSize.layer_height = m_height;
    //     osd_layer.layer_rgn = {TYPE_IMAGE, {m_width, m_height}};
    //     osd_create_layer(m_osd_handle, (ssLAYER_HANDLE)layer_index, &osd_layer);
    //     osd_set_layer_buffer(m_osd_handle, (ssLAYER_HANDLE)layer_index, m_layer_dma[layer_index]);
    // }

    // // draw image use run-length
    // DrawTexture(m_texture_path.c_str(), OSD_LAYER_SIZE - 1);
}


void OsdDevice::Release(){
    std::cout << "OsdDevice Release" << std::endl;

    // destroy layer and delete dma buf
    for(int i = 0; i < OSD_LAYER_SIZE; i++){
        osd_destroy_layer(m_osd_handle, (ssLAYER_HANDLE)i);

        if(m_layer_dma[i].dma != nullptr)
            osd_delete_buffer(m_osd_handle, m_layer_dma[i].dma);
        if(m_layer_dma[i].dma_2 != nullptr)
            osd_delete_buffer(m_osd_handle, m_layer_dma[i].dma_2);
    }

    if(m_pcolor_lut != nullptr){
        delete m_pcolor_lut;
        m_pcolor_lut = nullptr;
    }

    osd_close_device(m_osd_handle);
}



int OsdDevice::LoadLutFile(const char* filename){
    std::cout << "[OsdDevice] Attempting to load LUT file: " << filename << std::endl;

    // 检查文件是否存在
    struct stat file_stat;
    if (stat(filename, &file_stat) != 0) {
        std::cerr << "[OsdDevice] ERROR: File does not exist or cannot access: " << filename << std::endl;
        std::cerr << "[OsdDevice] Error code: " << errno << " (" << strerror(errno) << ")" << std::endl;
        return -1;
    }

    // 检查文件大小
    if (file_stat.st_size <= 0) {
        std::cerr << "[OsdDevice] ERROR: Invalid file size: " << file_stat.st_size << " bytes" << std::endl;
        return -1;
    }

    std::cout << "[OsdDevice] File exists, size: " << file_stat.st_size << " bytes" << std::endl;

    // 检查文件权限
    if (access(filename, R_OK) != 0) {
        std::cerr << "[OsdDevice] ERROR: No read permission for file: " << filename << std::endl;
        std::cerr << "[OsdDevice] Error code: " << errno << " (" << strerror(errno) << ")" << std::endl;
        return -1;
    }

    // 打开文件
    std::ifstream file(filename, std::ios::binary | std::ios::in | std::ios::ate);
    if (!file) {
        std::cerr << "[OsdDevice] ERROR: Cannot open file: " << filename << std::endl;
        std::cerr << "[OsdDevice] Error code: " << errno << " (" << strerror(errno) << ")" << std::endl;
        return -1;
    }

    // 获取文件大小
    m_file_size = file.tellg();
    if (m_file_size <= 0) {
        std::cerr << "[OsdDevice] ERROR: Invalid file size from stream: " << m_file_size << " bytes" << std::endl;
        file.close();
        return -1;
    }

    // 分配内存并读取文件
    m_pcolor_lut = new uint8_t[m_file_size];
    file.seekg(0, std::ios::beg);
    file.read((char*)m_pcolor_lut, m_file_size);

    // 检查是否读取成功
    if (file.gcount() != m_file_size) {
        std::cerr << "[OsdDevice] ERROR: Failed to read complete file. Expected: " << m_file_size
                  << " bytes, Read: " << file.gcount() << " bytes" << std::endl;
        delete[] m_pcolor_lut;
        m_pcolor_lut = nullptr;
        file.close();
        return -1;
    }

    file.close();
    std::cout << "[OsdDevice] Successfully loaded LUT file: " << filename
              << ", size: " << m_file_size << " bytes" << std::endl;
    return 0;
}

// draw mode: auto alloc layer
void OsdDevice::Draw(std::vector<OsdQuadRangle> &quad_rangle){
    if ((quad_rangle.size() == 0)){
        osd_clean_all_layer(m_osd_handle);
        return;
    }

    // generate qrangle box
    for(auto &q : quad_rangle){
        GenQrangleBox(q.box, q.border);
        COVER_ATTR_S qrangle_attr = {q.color, q.type, q.alpha, m_qrangle_out, m_qrangle_in};
        osd_add_quad_rangle(m_osd_handle, &qrangle_attr);
    }

    // flush data to ddr
    osd_flush_quad_rangle(m_osd_handle);
}

// draw mode: manual alloc layer
void OsdDevice::Draw(std::vector<OsdQuadRangle> &quad_rangle, int layer_id){
    if ((quad_rangle.size() == 0)){
        osd_clean_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id);
        LOG_DEBUG("Draw --- osd_clean_layer\n");
        return;
    }
    int ret = 0;

    // generate qrangle box
    for(auto &q : quad_rangle){
        LOG_DEBUG("Draw --- q.box: %f, %f, %f, %f\n", q.box[0], q.box[1], q.box[2], q.box[3]);
        GenQrangleBox(q.box, q.border);
        COVER_ATTR_S qrangle_attr = {q.color, q.type, q.alpha, m_qrangle_out, m_qrangle_in};
        ret = osd_add_quad_rangle_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id, &qrangle_attr);
        LOG_DEBUG("Draw --- osd_add_quad_rangle_layer ret: %d\n", ret);
    }

    // flush data to ddr
    osd_flush_quad_rangle_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id);
}

// draw mode: manual alloc layer
void OsdDevice::Draw(std::vector<std::array<float, 4>>& boxes, int border, int layer_id, tagQUADRANGLETYPE type, tagALPHATYPE alpha, int color){
    if ((boxes.size() == 0)){
        osd_clean_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id);
        return;
    }

    int ret = 0;
    // generate qrangle box
    for (auto &box : boxes){
        GenQrangleBox(box, border);
        COVER_ATTR_S qrangle_attr = {color, type, alpha, m_qrangle_out, m_qrangle_in};
        ret = osd_add_quad_rangle_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id, &qrangle_attr);
    }

    // flush data to ddr
    osd_flush_quad_rangle_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id);
}


/**
 * @brief 绘制位图到指定图层
 * @note LUT应该在初始化时加载，osd_init_device必须在创建图层前调用
 *       如果在绘制时重新初始化，会破坏已创建的图层
 */
void OsdDevice::DrawTexture(const char* bitmap_path, const char* lut_path, int layer_id, int pos_x, int pos_y, fdevice::ALPHATYPE alpha) {
    // 注意：LUT已经在Initialize时加载，这里不再重新初始化设备
    // lut_path参数保留用于日志记录，但不会重新加载LUT
    // 位图坐标系统：以图层左上角为原点（绝对坐标系统）
    // pos_x和pos_y是绝对坐标（左上角为原点，Y向下为正）

    // 创建位图信息结构
    fdevice::BITMAP_INFO_S bm_info;
    bm_info.pSSbmpFile = bitmap_path;
    bm_info.alpha = fdevice::TYPE_ALPHA100;
    bm_info.position.x = pos_x;  // 绝对坐标，以左上角为原点
    bm_info.position.y = pos_y;  // 绝对坐标，以左上角为原点，Y向下为正
    //osd_clean_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id); //zlj+
    LOG_DEBUG("[OsdDevice] Drawing texture: %s", bitmap_path);
    LOG_DEBUG(" at absolute position %d,%d", pos_x, pos_y);
    LOG_DEBUG(" layer_id=%d\n", layer_id);
    LOG_DEBUG("[OsdDevice] Bitmap file path: %s\n", bitmap_path ? bitmap_path : "NULL");
    LOG_DEBUG("[OsdDevice] Bitmap position: %d,%d\n", bm_info.position.x, bm_info.position.y);
    LOG_DEBUG("[OsdDevice] Bitmap alpha: %d\n", (int)bm_info.alpha);
    // 添加位图到指定图层
    //std::cout << "[OsdDevice] Calling osd_add_texture_layer for layer_id=" << layer_id << std::endl;
    int ret = osd_add_texture_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id, &bm_info);
    if (ret != 0) {
        std::cerr << "[OsdDevice] ERROR: osd_add_texture_layer failed! ret=" << ret
                  << ", layer_id=" << layer_id << std::endl;
        if (ret == -1) {
            std::cerr << "[OsdDevice] Layer does not exist or type mismatch (should be TYPE_IMAGE)" << std::endl;
            std::cerr << "[OsdDevice] Possible causes:" << std::endl;
            std::cerr << "[OsdDevice]   1. Layer " << layer_id << " was not created" << std::endl;
            std::cerr << "[OsdDevice]   2. Layer " << layer_id << " region type is not TYPE_IMAGE" << std::endl;
            std::cerr << "[OsdDevice]   3. Layer " << layer_id << " region object (m_pRgn) is nullptr" << std::endl;
        } else if (ret == -2) {
            std::cerr << "[OsdDevice] Bitmap add failed (encoding data too large or invalid file)" << std::endl;
        }
        return;
    }
    LOG_DEBUG("[OsdDevice] osd_add_texture_layer succeeded\n");
    // 刷新位图数据到设备
    //std::cout << "[OsdDevice] Before flush: checking layer " << layer_id << " status" << std::endl;
    //std::cout << "[OsdDevice] Calling osd_flush_texture_layer for layer_id=" << layer_id << std::endl;
    ret = osd_flush_texture_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id);
    if (ret != 0) {
        std::cerr << "[OsdDevice] ERROR: osd_flush_texture_layer failed! ret=" << ret
                  << ", layer_id=" << layer_id << std::endl;
        std::cerr << "[OsdDevice] Possible causes:" << std::endl;
        std::cerr << "[OsdDevice]   1. Layer " << layer_id << " codeTYPE is not SS_TYPE_RLE" << std::endl;
        std::cerr << "[OsdDevice]   2. Layer " << layer_id << " region type mismatch" << std::endl;
        std::cerr << "[OsdDevice]   3. Layer " << layer_id << " DMA buffer not set correctly" << std::endl;
        std::cerr << "[OsdDevice]   4. Layer " << layer_id << " region object encoding failed" << std::endl;
        std::cerr << "[OsdDevice]   5. Layer " << layer_id << " not enabled" << std::endl;
    } else {
        LOG_DEBUG("[OsdDevice] Texture drawn successfully\n");
    }
}

void OsdDevice::GenQrangleBox(std::array<float, 4>& det, int border){
    std::array<int, 16> box;

    box[0] = std::min(m_width, std::max(0, int(det[0]+border)));
    box[1] = std::min(m_height, std::max(0, int(det[1]+border)));
    box[2] = std::min(m_width, std::max(0, int(det[0]+border)));
    box[3] = std::min(m_height, std::max(0, int(det[3]-border)));
    box[4] = std::min(m_width, std::max(0, int(det[2]-border)));
    box[5] = std::min(m_height, std::max(0, int(det[3]-border)));
    box[6] = std::min(m_width, std::max(0, int(det[2]-border)));
    box[7] = std::min(m_height, std::max(0, int(det[1]+border)));

    box[8] = std::min(m_width, std::max(0, int(det[0]-border)));
    box[9] = std::min(m_height, std::max(0, int(det[1]-border)));
    box[10] = std::min(m_width, std::max(0, int(det[0]-border)));
    box[11] = std::min(m_height, std::max(0, int(det[3]+border)));
    box[12] = std::min(m_width, std::max(0, int(det[2]+border)));
    box[13] = std::min(m_height, std::max(0, int(det[3]+border)));
    box[14] = std::min(m_width, std::max(0, int(det[2]+border)));
    box[15] = std::min(m_height, std::max(0, int(det[1]-border)));

    m_qrangle_in.points[0]={box[0], box[1]};
    m_qrangle_in.points[1]={box[2], box[3]};
    m_qrangle_in.points[2]={box[4], box[5]};
    m_qrangle_in.points[3]={box[6], box[7]};
    m_qrangle_out.points[0] = {box[8], box[9]};
    m_qrangle_out.points[1] = {box[10], box[11]};
    m_qrangle_out.points[2] = {box[12], box[13]};
    m_qrangle_out.points[3] = {box[14], box[15]};
}

} // namespace osd
} // namespace device
} // namespace sst
