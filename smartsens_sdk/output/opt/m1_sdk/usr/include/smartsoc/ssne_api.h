// FileName: ssne_api.h
// DateTime: 2022/07/11 19:33:18
// Author: Phil He
// Email: jin.he@smartsenstech.com
// Copyright (c) 2022 SmartSens

/*****************************************************************
 *      SMARTSENS Neural Engine (SSNE)
 *  SmartSens SoC runtime for AI task
 *****************************************************************/

#ifndef _SSNE_API_H_
#define _SSNE_API_H_

#include <stdint.h>
#include <stddef.h>
#include <vector>
#ifdef __cplusplus
extern "C"{
#endif

#define SSNE_ERRCODE_NO_ERROR       0
#define SSNE_ERRCODE_JOB_ERROR      100
#define SSNE_ERRCODE_FILE_ERROR     200
#define SSNE_ERRCODE_MODEL_FILE_ERROR      201
#define SSNE_ERRCODE_DEVICE_ERROR   300
#define SSNE_ERRCODE_INPUT_ERROR    400
#define SSNE_ERRCODE_INPUT_NUM_ERROR       401
#define SSNE_ERRCODE_INPUT_SIZE_ERROR      402
#define SSNE_ERRCODE_INPUT_DTYPE_ERROR     403
#define SSNE_ERRCODE_INPUT_FORMAT_ERROR    404
#define SSNE_ERRCODE_INPUT_BUFFER_ERROR    405
#define SSNE_ERRCODE_OUTPUT_ERROR   500
#define SSNE_ERRCODE_SPACE_ERROR    600

#define SSNE_UINT8      0
#define SSNE_INT8       1
#define SSNE_FLOAT32    2

#define SSNE_BYTES      0
#define SSNE_YUV422_20  1
#define SSNE_YUV422_16  2
#define SSNE_Y_10       3
#define SSNE_Y_8        4
#define SSNE_RGB        5
#define SSNE_BGR        6

#define SSNE_STATIC_ALLOC   0
#define SSNE_DYNAMIC_ALLOC  1

/**
 * @brief SSNE buffer type to locate source
 */
enum ssne_buffer_type
{
    SSNE_BUF_LINUX = 0,
    SSNE_BUF_AI = 1,
    SSNE_BUF_SRAM = 2,
    SSNE_BUF_LNPU = 3
};

typedef enum
{
    kPipeline0,
    kPipeline1
}PipelineIdType;

typedef enum
{
    kSensor0,
    kSensor1
}SensorIdType;

typedef enum
{
    kDownSample1x = 1,
    kDownSample2x = 2,
    kDownSample4x = 4,
} BinningRatioType;

/****************************************/
/*********     ssne_tensor_t      *******/
/****************************************/
/**
 * @brief SSNE wrapper for ssne tensor
 * @param data tensor pointer
 */
typedef struct ssne_tensor
{
    void *data;
} ssne_tensor_t;

/**
 * @brief initial SSNE
 * @return initial status
 */
int ssne_initial();

/**
 * @brief release SSNE
 * @return release status
 */
int ssne_release();

/**
 * @brief load SSModel for inference
 * @param path SSModel file path
 * @param load_flag SSModel loading flag SSNE_STATIC_ALLOC/SSNE_DYNAMIC_ALLOC
 * @return SSModel id in SSNE
 */
uint16_t ssne_loadmodel(char *path, uint8_t load_flag);

/**
 * @brief get SSModel
 * @param model_id SSModel id in SSNE from ssne_loadmodel()
 * @param input_num number of ssne_tensor_t object
 * @param input_tensor multiple ssne_tensor_t object
 * @return  status
 */
int ssne_get_model_normalize_params(uint16_t model_id, int* mean, int* std, int* is_uint8);

/**
 * @brief get SSModel
 * @param model_id SSModel id in SSNE from ssne_loadmodel()
 * @return number of input tensors
 */
 int ssne_get_model_input_num(uint16_t model_id);

/**
 * @brief get SSModel
 * @param model_id SSModel id in SSNE from ssne_loadmodel()
 * @param dtype input data type
 * @return status
 */
 int ssne_get_model_input_dtype(uint16_t model_id, int* dtype);


/**
 * @brief inference SSModel
 * @param model_id SSModel id in SSNE from ssne_loadmodel()
 * @param input_num number of ssne_tensor_t object
 * @param input_tensor multiple ssne_tensor_t object
 * @return inference status
 */
int ssne_inference(uint16_t model_id, uint8_t input_num, ssne_tensor_t input_tensor[]);

/**
 * @brief get SSModel inference result
 * @param model_id SSModel id in SSNE from ssne_loadmodel()
 * @param output_num number of ssne_tensor_t object
 * @param output_tensor multiple ssne_tensor_t object
 * @return status
 */

int ssne_getoutput(uint16_t model_id, uint8_t output_num, ssne_tensor_t output_tensor[]);

/**
 * @brief change model location to SRAM
 * @param model_id SSModel id in SSNE from ssne_loadmodel()
 * @return move status
 */
int ssne_movemodeltosram(uint16_t model_id);


/**
 * @brief create_tensor  with width height and format
 * @param width  tensor width
 * @param height tensor height
 * @param format tensor format
 * @return ssne_tensor_t
 */
ssne_tensor_t create_tensor(uint32_t width, uint32_t height, uint8_t format, ssne_buffer_type buffer_location);

/**
 * @brief create_tensor  with width height and format
 * @param filepath  filename
 * @return ssne_tensor_t
 */
ssne_tensor_t create_tensor_from_file(const char* filepath, ssne_buffer_type buffer_location = SSNE_BUF_AI);

/**
 * @brief release tensor
 * @param tensor  tensor need release

 * @return status
 */
int release_tensor(ssne_tensor_t tensor);

/**
 * @brief load tensor from file
 * @param tensor  tensor
 * @param filepath  filename
 * @return status
 */
int load_tensor_buffer(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief load tensor from pointer
 * @param tensor  tensor
 * @param data  data pointer
 * @param mem_size  the memory size to load
 * @return status
 */
int load_tensor_buffer_ptr(ssne_tensor_t tensor, void* data, int mem_size);

/**
 * @brief save tensor to file

 * @param filepath  filename
 * @return status
 */

int save_tensor(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief save tensor buffer to file
 * @param tensor tensor
 * @param filepath filename
 * @return status
 */
int save_tensor_buffer(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief save tensor buffer to pointer
 * @param tensor tensor
 * @param data  data pointer
 * @param mem_size  the memory size to save
 * @return status
 */
int save_tensor_buffer_ptr(ssne_tensor_t tensor, void* data, int mem_size);

/**
 * @brief get tensor total size
 * @param tensor tensor
 * @return size
 */

uint32_t get_total_size(ssne_tensor_t tensor);

/**
 * @brief get tensor memory size
 * @param tensor tensor
 * @return memory size
 */
size_t get_mem_size(ssne_tensor_t tensor);

/**
 * @brief get tensor width
 * @param tensor tensor
 * @return width
 */
uint32_t get_width(ssne_tensor_t tensor);

/**
 * @brief get tensor height
 * @param tensor tensor
 * @return height
 */
uint32_t get_height(ssne_tensor_t tensor);

/**
 * @brief get tensor dtype
 * @param tensor tensor
 * @return dtype
 */
uint8_t get_data_type(ssne_tensor_t tensor);

/**
 * @brief set tensor dtype
 * @param tensor tensor
 * @param dtype data type
 * @return status
 */
int set_data_type(ssne_tensor_t tensor, uint8_t dtype);

/**
 * @brief get tensor data format
 * @param tensor tensor
 * @return data format
 */
uint8_t get_data_format(ssne_tensor_t tensor);

/**
 * @brief get tensor data
 * @param tensor tensor
 * @return tensor data pointer
 */
void* get_data(ssne_tensor_t tensor);

/**
 * @brief comparetensor buffer
 * @param tensor_a one tensor to be compared
 * @param tensor_b another tensor to be compared
 * @param show_detail whether show detail when tensors are different
 * @return status code
 */
int compare_tensor(ssne_tensor_t tensor_a, ssne_tensor_t tensor_b, uint8_t show_detail = 0);

/**
 * @brief return a new tensor with the same shape and buffer
 * @param tensor tensor
 * @param seed seed
 * @return new tensor
 */
ssne_tensor_t copy_tensor(ssne_tensor_t tensor);

/**
 * @brief mirror tensor
 * @param src_tensor src_tensor
 * @param dst_tensor dst_tensor
 * @return status code
 */
int copy_tensor_buffer(ssne_tensor_t src_tensor, ssne_tensor_t dst_tensor);

/**
 * @brief mirror tensor
 * @param src_tensor src_tensor
 * @param dst_tensor dst_tensor
 * @return status code
 */
int mirror_tensor(ssne_tensor_t src_tensor, ssne_tensor_t dst_tensor);

/**
 * @brief fusiony8
 * @param src_tensors src_tensors
 * @return status code
 */
int fusiony8_tensor(std::vector<ssne_tensor_t>& src_tensors);

/**
 * @brief fusionraw10
 * @param src_tensors src_tensors
 * @return status code
 */
int fusionraw10_tensor(std::vector<ssne_tensor_t>& src_tensors);


/**
 * @brief fusionyuv8
 * @param src_tensors src_tensors
 * @return status code
 */
int fusionyuv8_tensor(std::vector<ssne_tensor_t>& src_tensors);


/**
 * @brief fusionyuv8
 * @param src_tensors src_tensors
 * @return status code
 */
int fpnc_tensor(std::vector<ssne_tensor_t>& src_tensors, uint8_t k, uint8_t m);


// ==============================
//        AiPreCapture
// ==============================

//return error_code
int OpenOnlinePipeline(PipelineIdType pipeline_id);
int OpenDoubleOnlinePipeline();
int CloseOnlinePipeline(PipelineIdType pipeline_id);

int OnlineSetBinning(PipelineIdType pipeline_id, BinningRatioType ratio_w, BinningRatioType ratio_h);
int OnlineSetCrop(PipelineIdType pipeline_id, uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2);
int OnlineSetNormalize2(uint16_t mean_1, uint16_t mean_2, uint16_t mean_3, uint16_t std_1, uint16_t std_2, uint16_t std_3, bool is_uint8);
int OnlineSetNormalize(uint16_t model_id);
int OnlineSetFrameDrop(PipelineIdType pipeline_id, uint8_t gated_frames, uint8_t skip_frames);
int OnlineSetOutputImage(PipelineIdType pipeline_id, uint8_t dtype, uint16_t width, uint16_t height);

int UpdateOnlineParam();

int GetImageData(ssne_tensor_t *cur_image, PipelineIdType pipeline_id, SensorIdType sensor_id, bool get_owner);
int ChangeLoadData(ssne_tensor_t* even_image, ssne_tensor_t* odd_image);
int GetDoubleImageData(ssne_tensor_t *image0, ssne_tensor_t *image1, SensorIdType sensor_id, bool get_owner0, bool get_owner1);
// ==============================
//        AiPreprocess
// ==============================

struct AiPreprocessPipe_;
typedef struct AiPreprocessPipe_ *AiPreprocessPipe;

AiPreprocessPipe GetAIPreprocessPipe();
int ReleaseAIPreprocessPipe(AiPreprocessPipe handle);
void Clear(AiPreprocessPipe handle);
int RunAiPreprocessPipe(AiPreprocessPipe handle, ssne_tensor_t input_image, ssne_tensor_t output_image);
int SetCrop(AiPreprocessPipe handle, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
int SetPadding(AiPreprocessPipe handle, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom,
                uint16_t color_y, uint16_t color_u, uint16_t color_v);
int SetPadding2(AiPreprocessPipe handle, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint16_t color);
int SetNormalize3(AiPreprocessPipe handle, uint16_t mean_1, uint16_t mean_2, uint16_t mean_3,
                    uint16_t std_scale_1, uint16_t std_scale_2, uint16_t std_scale_3,
                    uint16_t output_int8);
int SetNormalize2(AiPreprocessPipe handle, uint16_t mean, uint16_t std_scale, uint16_t output_int8);
int SetNormalize(AiPreprocessPipe handle, uint16_t model_id);
int SetFlip(AiPreprocessPipe handle, uint16_t is_filp);

// ==============================
//       isp debug
// ==============================

int set_isp_debug_config(ssne_tensor_t even_data, ssne_tensor_t odd_data);
int start_isp_debug_load();
int get_even_or_odd_flag(uint8_t &flag);

#ifdef __cplusplus
}
#endif

#endif