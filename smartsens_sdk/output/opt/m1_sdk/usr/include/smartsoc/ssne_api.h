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

/**
 * @brief Pipeline identifier type
 * @details Used to identify different image processing pipelines
 */
typedef enum
{
    kPipeline0,  /**< Pipeline 0 identifier */
    kPipeline1   /**< Pipeline 1 identifier */
}PipelineIdType;

/**
 * @brief Sensor identifier type
 * @details Used to identify different image sensors
 */
typedef enum
{
    kSensor0,  /**< Sensor 0 identifier */
    kSensor1   /**< Sensor 1 identifier */
}SensorIdType;

/**
 * @brief Binning ratio type for image downsampling
 * @details Specifies the downsampling ratio for image binning operations
 */
typedef enum
{
    kDownSample1x = 1,  /**< No downsampling (1x) */
    kDownSample2x = 2,  /**< 2x downsampling */
    kDownSample4x = 4,  /**< 4x downsampling */
} BinningRatioType;

/****************************************/
/*********     ssne_tensor_t      *******/
/****************************************/
/**
 * @brief SSNE wrapper for ssne tensor
 * @details Structure that wraps tensor data pointer for SSNE operations
 * @param data Pointer to the tensor data buffer
 */
typedef struct ssne_tensor
{
    void *data;
} ssne_tensor_t;

/**
 * @brief Initialize SSNE (SmartSens Neural Engine)
 * @details Initializes the SSNE runtime environment. Must be called before using any other SSNE functions.
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 * @note This function should be called once at the beginning of the application
 */
int ssne_initial();

/**
 * @brief Release SSNE (SmartSens Neural Engine)
 * @details Releases all resources allocated by SSNE and cleans up the runtime environment.
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 * @note This function should be called at the end of the application to free resources
 */
int ssne_release();

/**
 * @brief Load SSModel for inference
 * @details Loads a SmartSens model file into SSNE for inference operations
 * @param path Path to the SSModel file
 * @param load_flag Model loading flag: SSNE_STATIC_ALLOC (0) for static allocation or SSNE_DYNAMIC_ALLOC (1) for dynamic allocation
 * @return Returns the model ID (uint16_t) on success, which should be used in subsequent operations. 
 * @note The returned model_id should be saved and used for inference and other model operations
 */
uint16_t ssne_loadmodel(char *path, uint8_t load_flag);

/**
 * @brief Get model normalization parameters
 * @details Retrieves the normalization parameters (mean, std, and data type flag) for a loaded model
 * @param model_id Model ID returned from ssne_loadmodel()
 * @param mean Pointer to array of 3 integers to store mean values [mean_1, mean_2, mean_3]
 * @param std Pointer to array of 3 integers to store standard deviation values [std_1, std_2, std_3]
 * @param is_uint8 Pointer to integer to store flag indicating if input is uint8 (1) or not (0)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int ssne_get_model_normalize_params(uint16_t model_id, int* mean, int* std, int* is_uint8);

/**
 * @brief Get the number of input tensors required by the model
 * @details Returns how many input tensors the model expects for inference
 * @param model_id Model ID returned from ssne_loadmodel()
 * @return Returns the number of input tensors required by the model, or 0 on error
 */
 int ssne_get_model_input_num(uint16_t model_id);

/**
 * @brief Get the input data type required by the model
 * @details Retrieves the data type (SSNE_UINT8, SSNE_INT8, or SSNE_FLOAT32) expected for model inputs
 * @param model_id Model ID returned from ssne_loadmodel()
 * @param dtype Pointer to integer to store the data type: SSNE_UINT8 (0), SSNE_INT8 (1), or SSNE_FLOAT32 (2)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
 int ssne_get_model_input_dtype(uint16_t model_id, int* dtype);


/**
 * @brief Run inference on SSModel
 * @details Executes inference on the loaded model with the provided input tensors
 * @param model_id Model ID returned from ssne_loadmodel()
 * @param input_num Number of input tensors (should match the model's expected input count)
 * @param input_tensor Array of input tensors containing the input data
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 * @note Input tensors must match the model's expected input shape, data type, and format
 */
int ssne_inference(uint16_t model_id, uint8_t input_num, ssne_tensor_t input_tensor[]);

/**
 * @brief Get SSModel inference results
 * @details Retrieves the output tensors from the last inference operation
 * @param model_id Model ID returned from ssne_loadmodel()
 * @param output_num Number of output tensors to retrieve (should match the model's output count)
 * @param output_tensor Array of output tensors to store the inference results. These tensors must be pre-allocated.
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 * @note Call ssne_inference() first.
 */
int ssne_getoutput(uint16_t model_id, uint8_t output_num, ssne_tensor_t output_tensor[]);

/**
 * @brief Move model to SRAM for faster access
 * @details Relocates the model data to SRAM memory for improved inference performance
 * @param model_id Model ID returned from ssne_loadmodel()
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 * @note Model has already been allocated on SRAM; NO NEED TO CALL
 */
int ssne_movemodeltosram(uint16_t model_id);


/**
 * @brief Create a tensor with specified width, height and format
 * @details Allocates and initializes a new tensor with the specified dimensions and data format
 * @param width Width of the tensor in pixels
 * @param height Height of the tensor in pixels
 * @param format Data format: SSNE_BYTES (0), SSNE_YUV422_20 (1), SSNE_YUV422_16 (2), SSNE_Y_10 (3), SSNE_Y_8 (4), SSNE_RGB (5), SSNE_BGR (6)
 * @param buffer_location Memory location for the tensor buffer (SSNE_BUF_LINUX, SSNE_BUF_AI, SSNE_BUF_SRAM, or SSNE_BUF_LNPU)
 * @return Returns an initialized ssne_tensor_t object. The tensor should be released with release_tensor() when no longer needed.
 */
ssne_tensor_t create_tensor(uint32_t width, uint32_t height, uint8_t format, ssne_buffer_type buffer_location);

/**
 * @brief Create a tensor from file
 * @details Creates a tensor and loads its data from a file. The tensor dimensions and format are determined from the file.
 * @param filepath Path to the file containing tensor data
 * @param buffer_location Memory location for the tensor buffer (default: SSNE_BUF_AI)
 * @return Returns an initialized ssne_tensor_t object loaded from the file. The tensor should be released with release_tensor() when no longer needed.
 */
ssne_tensor_t create_tensor_from_file(const char* filepath, ssne_buffer_type buffer_location = SSNE_BUF_AI);

/**
 * @brief Release tensor resources
 * @details Frees all memory and resources associated with the tensor
 * @param tensor Tensor to be released
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 * @note After calling this function, the tensor should not be used anymore
 */
int release_tensor(ssne_tensor_t tensor);

/**
 * @brief Load tensor data from file
 * @details Loads tensor buffer data from a file into an existing tensor
 * @param tensor Tensor to load data into (must be pre-allocated)
 * @param filepath Path to the file containing tensor data
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int load_tensor_buffer(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief Load tensor data from memory pointer
 * @details Loads tensor buffer data from a memory pointer into an existing tensor
 * @param tensor Tensor to load data into (must be pre-allocated)
 * @param data Pointer to the source data in memory
 * @param mem_size Size of the data to load in bytes
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int load_tensor_buffer_ptr(ssne_tensor_t tensor, void* data, int mem_size);

/**
 * @brief Save tensor to file
 * @details Saves the entire tensor (including metadata) to a file
 * @param tensor Tensor to save
 * @param filepath Path to the output file
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int save_tensor(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief Save tensor buffer data to file
 * @details Saves only the tensor buffer data (raw pixel data) to a file
 * @param tensor Tensor whose buffer data will be saved
 * @param filepath Path to the output file
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int save_tensor_buffer(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief Save tensor buffer data to memory pointer
 * @details Copies tensor buffer data to a memory location
 * @param tensor Tensor whose buffer data will be saved
 * @param data Pointer to the destination memory buffer
 * @param mem_size Size of the destination buffer in bytes (must be at least get_mem_size(tensor))
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int save_tensor_buffer_ptr(ssne_tensor_t tensor, void* data, int mem_size);

/**
 * @brief Get tensor total size
 * @details Returns the total size of the tensor including all channels 
 * @param tensor Tensor to query
 * @return Returns the total size in bytes
 */
uint32_t get_total_size(ssne_tensor_t tensor);

/**
 * @brief Get tensor memory size
 * @details Returns the actual memory size allocated for the tensor buffer
 * @param tensor Tensor to query
 * @return Returns the memory size in bytes
 */
size_t get_mem_size(ssne_tensor_t tensor);

/**
 * @brief Get tensor width
 * @details Returns the width dimension of the tensor
 * @param tensor Tensor to query
 * @return Returns the width in pixels
 */
uint32_t get_width(ssne_tensor_t tensor);

/**
 * @brief Get tensor height
 * @details Returns the height dimension of the tensor
 * @param tensor Tensor to query
 * @return Returns the height in pixels
 */
uint32_t get_height(ssne_tensor_t tensor);

/**
 * @brief Get tensor data type
 * @details Returns the data type of the tensor elements
 * @param tensor Tensor to query
 * @return Returns the data type: SSNE_UINT8 (0), SSNE_INT8 (1), or SSNE_FLOAT32 (2)
 */
uint8_t get_data_type(ssne_tensor_t tensor);

/**
 * @brief Set tensor data type
 * @details Sets the data type for tensor elements
 * @param tensor Tensor to modify
 * @param dtype Data type to set: SSNE_UINT8 (0), SSNE_INT8 (1), or SSNE_FLOAT32 (2)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int set_data_type(ssne_tensor_t tensor, uint8_t dtype);

/**
 * @brief Get tensor data format
 * @details Returns the pixel format of the tensor
 * @param tensor Tensor to query
 * @return Returns the format: SSNE_BYTES (0), SSNE_YUV422_20 (1), SSNE_YUV422_16 (2), SSNE_Y_10 (3), SSNE_Y_8 (4), SSNE_RGB (5), SSNE_BGR (6)
 */
uint8_t get_data_format(ssne_tensor_t tensor);

/**
 * @brief Get tensor data pointer
 * @details Returns a pointer to the tensor's data buffer
 * @param tensor Tensor to query
 * @return Returns a void pointer to the tensor data buffer, or NULL on error
 * @note The returned pointer should not be freed directly. Use release_tensor() instead.
 */
void* get_data(ssne_tensor_t tensor);

/**
 * @brief Compare two tensors
 * @details Compares the buffer data of two tensors to check if they are equal
 * @param tensor_a First tensor to compare
 * @param tensor_b Second tensor to compare
 * @param show_detail Flag to show detailed differences when tensors are different (default: 0)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) if tensors are equal, or an error code if different
 */
int compare_tensor(ssne_tensor_t tensor_a, ssne_tensor_t tensor_b, uint8_t show_detail = 0);

/**
 * @brief Copy tensor (create a new tensor with the same shape and data)
 * @details Creates a new tensor that is a copy of the source tensor, including shape and buffer data
 * @param tensor Source tensor to copy
 * @return Returns a new ssne_tensor_t object that is a copy of the source tensor. The new tensor should be released with release_tensor() when no longer needed.
 */
ssne_tensor_t copy_tensor(ssne_tensor_t tensor);

/**
 * @brief Copy tensor buffer data
 * @details Copies the buffer data from source tensor to destination tensor. Both tensors must have compatible shapes.
 * @param src_tensor Source tensor to copy data from
 * @param dst_tensor Destination tensor to copy data to (must be pre-allocated)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int copy_tensor_buffer(ssne_tensor_t src_tensor, ssne_tensor_t dst_tensor);

/**
 * @brief Mirror tensor (horizontal flip)
 * @details Performs horizontal mirroring (flip) operation on the tensor data
 * @param src_tensor Source tensor to mirror
 * @param dst_tensor Destination tensor to store mirrored data (must be pre-allocated with same dimensions as source)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int mirror_tensor(ssne_tensor_t src_tensor, ssne_tensor_t dst_tensor);

/**
 * @brief Fusion Y8 tensor operation
 * @details Performs Y8 format fusion operation on multiple input tensors
 * @param src_tensors Vector of source tensors to fuse
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int fusiony8_tensor(std::vector<ssne_tensor_t>& src_tensors);

/**
 * @brief Fusion RAW10 tensor operation
 * @details Performs RAW10 format fusion operation on multiple input tensors
 * @param src_tensors Vector of source tensors to fuse
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int fusionraw10_tensor(std::vector<ssne_tensor_t>& src_tensors);


/**
 * @brief Fusion YUV8 tensor operation
 * @details Performs YUV8 format fusion operation on multiple input tensors
 * @param src_tensors Vector of source tensors to fuse
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int fusionyuv8_tensor(std::vector<ssne_tensor_t>& src_tensors);


/**
 * @brief FPNC (Focal Plane Non-Uniformity Correction) tensor operation
 * @details Performs FPNC correction on multiple input tensors
 * @param src_tensors Vector of source tensors to process
 * @param k FPNC parameter k
 * @param m FPNC parameter m
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int fpnc_tensor(std::vector<ssne_tensor_t>& src_tensors, uint8_t k, uint8_t m);


// ==============================
//        AiPreCapture
// ==============================

/**
 * @brief Open online image capture pipeline
 * @details Opens a single online image capture pipeline for real-time image acquisition
 * @param pipeline_id Pipeline identifier (kPipeline0 or kPipeline1)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int OpenOnlinePipeline(PipelineIdType pipeline_id);

/**
 * @brief Open double online image capture pipeline
 * @details Opens both pipelines (Pipeline0 and Pipeline1) simultaneously for dual-sensor capture
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int OpenDoubleOnlinePipeline();

/**
 * @brief Close online image capture pipeline
 * @details Closes the specified online image capture pipeline and releases its resources
 * @param pipeline_id Pipeline identifier (kPipeline0 or kPipeline1) to close
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int CloseOnlinePipeline(PipelineIdType pipeline_id);

/**
 * @brief Set binning ratio for online pipeline
 * @details Sets the binning (downsampling) ratio for the specified pipeline
 * @param pipeline_id Pipeline identifier (kPipeline0 or kPipeline1)
 * @param ratio_w Horizontal binning ratio (kDownSample1x, kDownSample2x, or kDownSample4x)
 * @param ratio_h Vertical binning ratio (kDownSample1x, kDownSample2x, or kDownSample4x)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int OnlineSetBinning(PipelineIdType pipeline_id, BinningRatioType ratio_w, BinningRatioType ratio_h);

/**
 * @brief Set crop region for online pipeline
 * @details Sets the crop region for image capture in the specified pipeline
 * @param pipeline_id Pipeline identifier (kPipeline0 or kPipeline1)
 * @param x1 Left coordinate of crop region
 * @param x2 Right coordinate of crop region
 * @param y1 Top coordinate of crop region
 * @param y2 Bottom coordinate of crop region
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int OnlineSetCrop(PipelineIdType pipeline_id, uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2);

/**
 * @brief Set normalization parameters for online pipeline (3-channel)
 * @details Sets the normalization parameters with separate mean and std values for 3 channels
 * @param mean_1 Mean value for channel 1
 * @param mean_2 Mean value for channel 2
 * @param mean_3 Mean value for channel 3
 * @param std_1 Standard deviation scale for channel 1
 * @param std_2 Standard deviation scale for channel 2
 * @param std_3 Standard deviation scale for channel 3
 * @param is_uint8 Flag indicating if input data is uint8 (true) or not (false)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int OnlineSetNormalize2(uint16_t mean_1, uint16_t mean_2, uint16_t mean_3, uint16_t std_1, uint16_t std_2, uint16_t std_3, bool is_uint8);

/**
 * @brief Set normalization parameters from model
 * @details Sets normalization parameters automatically from a loaded model's configuration
 * @param model_id Model ID returned from ssne_loadmodel()
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int OnlineSetNormalize(uint16_t model_id);

/**
 * @brief Set frame drop parameters for online pipeline
 * @details Configures frame gating and skipping for the specified pipeline
 * @param pipeline_id Pipeline identifier (kPipeline0 or kPipeline1)
 * @param gated_frames Number of frames to gate (wait) before capture
 * @param skip_frames Number of frames to skip between captures
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int OnlineSetFrameDrop(PipelineIdType pipeline_id, uint8_t gated_frames, uint8_t skip_frames);

/**
 * @brief Set output image parameters for online pipeline
 * @details Configures the output image data type and dimensions for the specified pipeline
 * @param pipeline_id Pipeline identifier (kPipeline0 or kPipeline1)
 * @param dtype Output data type: SSNE_UINT8 (0), SSNE_INT8 (1), or SSNE_FLOAT32 (2)
 * @param width Output image width in pixels
 * @param height Output image height in pixels
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int OnlineSetOutputImage(PipelineIdType pipeline_id, uint8_t dtype, uint16_t width, uint16_t height);

/**
 * @brief Update online pipeline parameters
 * @details Applies all pending parameter changes to the online pipelines
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 * @note This function should be called after setting all pipeline parameters to apply them
 */
int UpdateOnlineParam();

/**
 * @brief Get image data from online pipeline
 * @details Captures and retrieves image data from the specified pipeline and sensor
 * @param cur_image Pointer to tensor to store the captured image 
 * @param pipeline_id Pipeline identifier (kPipeline0 or kPipeline1)
 * @param sensor_id Sensor identifier (kSensor0 or kSensor1)
 * @param get_owner Flag indicating if the caller takes ownership of the buffer (true) or not (false)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int GetImageData(ssne_tensor_t *cur_image, PipelineIdType pipeline_id, SensorIdType sensor_id, bool get_owner);

/**
 * @brief Get dual image data for stereo vision 
 * @details Captures and retrieves dual image data from the specified pipeline for stereo vision applications.
 *          This function is used for binocular/stereo camera systems where two images are captured simultaneously
 *          from left and right cameras or sensors.
 * @param image0 Pointer to tensor to store the first image (left camera or primary sensor)
 * @param image1 Pointer to tensor to store the second image (right camera or secondary sensor)
 * @param pipeline_id Pipeline identifier (kPipeline0 or kPipeline1)
 * @param get_owner Flag indicating if the caller takes ownership of the buffers (true) or not (false)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 * @note Both image0 and image1 tensors must be pre-allocated before calling this function.
 *       This function is specifically designed for stereo/binocular vision applications.
 */
int GetDualImageData(ssne_tensor_t *image0, ssne_tensor_t *image1, PipelineIdType pipeline_id, bool get_owner);

/**
 * @brief Change load data for even/odd frame processing
 * @details Switches the data loading between even and odd frames for frame-based processing
 * @param even_image Pointer to tensor for even frame data
 * @param odd_image Pointer to tensor for odd frame data
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int ChangeLoadData(ssne_tensor_t* even_image, ssne_tensor_t* odd_image);

/**
 * @brief Get double image data from online pipeline
 * @details Captures image data from both pipelines simultaneously for dual-sensor capture
 * @param image0 Pointer to tensor to store image from first pipeline (must be pre-allocated)
 * @param image1 Pointer to tensor to store image from second pipeline (must be pre-allocated)
 * @param sensor_id Sensor identifier (kSensor0 or kSensor1)
 * @param get_owner0 Flag indicating if caller takes ownership of first buffer (true) or not (false)
 * @param get_owner1 Flag indicating if caller takes ownership of second buffer (true) or not (false)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int GetDoubleImageData(ssne_tensor_t *image0, ssne_tensor_t *image1, SensorIdType sensor_id, bool get_owner0, bool get_owner1);
// ==============================
//        AiPreprocess
// ==============================

struct AiPreprocessPipe_;
typedef struct AiPreprocessPipe_ *AiPreprocessPipe;

/**
 * @brief Get AI preprocessing pipeline handle
 * @details Creates and returns a handle to an AI preprocessing pipeline for image preprocessing operations
 * @return Returns an AiPreprocessPipe handle on success, or NULL on failure
 * @note The returned handle should be released with ReleaseAIPreprocessPipe() when no longer needed
 */
AiPreprocessPipe GetAIPreprocessPipe();

/**
 * @brief Release AI preprocessing pipeline handle
 * @details Releases the AI preprocessing pipeline handle and frees associated resources
 * @param handle AI preprocessing pipeline handle returned from GetAIPreprocessPipe()
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int ReleaseAIPreprocessPipe(AiPreprocessPipe handle);

/**
 * @brief Clear AI preprocessing pipeline state
 * @details Clears the internal state of the AI preprocessing pipeline
 * @param handle AI preprocessing pipeline handle
 */
void Clear(AiPreprocessPipe handle);

/**
 * @brief Run AI preprocessing pipeline
 * @details Executes the preprocessing pipeline on the input image and produces the output image
 * @param handle AI preprocessing pipeline handle
 * @param input_image Input tensor containing the source image
 * @param output_image Output tensor to store the preprocessed image (must be pre-allocated)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int RunAiPreprocessPipe(AiPreprocessPipe handle, ssne_tensor_t input_image, ssne_tensor_t output_image);

/**
 * @brief Set crop region for AI preprocessing pipeline
 * @details Sets the crop region for image preprocessing operations
 * @param handle AI preprocessing pipeline handle
 * @param x1 Left coordinate of crop region
 * @param y1 Top coordinate of crop region
 * @param x2 Right coordinate of crop region
 * @param y2 Bottom coordinate of crop region
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int SetCrop(AiPreprocessPipe handle, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief Set padding for AI preprocessing pipeline (YUV format)
 * @details Sets padding parameters with separate Y, U, V color values for YUV format images
 * @param handle AI preprocessing pipeline handle
 * @param left Left padding size in pixels
 * @param top Top padding size in pixels
 * @param right Right padding size in pixels
 * @param bottom Bottom padding size in pixels
 * @param color_y Y channel padding color value
 * @param color_u U channel padding color value
 * @param color_v V channel padding color value
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int SetPadding(AiPreprocessPipe handle, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom,
                uint16_t color_y, uint16_t color_u, uint16_t color_v);

/**
 * @brief Set padding for AI preprocessing pipeline (single color)
 * @details Sets padding parameters with a single color value for all channels
 * @param handle AI preprocessing pipeline handle
 * @param left Left padding size in pixels
 * @param top Top padding size in pixels
 * @param right Right padding size in pixels
 * @param bottom Bottom padding size in pixels
 * @param color Padding color value for all channels
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int SetPadding2(AiPreprocessPipe handle, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint16_t color);

/**
 * @brief Set normalization parameters for AI preprocessing pipeline (3-channel)
 * @details Sets normalization parameters with separate mean and std scale values for 3 channels
 * @param handle AI preprocessing pipeline handle
 * @param mean_1 Mean value for channel 1
 * @param mean_2 Mean value for channel 2
 * @param mean_3 Mean value for channel 3
 * @param std_scale_1 Standard deviation scale for channel 1
 * @param std_scale_2 Standard deviation scale for channel 2
 * @param std_scale_3 Standard deviation scale for channel 3
 * @param output_int8 Flag indicating if output should be int8 (1) or not (0)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int SetNormalize3(AiPreprocessPipe handle, uint16_t mean_1, uint16_t mean_2, uint16_t mean_3,
                    uint16_t std_scale_1, uint16_t std_scale_2, uint16_t std_scale_3,
                    uint16_t output_int8);

/**
 * @brief Set normalization parameters for AI preprocessing pipeline (single channel)
 * @details Sets normalization parameters with a single mean and std scale value for all channels
 * @param handle AI preprocessing pipeline handle
 * @param mean Mean value for all channels
 * @param std_scale Standard deviation scale for all channels
 * @param output_int8 Flag indicating if output should be int8 (1) or not (0)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int SetNormalize2(AiPreprocessPipe handle, uint16_t mean, uint16_t std_scale, uint16_t output_int8);

/**
 * @brief Set normalization parameters from model for AI preprocessing pipeline
 * @details Sets normalization parameters automatically from a loaded model's configuration
 * @param handle AI preprocessing pipeline handle
 * @param model_id Model ID returned from ssne_loadmodel()
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int SetNormalize(AiPreprocessPipe handle, uint16_t model_id);

/**
 * @brief Set flip operation for AI preprocessing pipeline
 * @details Enables or disables horizontal/vertical flip operation
 * @param handle AI preprocessing pipeline handle
 * @param is_filp Flag to enable flip (non-zero) or disable flip (0)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int SetFlip(AiPreprocessPipe handle, uint16_t is_filp);

// ==============================
//       isp debug
// ==============================

/**
 * @brief Set ISP debug configuration
 * @details Configures ISP debug mode with even and odd frame data
 * @param even_data Tensor containing even frame data for ISP debug
 * @param odd_data Tensor containing odd frame data for ISP debug
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int set_isp_debug_config(ssne_tensor_t even_data, ssne_tensor_t odd_data);

/**
 * @brief Start ISP debug data loading
 * @details Starts the ISP debug data loading process
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int start_isp_debug_load();

/**
 * @brief Get even or odd frame flag
 * @details Retrieves the current even/odd frame flag from ISP debug system
 * @param flag Reference to store the flag value (0 for even, 1 for odd)
 * @return Returns SSNE_ERRCODE_NO_ERROR (0) on success, or an error code on failure
 */
int get_even_or_odd_flag(uint8_t &flag);

#ifdef __cplusplus
}
#endif

#endif
