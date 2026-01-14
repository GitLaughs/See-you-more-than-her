/*
 * @Filename: osd_lib_api.h
 * @Author: Jingwen Bai
 * @Date: 2024-07-08 17:33:13
 * @Description: osd lib api 
 */

#ifndef SS_OSD_LIB_API_H
#define SS_OSD_LIB_API_H

#include "osd_lib_types.h"

#define EXPORT_API __attribute__((visibility("default")))


namespace fdevice{

#ifdef __cplusplus
extern "C"{
#endif


EXPORT_API const char *osd_get_lib_version(void);

EXPORT_API handle_t osd_open_device();

EXPORT_API int osd_close_device(handle_t handle);

EXPORT_API int osd_init_device(handle_t handle, int layer_cnt, char *pFileColorLUT);

// layer api
EXPORT_API int osd_create_layer(handle_t handle, LAYER_HANDLE layer_index, LAYER_ATTR_S *pstLayer);

EXPORT_API int osd_destroy_layer(handle_t handle, LAYER_HANDLE layer_index);

EXPORT_API int osd_enable_layer(handle_t handle, LAYER_HANDLE layer_index, bool enbale);

EXPORT_API int osd_lock_layer(handle_t handle, LAYER_HANDLE layer_index, bool lock);

EXPORT_API int osd_clean_layer(handle_t handle, LAYER_HANDLE layer_index);

EXPORT_API int osd_clean_all_layer(handle_t handle);

EXPORT_API int osd_get_status(handle_t handle, unsigned char *status);

// osd dma buffer api
EXPORT_API int osd_alloc_buffer(handle_t handle, void * &buffer_handle, int buf_size);

EXPORT_API int osd_delete_buffer(handle_t handle, void * buffer_handle);

EXPORT_API void *osd_get_buffer_ptr(handle_t handle, void * buffer_handle);

EXPORT_API int osd_get_buffer_fd(handle_t handle, void * buffer_handle);

EXPORT_API int osd_set_layer_buffer(handle_t handle, LAYER_HANDLE layer_index, DMA_BUFFER_ATTR_S dma);


// add/flush q-rangle texture
EXPORT_API int osd_add_quad_rangle(handle_t handle, COVER_ATTR_S *attr);

EXPORT_API int osd_flush_quad_rangle(handle_t handle);

EXPORT_API int osd_add_texture(handle_t handle, BITMAP_INFO_S *info);

EXPORT_API int osd_flush_texture(handle_t handle);

// add/flush q-rangle texture 
EXPORT_API int osd_add_quad_rangle_layer(handle_t handle, LAYER_HANDLE layer_index, COVER_ATTR_S *attr);

EXPORT_API int osd_flush_quad_rangle_layer(handle_t handle, LAYER_HANDLE layer_index);

EXPORT_API int osd_add_texture_layer(handle_t handle, LAYER_HANDLE layer_index, BITMAP_INFO_S *info);

EXPORT_API int osd_flush_texture_layer(handle_t handle, LAYER_HANDLE layer_index);


#ifdef __cplusplus
}
#endif

} // namespace fdevice


#endif // SS_OSD_LIB_API_H