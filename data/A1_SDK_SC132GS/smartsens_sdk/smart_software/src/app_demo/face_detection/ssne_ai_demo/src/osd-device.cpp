/*
 * @Author: Jingwen Bai
 * @Date: 2024-07-04 11:07:00
 * @Description: osd device
 * @Filename: osd-device.cpp
 */

#include <iostream>
#include <fstream>
#include <cstring>

#include "../include/osd-device.hpp"

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

void OsdDevice::Initialize(int width, int height){
    m_width = width;
    m_height = height;

    // load osd color lut
    LoadLutFile(m_osd_lut_path.c_str());

    // open osd device 
    m_osd_handle = osd_open_device();
    // init osd 
    osd_init_device(m_osd_handle, OSD_LAYER_SIZE, (char*)m_pcolor_lut);

    // init quad-rangle layer
    int dma_size = 1024;
    for(int layer_index = 0; layer_index < OSD_LAYER_SIZE; layer_index++){
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
    std::ifstream file(filename, std::ios::binary | std::ios::in | std::ios::ate);
    if (!file) {
        std::cerr << "无法打开文件 " <<filename<< std::endl;
        return -1;
    }
    m_file_size = file.tellg();
    m_pcolor_lut = new uint8_t[m_file_size];
    file.seekg(0, std::ios::beg);
    file.read((char*)m_pcolor_lut, m_file_size);
    file.close();

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
        printf("Draw --- osd_clean_layer\n");
        return;
    }
    int ret = 0;

    // generate qrangle box
    for(auto &q : quad_rangle){
        printf("Draw --- q.box: %f, %f, %f, %f\n", q.box[0], q.box[1], q.box[2], q.box[3]);
        GenQrangleBox(q.box, q.border);
        COVER_ATTR_S qrangle_attr = {q.color, q.type, q.alpha, m_qrangle_out, m_qrangle_in};
        ret = osd_add_quad_rangle_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id, &qrangle_attr);   
        printf("Draw --- osd_add_quad_rangle_layer ret: %d\n", ret);
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


// void OsdDevice::DrawTexture(const char* filename, int layer_id) {
//     BITMAP_INFO_S bm_info = {filename, TYPE_ALPHA100, {0, 0}};

//     osd_add_texture(m_osd_handle, &bm_info);

//     osd_flush_texture(m_osd_handle);

//     osd_lock_layer(m_osd_handle, (ssLAYER_HANDLE)layer_id, true);
// }

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
