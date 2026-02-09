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

/**
 * @brief SSNE错误码定义
 * @details 定义SSNE API返回的各种错误码，用于标识操作结果和错误类型
 */
#define SSNE_ERRCODE_NO_ERROR       0      /**< 无错误，操作成功 */
#define SSNE_ERRCODE_JOB_ERROR      100    /**< 任务/作业错误 */
#define SSNE_ERRCODE_FILE_ERROR     200    /**< 文件操作错误 */
#define SSNE_ERRCODE_MODEL_FILE_ERROR      201    /**< 模型文件格式错误 */
#define SSNE_ERRCODE_DEVICE_ERROR   300    /**< 设备错误 */
#define SSNE_ERRCODE_INPUT_ERROR    400    /**< 输入错误（通用） */
#define SSNE_ERRCODE_INPUT_NUM_ERROR       401    /**< 输入数量错误 */
#define SSNE_ERRCODE_INPUT_SIZE_ERROR      402    /**< 输入尺寸错误 */
#define SSNE_ERRCODE_INPUT_DTYPE_ERROR     403    /**< 输入数据类型错误 */
#define SSNE_ERRCODE_INPUT_FORMAT_ERROR    404    /**< 输入格式错误 */
#define SSNE_ERRCODE_INPUT_BUFFER_ERROR    405    /**< 输入缓冲区错误 */
#define SSNE_ERRCODE_OUTPUT_ERROR   500    /**< 输出错误 */
#define SSNE_ERRCODE_SPACE_ERROR    600    /**< 空间/内存不足错误 */

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
 * @brief SSNE缓冲区类型，用于定位数据源
 */
enum ssne_buffer_type
{
    SSNE_BUF_LINUX = 0,
    SSNE_BUF_AI = 1,
    SSNE_BUF_SRAM = 2,
    SSNE_BUF_LNPU = 3
};

/**
 * @brief 管道标识符类型
 * @details 用于标识不同的图像处理管道
 */
typedef enum
{
    kPipeline0,  /**< 管道0标识符 */
    kPipeline1   /**< 管道1标识符 */
}PipelineIdType;

/**
 * @brief 传感器标识符类型
 * @details 用于标识不同的图像传感器
 */
typedef enum
{
    kSensor0,  /**< 传感器0标识符 */
    kSensor1   /**< 传感器1标识符 */
}SensorIdType;

/**
 * @brief 图像下采样binning比例类型
 * @details 指定图像binning操作的下采样比例
 */
typedef enum
{
    kDownSample1x = 1,  /**< 无下采样 (1x) */
    kDownSample2x = 2,  /**< 2倍下采样 */
    kDownSample4x = 4,  /**< 4倍下采样 */
} BinningRatioType;

/****************************************/
/*********     ssne_tensor_t      *******/
/****************************************/
/**
 * @brief SSNE的tensor包装结构
 * @details 用于SSNE操作的tensor数据指针包装结构
 * @param data 指向tensor数据缓冲区的指针
 */
typedef struct ssne_tensor
{
    void *data;
} ssne_tensor_t;

/**
 * @brief 初始化SSNE (SmartSens Neural Engine)
 * @details 初始化SSNE运行时环境。在使用任何其他SSNE函数之前必须调用此函数。
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 此函数应在应用程序开始时调用一次
 */
int ssne_initial();

/**
 * @brief 释放SSNE (SmartSens Neural Engine)
 * @details 释放SSNE分配的所有资源并清理运行时环境。
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 此函数应在应用程序结束时调用以释放资源
 */
int ssne_release();

/**
 * @brief 加载SSModel用于推理
 * @details 将SmartSens模型文件加载到SSNE中用于推理操作
 * @param path SSModel文件路径
 * @param load_flag 模型加载标志：SSNE_STATIC_ALLOC (0) 表示静态分配，SSNE_DYNAMIC_ALLOC (1) 表示动态分配
 * @return 成功返回模型ID (uint16_t)，该ID应在后续操作中使用。
 * @note 返回的model_id应保存并在推理和其他模型操作中使用
 */
uint16_t ssne_loadmodel(char *path, uint8_t load_flag);

/**
 * @brief 获取模型归一化参数
 * @details 获取已加载模型的归一化参数（均值、标准差和数据类型标志）
 * @param model_id 从ssne_loadmodel()返回的模型ID
 * @param mean 指向3个整数数组的指针，用于存储均值 [mean_1, mean_2, mean_3]
 * @param std 指向3个整数数组的指针，用于存储标准差 [std_1, std_2, std_3]
 * @param is_uint8 指向整数的指针，用于存储标志，表示输入是否为uint8 (1) 或不是 (0)
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int ssne_get_model_normalize_params(uint16_t model_id, int* mean, int* std, int* is_uint8);

/**
 * @brief 获取模型所需的输入tensor数量
 * @details 返回模型期望用于推理的输入tensor数量
 * @param model_id 从ssne_loadmodel()返回的模型ID
 * @return 返回模型所需的输入tensor数量，错误时返回0
 */
 int ssne_get_model_input_num(uint16_t model_id);

/**
 * @brief 获取模型所需的输入数据类型
 * @details 获取模型输入期望的数据类型（SSNE_UINT8、SSNE_INT8或SSNE_FLOAT32）
 * @param model_id 从ssne_loadmodel()返回的模型ID
 * @param dtype 指向整数的指针，用于存储数据类型：SSNE_UINT8 (0)、SSNE_INT8 (1) 或 SSNE_FLOAT32 (2)
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
 int ssne_get_model_input_dtype(uint16_t model_id, int* dtype);


/**
 * @brief 运行SSModel推理
 * @details 使用提供的输入tensor对已加载的模型执行推理
 * @param model_id 从ssne_loadmodel()返回的模型ID
 * @param input_num 输入tensor数量（应与模型期望的输入数量匹配）
 * @param input_tensor 包含输入数据的输入tensor数组
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 输入tensor必须与模型期望的输入形状、数据类型和格式匹配
 */
int ssne_inference(uint16_t model_id, uint8_t input_num, ssne_tensor_t input_tensor[]);

/**
 * @brief 获取SSModel推理结果
 * @details 从最后一次推理操作中获取输出tensor
 * @param model_id 从ssne_loadmodel()返回的模型ID
 * @param output_num 要获取的输出tensor数量（应与模型的输出数量匹配）
 * @param output_tensor 用于存储推理结果的输出tensor数组。这些tensor必须预先分配。
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 请先调用ssne_inference()。
 */
int ssne_getoutput(uint16_t model_id, uint8_t output_num, ssne_tensor_t output_tensor[]);

/**
 * @brief 将模型移动到SRAM以获得更快的访问速度
 * @details 将模型数据重新定位到SRAM内存以提高推理性能
 * @param model_id 从ssne_loadmodel()返回的模型ID
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 模型已分配在SRAM上；无需调用
 */
int ssne_movemodeltosram(uint16_t model_id);


/**
 * @brief 创建具有指定宽度、高度和格式的tensor
 * @details 分配并初始化一个具有指定维度和数据格式的新tensor
 * @param width tensor的宽度（像素）
 * @param height tensor的高度（像素）
 * @param format 数据格式：SSNE_BYTES (0)、SSNE_YUV422_20 (1)、SSNE_YUV422_16 (2)、SSNE_Y_10 (3)、SSNE_Y_8 (4)、SSNE_RGB (5)、SSNE_BGR (6)
 * @param buffer_location tensor缓冲区的内存位置（SSNE_BUF_LINUX、SSNE_BUF_AI、SSNE_BUF_SRAM或SSNE_BUF_LNPU）
 * @return 返回一个初始化的ssne_tensor_t对象。当不再需要时，应使用release_tensor()释放tensor。
 */
ssne_tensor_t create_tensor(uint32_t width, uint32_t height, uint8_t format, ssne_buffer_type buffer_location);

/**
 * @brief 从文件创建tensor
 * @details 创建tensor并从文件加载其数据。tensor的维度和格式由文件确定。
 * @param filepath 包含tensor数据的文件路径
 * @param buffer_location tensor缓冲区的内存位置（默认：SSNE_BUF_AI）
 * @return 返回从文件加载的已初始化ssne_tensor_t对象。当不再需要时，应使用release_tensor()释放tensor。
 */
ssne_tensor_t create_tensor_from_file(const char* filepath, ssne_buffer_type buffer_location = SSNE_BUF_AI);

/**
 * @brief 释放tensor资源
 * @details 释放与tensor关联的所有内存和资源
 * @param tensor 要释放的tensor
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 调用此函数后，不应再使用该tensor
 */
int release_tensor(ssne_tensor_t tensor);

/**
 * @brief 从文件加载tensor数据
 * @details 将tensor缓冲区数据从文件加载到现有tensor中
 * @param tensor 要加载数据的tensor（必须预先分配）
 * @param filepath 包含tensor数据的文件路径
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int load_tensor_buffer(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief 从内存指针加载tensor数据
 * @details 将tensor缓冲区数据从内存指针加载到现有tensor中
 * @param tensor 要加载数据的tensor（必须预先分配）
 * @param data 指向内存中源数据的指针
 * @param mem_size 要加载的数据大小（字节）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int load_tensor_buffer_ptr(ssne_tensor_t tensor, void* data, int mem_size);

/**
 * @brief 保存tensor到文件
 * @details 将整个tensor（包括元数据）保存到文件
 * @param tensor 要保存的tensor
 * @param filepath 输出文件路径
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int save_tensor(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief 保存tensor缓冲区数据到文件
 * @details 仅将tensor缓冲区数据（原始像素数据）保存到文件
 * @param tensor 要保存缓冲区数据的tensor
 * @param filepath 输出文件路径
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int save_tensor_buffer(ssne_tensor_t tensor, const char* filepath);

/**
 * @brief 保存tensor缓冲区数据到内存指针
 * @details 将tensor缓冲区数据复制到内存位置
 * @param tensor 要保存缓冲区数据的tensor
 * @param data 指向目标内存缓冲区的指针
 * @param mem_size 目标缓冲区的大小（字节）（必须至少为get_mem_size(tensor)）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int save_tensor_buffer_ptr(ssne_tensor_t tensor, void* data, int mem_size);

/**
 * @brief 获取tensor总大小
 * @details 返回tensor的总大小，包括所有通道
 * @param tensor 要查询的tensor
 * @return 返回总大小（字节）
 */
uint32_t get_total_size(ssne_tensor_t tensor);

/**
 * @brief 获取tensor内存大小
 * @details 返回为tensor缓冲区分配的实际内存大小
 * @param tensor 要查询的tensor
 * @return 返回内存大小（字节）
 */
size_t get_mem_size(ssne_tensor_t tensor);

/**
 * @brief 获取tensor宽度
 * @details 返回tensor的宽度维度
 * @param tensor 要查询的tensor
 * @return 返回宽度（像素）
 */
uint32_t get_width(ssne_tensor_t tensor);

/**
 * @brief 获取tensor高度
 * @details 返回tensor的高度维度
 * @param tensor 要查询的tensor
 * @return 返回高度（像素）
 */
uint32_t get_height(ssne_tensor_t tensor);

/**
 * @brief 获取tensor数据类型
 * @details 返回tensor元素的数据类型
 * @param tensor 要查询的tensor
 * @return 返回数据类型：SSNE_UINT8 (0)、SSNE_INT8 (1) 或 SSNE_FLOAT32 (2)
 */
uint8_t get_data_type(ssne_tensor_t tensor);

/**
 * @brief 设置tensor数据类型
 * @details 设置tensor元素的数据类型
 * @param tensor 要修改的tensor
 * @param dtype 要设置的数据类型：SSNE_UINT8 (0)、SSNE_INT8 (1) 或 SSNE_FLOAT32 (2)
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int set_data_type(ssne_tensor_t tensor, uint8_t dtype);

/**
 * @brief 获取tensor数据格式
 * @details 返回tensor的像素格式
 * @param tensor 要查询的tensor
 * @return 返回格式：SSNE_BYTES (0)、SSNE_YUV422_20 (1)、SSNE_YUV422_16 (2)、SSNE_Y_10 (3)、SSNE_Y_8 (4)、SSNE_RGB (5)、SSNE_BGR (6)
 */
uint8_t get_data_format(ssne_tensor_t tensor);

/**
 * @brief 获取tensor数据指针
 * @details 返回指向tensor数据缓冲区的指针
 * @param tensor 要查询的tensor
 * @return 返回指向tensor数据缓冲区的void指针，错误时返回NULL
 * @note 返回的指针不应直接释放。应使用release_tensor()代替。
 */
void* get_data(ssne_tensor_t tensor);

/**
 * @brief 比较两个tensor
 * @details 比较两个tensor的缓冲区数据以检查它们是否相等
 * @param tensor_a 要比较的第一个tensor
 * @param tensor_b 要比较的第二个tensor
 * @param show_detail 当tensor不同时是否显示详细差异的标志（默认：0）
 * @return 如果tensor相等返回SSNE_ERRCODE_NO_ERROR (0)，否则返回错误码
 */
int compare_tensor(ssne_tensor_t tensor_a, ssne_tensor_t tensor_b, uint8_t show_detail = 0);

/**
 * @brief 复制tensor（创建具有相同形状和数据的新tensor）
 * @details 创建一个新tensor，它是源tensor的副本，包括形状和缓冲区数据
 * @param tensor 要复制的源tensor
 * @return 返回一个新ssne_tensor_t对象，它是源tensor的副本。当不再需要时，应使用release_tensor()释放新tensor。
 */
ssne_tensor_t copy_tensor(ssne_tensor_t tensor);

/**
 * @brief 复制tensor缓冲区数据
 * @details 将缓冲区数据从源tensor复制到目标tensor。两个tensor必须具有兼容的形状。
 * @param src_tensor 要复制数据的源tensor
 * @param dst_tensor 要复制数据的目标tensor（必须预先分配）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int copy_tensor_buffer(ssne_tensor_t src_tensor, ssne_tensor_t dst_tensor);

/**
 * @brief 复制双tensor缓冲区数据（合并两个源tensor）
 * @details 将两个源tensor的缓冲区数据复制并合并到单个目标tensor中。
 *          此函数通常用于双目/立体视觉应用，需要将来自两个传感器或相机的数据合并到一个tensor中。
 * @param src1_tensor 要复制数据的第一个源tensor
 * @param src2_tensor 要复制数据的第二个源tensor
 * @param dst_tensor 用于存储合并数据的目标tensor 
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 目标tensor必须预先分配足够的大小以容纳来自两个源的合并数据
 */
int copy_double_tensor_buffer(ssne_tensor_t src1_tensor, ssne_tensor_t src2_tensor, ssne_tensor_t dst_tensor);
/**
 * @brief 镜像tensor（水平翻转）
 * @details 对tensor数据执行水平镜像（翻转）操作
 * @param src_tensor 要镜像的源tensor
 * @param dst_tensor 用于存储镜像数据的目标tensor（必须预先分配，与源tensor具有相同的维度）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int mirror_tensor(ssne_tensor_t src_tensor, ssne_tensor_t dst_tensor);

/**
 * @brief Y8格式tensor融合操作
 * @details 对多个输入tensor执行Y8格式融合操作
 * @param src_tensors 要融合的源tensor向量
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int fusiony8_tensor(std::vector<ssne_tensor_t>& src_tensors);

/**
 * @brief RAW10格式tensor融合操作
 * @details 对多个输入tensor执行RAW10格式融合操作
 * @param src_tensors 要融合的源tensor向量
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int fusionraw10_tensor(std::vector<ssne_tensor_t>& src_tensors);


/**
 * @brief YUV8格式tensor融合操作
 * @details 对多个输入tensor执行YUV8格式融合操作
 * @param src_tensors 要融合的源tensor向量
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int fusionyuv8_tensor(std::vector<ssne_tensor_t>& src_tensors);


/**
 * @brief FPNC（焦平面非均匀性校正）tensor操作
 * @details 对多个输入tensor执行FPNC校正
 * @param src_tensors 要处理的源tensor向量
 * @param k FPNC参数k
 * @param m FPNC参数m
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int fpnc_tensor(std::vector<ssne_tensor_t>& src_tensors, uint8_t k, uint8_t m);


// ==============================
//        AiPreCapture
// ==============================

/**
 * @brief 打开在线图像捕获管道
 * @details 打开单个在线图像捕获管道以进行实时图像采集
 * @param pipeline_id 管道标识符（kPipeline0或kPipeline1）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int OpenOnlinePipeline(PipelineIdType pipeline_id);

/**
 * @brief 打开双在线图像捕获管道
 * @details 同时打开两个管道（Pipeline0和Pipeline1）以进行双传感器捕获
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int OpenDoubleOnlinePipeline();

/**
 * @brief 打开双目传感器在线管道用于立体视觉
 * @details 打开双目传感器在线图像捕获管道用于立体/双目视觉应用。
 *          此函数在指定管道上启用从两个传感器（左右相机）同时进行图像捕获，用于立体视觉处理。
 * @param pipeline_id 要打开的双传感器支持的管道标识符（kPipeline0或kPipeline1）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 此函数专门为需要双传感器输入的立体视觉应用设计
 */
int OpenDualSnrOnline(PipelineIdType pipeline_id);

/**
 * @brief 关闭在线图像捕获管道
 * @details 关闭指定的在线图像捕获管道并释放其资源
 * @param pipeline_id 要关闭的管道标识符（kPipeline0或kPipeline1）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int CloseOnlinePipeline(PipelineIdType pipeline_id);

/**
 * @brief 设置在线管道的binning比例
 * @details 为指定管道设置binning（下采样）比例
 * @param pipeline_id 管道标识符（kPipeline0或kPipeline1）
 * @param ratio_w 水平binning比例（kDownSample1x、kDownSample2x或kDownSample4x）
 * @param ratio_h 垂直binning比例（kDownSample1x、kDownSample2x或kDownSample4x）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int OnlineSetBinning(PipelineIdType pipeline_id, BinningRatioType ratio_w, BinningRatioType ratio_h);

/**
 * @brief 设置在线管道的裁剪区域
 * @details 为指定管道中的图像捕获设置裁剪区域
 * @param pipeline_id 管道标识符（kPipeline0或kPipeline1）
 * @param x1 裁剪区域的左坐标
 * @param x2 裁剪区域的右坐标
 * @param y1 裁剪区域的顶部坐标
 * @param y2 裁剪区域的底部坐标
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int OnlineSetCrop(PipelineIdType pipeline_id, uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2);

/**
 * @brief 设置在线管道的归一化参数（3通道）
 * @details 为3个通道设置具有独立均值和标准差的归一化参数
 * @param mean_1 通道1的均值
 * @param mean_2 通道2的均值
 * @param mean_3 通道3的均值
 * @param std_1 通道1的标准差缩放
 * @param std_2 通道2的标准差缩放
 * @param std_3 通道3的标准差缩放
 * @param is_uint8 指示输入数据是否为uint8的标志（true）或不是（false）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int OnlineSetNormalize2(uint16_t mean_1, uint16_t mean_2, uint16_t mean_3, uint16_t std_1, uint16_t std_2, uint16_t std_3, bool is_uint8);

/**
 * @brief 从模型设置归一化参数
 * @details 从已加载模型的配置自动设置归一化参数
 * @param model_id 从ssne_loadmodel()返回的模型ID
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int OnlineSetNormalize(uint16_t model_id);

/**
 * @brief 设置在线管道的帧丢弃参数
 * @details 为指定管道配置帧门控和跳过
 * @param pipeline_id 管道标识符（kPipeline0或kPipeline1）
 * @param gated_frames 捕获前要门控（等待）的帧数
 * @param skip_frames 两次捕获之间要跳过的帧数
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int OnlineSetFrameDrop(PipelineIdType pipeline_id, uint8_t gated_frames, uint8_t skip_frames);

/**
 * @brief 设置在线管道的输出图像参数
 * @details 为指定管道配置输出图像数据类型和尺寸
 * @param pipeline_id 管道标识符（kPipeline0或kPipeline1）
 * @param dtype 输出数据类型：SSNE_UINT8 (0)、SSNE_INT8 (1) 或 SSNE_FLOAT32 (2)
 * @param width 输出图像宽度（像素）
 * @param height 输出图像高度（像素）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int OnlineSetOutputImage(PipelineIdType pipeline_id, uint8_t dtype, uint16_t width, uint16_t height);

/**
 * @brief 更新在线管道参数
 * @details 将所有待处理的参数更改应用到在线管道
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 设置所有管道参数后应调用此函数以应用它们
 */
int UpdateOnlineParam();

/**
 * @brief 从在线管道获取图像数据
 * @details 从指定管道和传感器捕获并获取图像数据
 * @param cur_image 指向用于存储捕获图像的tensor的指针 
 * @param pipeline_id 管道标识符（kPipeline0或kPipeline1）
 * @param sensor_id 传感器标识符（kSensor0或kSensor1）
 * @param get_owner 指示调用者是否拥有缓冲区所有权的标志（true）或不是（false）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int GetImageData(ssne_tensor_t *cur_image, PipelineIdType pipeline_id, SensorIdType sensor_id, bool get_owner);

/**
 * @brief 获取双目图像数据用于立体视觉 
 * @details 从指定管道捕获并获取双目图像数据用于立体视觉应用。
 *          此函数用于双目/立体相机系统，其中从左右相机或传感器同时捕获两个图像。
 * @param image0 指向用于存储第一个图像的tensor的指针（左相机或主传感器）
 * @param image1 指向用于存储第二个图像的tensor的指针（右相机或次传感器）
 * @param pipeline_id 管道标识符（kPipeline0或kPipeline1）
 * @param get_owner 指示调用者是否拥有缓冲区所有权的标志（true）或不是（false）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 * @note 调用此函数之前，image0和image1 tensor必须预先分配。
 *       此函数专门为立体/双目视觉应用设计。
 */
int GetDualImageData(ssne_tensor_t *image0, ssne_tensor_t *image1, PipelineIdType pipeline_id, bool get_owner);

/**
 * @brief 更改奇偶帧处理的数据加载
 * @details 在奇偶帧之间切换数据加载以进行基于帧的处理
 * @param even_image 指向用于奇帧数据的tensor的指针
 * @param odd_image 指向用于偶帧数据的tensor的指针
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int ChangeLoadData(ssne_tensor_t* even_image, ssne_tensor_t* odd_image);

/**
 * @brief 从在线管道获取双图像数据
 * @details 同时从两个管道捕获图像数据以进行双传感器捕获
 * @param image0 指向用于存储第一个管道图像的tensor的指针（必须预先分配）
 * @param image1 指向用于存储第二个管道图像的tensor的指针（必须预先分配）
 * @param sensor_id 传感器标识符（kSensor0或kSensor1）
 * @param get_owner0 指示调用者是否拥有第一个缓冲区所有权的标志（true）或不是（false）
 * @param get_owner1 指示调用者是否拥有第二个缓冲区所有权的标志（true）或不是（false）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int GetDoubleImageData(ssne_tensor_t *image0, ssne_tensor_t *image1, SensorIdType sensor_id, bool get_owner0, bool get_owner1);
// ==============================
//        AiPreprocess
// ==============================

struct AiPreprocessPipe_;
typedef struct AiPreprocessPipe_ *AiPreprocessPipe;

/**
 * @brief 获取AI预处理管道句柄
 * @details 创建并返回用于图像预处理操作的AI预处理管道句柄
 * @return 成功返回AiPreprocessPipe句柄，失败返回NULL
 * @note 返回的句柄在不再需要时应使用ReleaseAIPreprocessPipe()释放
 */
AiPreprocessPipe GetAIPreprocessPipe();

/**
 * @brief 释放AI预处理管道句柄
 * @details 释放AI预处理管道句柄并释放关联的资源
 * @param handle 从GetAIPreprocessPipe()返回的AI预处理管道句柄
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int ReleaseAIPreprocessPipe(AiPreprocessPipe handle);

/**
 * @brief 清除AI预处理管道状态
 * @details 清除AI预处理管道的内部状态
 * @param handle AI预处理管道句柄
 */
void Clear(AiPreprocessPipe handle);

/**
 * @brief 运行AI预处理管道
 * @details 对输入图像执行预处理管道并生成输出图像
 * @param handle AI预处理管道句柄
 * @param input_image 包含源图像的输入tensor
 * @param output_image 用于存储预处理图像的输出tensor（必须预先分配）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int RunAiPreprocessPipe(AiPreprocessPipe handle, ssne_tensor_t input_image, ssne_tensor_t output_image);

/**
 * @brief 设置AI预处理管道的裁剪区域
 * @details 为图像预处理操作设置裁剪区域
 * @param handle AI预处理管道句柄
 * @param x1 裁剪区域的左坐标
 * @param y1 裁剪区域的顶部坐标
 * @param x2 裁剪区域的右坐标
 * @param y2 裁剪区域的底部坐标
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int SetCrop(AiPreprocessPipe handle, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief 设置AI预处理管道的填充（YUV格式）
 * @details 为YUV格式图像设置具有独立Y、U、V颜色值的填充参数
 * @param handle AI预处理管道句柄
 * @param left 左填充大小（像素）
 * @param top 顶部填充大小（像素）
 * @param right 右填充大小（像素）
 * @param bottom 底部填充大小（像素）
 * @param color_y Y通道填充颜色值
 * @param color_u U通道填充颜色值
 * @param color_v V通道填充颜色值
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int SetPadding(AiPreprocessPipe handle, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom,
                uint16_t color_y, uint16_t color_u, uint16_t color_v);

/**
 * @brief 设置AI预处理管道的填充（单色）
 * @details 为所有通道设置具有单一颜色值的填充参数
 * @param handle AI预处理管道句柄
 * @param left 左填充大小（像素）
 * @param top 顶部填充大小（像素）
 * @param right 右填充大小（像素）
 * @param bottom 底部填充大小（像素）
 * @param color 所有通道的填充颜色值
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int SetPadding2(AiPreprocessPipe handle, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint16_t color);

/**
 * @brief 设置AI预处理管道的归一化参数（3通道）
 * @details 为3个通道设置具有独立均值和标准差缩放值的归一化参数
 * @param handle AI预处理管道句柄
 * @param mean_1 通道1的均值
 * @param mean_2 通道2的均值
 * @param mean_3 通道3的均值
 * @param std_scale_1 通道1的标准差缩放
 * @param std_scale_2 通道2的标准差缩放
 * @param std_scale_3 通道3的标准差缩放
 * @param output_int8 指示输出是否应为int8的标志（1）或不是（0）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int SetNormalize3(AiPreprocessPipe handle, uint16_t mean_1, uint16_t mean_2, uint16_t mean_3,
                    uint16_t std_scale_1, uint16_t std_scale_2, uint16_t std_scale_3,
                    uint16_t output_int8);

/**
 * @brief 设置AI预处理管道的归一化参数（单通道）
 * @details 为所有通道设置具有单一均值和标准差缩放值的归一化参数
 * @param handle AI预处理管道句柄
 * @param mean 所有通道的均值
 * @param std_scale 所有通道的标准差缩放
 * @param output_int8 指示输出是否应为int8的标志（1）或不是（0）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int SetNormalize2(AiPreprocessPipe handle, uint16_t mean, uint16_t std_scale, uint16_t output_int8);

/**
 * @brief 从模型为AI预处理管道设置归一化参数
 * @details 从已加载模型的配置自动设置归一化参数
 * @param handle AI预处理管道句柄
 * @param model_id 从ssne_loadmodel()返回的模型ID
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int SetNormalize(AiPreprocessPipe handle, uint16_t model_id);

/**
 * @brief 设置AI预处理管道的翻转操作
 * @details 启用或禁用水平/垂直翻转操作
 * @param handle AI预处理管道句柄
 * @param is_filp 启用翻转的标志（非零）或禁用翻转（0）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int SetFlip(AiPreprocessPipe handle, uint16_t is_filp);

// ==============================
//       isp debug
// ==============================

/**
 * @brief 设置ISP调试配置
 * @details 使用奇偶帧数据配置ISP调试模式
 * @param even_data 包含ISP调试偶帧数据的tensor
 * @param odd_data 包含ISP调试奇帧数据的tensor
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int set_isp_debug_config(ssne_tensor_t even_data, ssne_tensor_t odd_data);

/**
 * @brief 启动ISP调试数据加载
 * @details 启动ISP调试数据加载过程
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int start_isp_debug_load();

/**
 * @brief 获取奇偶帧标志
 * @details 从ISP调试系统获取当前奇偶帧标志
 * @param flag 用于存储标志值的引用（0表示偶，1表示奇）
 * @return 成功返回SSNE_ERRCODE_NO_ERROR (0)，失败返回错误码
 */
int get_even_or_odd_flag(uint8_t &flag);

#ifdef __cplusplus
}
#endif

#endif
