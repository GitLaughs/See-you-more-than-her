/**
 * @file osd_lib_api.h
 * @brief OSD库公共API头文件
 * @author Jingwen Bai
 * @date 2024-07-08
 * @description OSD (On-Screen Display) 屏幕显示库公共API接口
 * @details 特性：\n
 *          - OSD最大支持8个layer，每个图层同一行最大支持4个凸四边形\n
 *          - Layer编码方式支持两种：行程编码（RLE）和四边形编码（Quadrangle）\n
 *          - 最大支持32种颜色（0-29为有效颜色，30为反色，31为透明）\n
 *          - 支持Layer自动分配模式和手动分配模式\n
 *          - 基本流程：打开设备 → 初始化 → 创建图层 → add绘制对象 → flush刷写到显示内存 → 关闭设备
 */

#ifndef SS_OSD_LIB_API_H
#define SS_OSD_LIB_API_H

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

/** @brief 设备句柄类型 */
typedef unsigned long handle_t;

/** @brief 无效句柄值 */
#define INVALID_HANDLE (0)

/** @brief 8位无符号整数类型 */
typedef uint8_t BYTE;

/** @brief 8位无符号整数指针类型 */
typedef uint8_t *PBYTE;

/** @brief 16位无符号整数类型 */
typedef uint16_t WORD;

/** @brief 16位无符号整数指针类型 */
typedef uint16_t *PWORD;

/** @brief 32位无符号整数类型 */
typedef uint32_t  DWORD;

/**
 * @brief 缓冲区句柄结构体
 */
typedef struct buf_handle {
    uint32_t buf_type;      /**< 缓冲区类型 */
    union {
        int fd_dmabuf;      /**< DMA缓冲区文件描述符 */
        uint32_t sram_buf_handle; /**< SRAM缓冲区句柄 */
    } buf;
}buf_handle;

/** @brief API导出宏，用于共享库可见性 */
#define EXPORT_API __attribute__((visibility("default")))


namespace fdevice{

// ============================================================================
// Type Definitions (needed for API declarations)
// ============================================================================

/**
 * @brief 图层句柄枚举
 * @details OSD IP支持8个图层 (LAYER0 ~ LAYER7)
 */
typedef enum ssLAYER_HANDLE
{    
    LAYER0_HANDLE,  /**< 图层0句柄 */
    LAYER1_HANDLE,  /**< 图层1句柄 */
    LAYER2_HANDLE,  /**< 图层2句柄 */
    LAYER3_HANDLE,  /**< 图层3句柄 */
    LAYER4_HANDLE,  /**< 图层4句柄 */
    LAYER5_HANDLE,  /**< 图层5句柄 */
    LAYER6_HANDLE,  /**< 图层6句柄 */
    LAYER7_HANDLE,  /**< 图层7句柄 */
    LAYER_HANDLE_MAX /**< 最大图层数量 */
}LAYER_HANDLE;

/**
 * @brief 图层编码类型枚举
 * @details 支持的编码类型：RLE (行程编码) 和 四边形编码
 */
typedef enum ssLAYER_TYPE
{
  SS_TYPE_RLE,        /**< 行程编码 (Run-Length Encoding) */
  SS_TYPE_QUADRANGLE, /**< 四边形编码 */
  SS_TYPE_BUTT        /**< 无效类型 */
}LAYER_TYPE;

/**
 * @brief 图层坐标结构体
 * @details 定义图层的起始坐标，坐标系以图像中心为原点
 * @note 坐标原点为图像中心\n
 *       坐标范围：[-32768, 32767]
 */
typedef struct ssLAYER_COORD
{
    int16_t layer_start_x; /**< 图层起始X坐标 */
    int16_t layer_start_y; /**< 图层起始Y坐标 */
}LAYER_COORD;

/**
 * @brief 图层尺寸结构体
 * @details 超出图层尺寸部分的图像将不会被绘制
 */
typedef struct ssLAYER_SIZE
{
    int layer_width;  /**< 图层宽度 */
    int layer_height; /**< 图层高度 */
}LAYER_SIZE;

/**
 * @brief 四边形类型枚举
 */
typedef enum tagQUADRANGLETYPE
{
    TYPE_HOLLOW = 0, /**< 空心四边形 */
    TYPE_SOLID       /**< 实心四边形 */
} QUADRANGLETYPE;

/**
 * @brief Alpha透明度类型枚举
 */
typedef enum tagALPHATYPE
{
    TYPE_ALPHA25 = 0,  /**< 25%不透明度 */
    TYPE_ALPHA50,      /**< 50%不透明度 */
    TYPE_ALPHA75,      /**< 75%不透明度 */
    TYPE_ALPHA100      /**< 100%不透明度 */
} ALPHATYPE;

/**
 * @brief 区域类型枚举
 */
enum RGNTYPE
{
    TYPE_IMAGE = 0,   /**< 图像区域 */
    TYPE_GRAPHIC     /**< 图形区域 */
};

/**
 * @brief 坐标点结构体
 * @details 在图层内绘制图形或位图时，以所在图层的左上角为坐标原点
 * @note X：向右为正\n
 *       Y：向下为正\n
 *       单位：像素
 */
typedef struct tagPOINT_S
{
    int x; /**< X坐标 */
    int y; /**< Y坐标 */
}POINT_S;

/**
 * @brief 尺寸结构体
 */
typedef struct tagSIZE_S
{
    int w; /**< 宽度 */
    int h; /**< 高度 */
}SIZE_S;

/**
 * @brief 四边形顶点结构体
 */
typedef struct tagVERTEXS_S
{
    POINT_S points[4]; /**< 四边形的四个顶点 */
}VERTEXS_S;

/**
 * @brief 四边形覆盖属性结构体
 * @details 四边形的完整属性配置
 * @note 颜色索引范围：0-31（32色：其中30，31是特殊索引，索引30为反色，索引31为透明。实际颜色索引最大支持0-29）\n
 *       所有位图/图形共享唯一颜色查找表\n
 *       顶点坐标以图层左上角为原点，向右为正，向下为正
 */
typedef struct tagCOVER_ATTR_S
{
    int  colorIdx;        /**< 颜色索引（0-29为有效颜色，30为反色，31为透明） */
    QUADRANGLETYPE  eSolid; /**< 实心或空心 */
    ALPHATYPE       alpha;  /**< Alpha透明度（25%/50%/75%/100%） */
    VERTEXS_S       vertex_out; /**< 外四边形顶点 */
    VERTEXS_S       vertex_in;  /**< 内四边形顶点 */
}COVER_ATTR_S;

/**
 * @brief 位图信息结构体
 * @details 位图文件的配置信息
 * @note 位图文件必须为.ssbmp格式\n
 *       位置以左上角坐标为基准定位，超出区域部分会被裁剪\n
 *       颜色索引范围：0-31（30为反色，31为透明，实际颜色索引最大支持0-29）
 */
typedef struct tagBITMAP_INFO_S
{
    const char*       pSSbmpFile; /**< SS位图文件路径（.ssbmp格式） */
    ALPHATYPE   alpha;            /**< Alpha透明度（25%/50%/75%/100%） */
    POINT_S     position;         /**< 位图位置（以图层左上角为原点） */
}BITMAP_INFO_S;

/**
 * @brief 区域属性结构体
 */
typedef struct tagRGN_ATTR_S
{ 
    RGNTYPE enType;  /**< 区域类型 (图像或图形) */
    SIZE_S size_s;    /**< 区域大小 */
}RGN_ATTR_S;

/**
 * @brief RLE (行程编码) 结构体
 * @details RLE编码的位图信息
 */
typedef struct ssRLE_S
{
    char*       pdata_addr;  /**< RLE编码数据首地址 */
    int         data_length; /**< RLE编码数据长度 */
    buf_handle  osd_buf;      /**< OSD缓冲区句柄 */
}RLE_S;

/**
 * @brief 四边形编码结构体
 * @details 四边形类型的编码信息
 */
typedef struct ssQUADRANGLE_S
{
    char*       pdata_addr;  /**< 四边形编码数据首地址 */
    int         data_length; /**< 四边形编码数据长度 */
    buf_handle  osd_buf;      /**< OSD缓冲区句柄 */
}QUADRANGLE_S;

/**
 * @brief 图层属性结构体
 * @details 完整的图层配置，包括位置、尺寸、编码类型和数据
 */
typedef struct ssLAYER_ATTR_S
{
    LAYER_TYPE      codeTYPE;      /**< 编码类型 (RLE 或 四边形) */
    LAYER_COORD     layerStart;    /**< 图层起始坐标 */
    LAYER_SIZE      layerSize;     /**< 图层尺寸 */
    int             sensor_flag;    /**< 传感器标志 */
    QUADRANGLE_S    layer_data_QR; /**< 四边形编码数据 */
    RLE_S           layer_data_RLE;/**< RLE编码数据 */
    RGN_ATTR_S      layer_rgn;     /**< 区域属性 */
}LAYER_ATTR_S;

/**
 * @brief DMA缓冲区属性结构体
 */
typedef struct ssDMA_BUFFER_ATTR_S{
	void* dma = nullptr;   /**< 主DMA缓冲区指针 */
	void* dma_2 = nullptr; /**< 辅助DMA缓冲区指针 */
}DMA_BUFFER_ATTR_S;

#ifdef __cplusplus
extern "C"{
#endif

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief 获取OSD库版本字符串
 * @return 版本字符串 (例如: "flc-osd:v0.0.3")
 */
EXPORT_API const char *osd_get_lib_version(void);

/**
 * @brief 打开OSD设备
 * @details 打开OSD设备，创建设备句柄。必须在使用其他API之前调用。
 * @return 成功：返回有效设备句柄handle（非0）\n
 *         失败：返回INVALID_HANDLE (0)
 */
EXPORT_API handle_t osd_open_device();

/**
 * @brief 关闭OSD设备
 * @details 关闭OSD设备，释放设备句柄和相关资源。
 * @param handle 设备句柄，由osd_open_device()返回
 * @return 成功：0\n
 *         失败：-1（通常因为句柄无效）
 * @note 关闭前建议先清理所有图层和缓冲区\n
 *       关闭后句柄无效，不可再使用\n
 *       建议程序退出前调用
 */
EXPORT_API int osd_close_device(handle_t handle);

/**
 * @brief 初始化OSD设备
 * @details 初始化OSD设备，设置图层数量和颜色查找表（Color LUT）。在打开设备后、创建图层前调用。
 * @param handle 设备句柄
 * @param layer_cnt 图层数量（1-8，最大为8）
 * @param pFileColorLUT Color LUT数据指针，指向COLORLUT_S结构体
 * @return 成功：0\n
 *         失败：-1
 * @note 最大layer_cnt <= 8（LAYER_HANDLE_MAX）\n
 *       LUT格式：前8字节头部信息，后续为RGB（每3字节一组），OSD设备全局只有一个颜色查找表，为所有图层共用\n
 *       初始化后设备配置为阻塞模式\n
 *       layer_cnt > 8 返回 -1
 */
EXPORT_API int osd_init_device(handle_t handle, int layer_cnt, char *pFileColorLUT);

/**
 * @brief 创建图层
 * @details 创建新的OSD图层。图层是OSD显示基本单位，可独立配置管理。
 * @param handle 设备句柄
 * @param layer_index 图层索引（LAYER0_HANDLE ~ LAYER7_HANDLE）
 * @param pstLayer 图层属性结构体指针
 * @return 成功：0\n
 *         失败：-1
 * @note 同一图层索引只能创建一次，重复创建会失败\n
 *       最大8个图层\n
 *       pstLayer必须完整初始化\n
 *       创建后默认禁用，需osd_enable_layer使能，flush功能的API例如osd_flush_quad_rangle也会使能图层\n
 *       索引已存在或超出最大数量返回-1
 */
EXPORT_API int osd_create_layer(handle_t handle, LAYER_HANDLE layer_index, LAYER_ATTR_S *pstLayer);

/**
 * @brief 销毁图层
 * @details 销毁指定图层并释放资源。
 * @param handle 设备句柄
 * @param layer_index 图层索引
 * @return 成功：0\n
 *         失败：-1（图层不存在或已锁定）
 * @note 建议销毁前先osd_clean_layer\n
 *       锁定图层也可销毁（会从锁定映射移除）\n
 *       销毁后索引可复用
 */
EXPORT_API int osd_destroy_layer(handle_t handle, LAYER_HANDLE layer_index);

/**
 * @brief 启用或禁用图层
 * @details 使能/禁用指定图层。使能后才会显示。
 * @param handle 设备句柄
 * @param layer_index 图层索引
 * @param enbale true使能；false禁用
 * @return 成功：0\n
 *         失败：-1
 * @note 创建后默认禁用\n
 *       禁用不会清空内容，仅不显示\n
 *       可随时切换状态\n
 *       Layer enable说明：\n
 *       - 即使对指定层进行disable，将仍然可以对其layer进行add和flush操作\n
 *       - 但是在最终绘制过程中将不会对该层进行绘制\n
 *       - 该功能需要慎用\n
 *       - 在layer自动分配模式下将不会考虑layer的enable属性
 */
EXPORT_API int osd_enable_layer(handle_t handle, LAYER_HANDLE layer_index, bool enbale);

/**
 * @brief 锁定或解锁图层
 * @details 锁定/解锁图层。锁定后不能修改，只能查看或销毁。
 * @param handle 设备句柄
 * @param layer_index 图层索引
 * @param lock true锁定；false解锁
 * @return 成功：0\n
 *         失败：-1（图层不存在）
 * @note 锁定图层不能被osd_clean_layer清理\n
 *       锁定图层仍可enable/disable\n
 *       锁定图层可被destroy\n
 *       Layer lock说明：\n
 *       - layer锁定模式下，该层将不会被写入和清空\n
 *       - layer自动模式下，在添加绘制对象的过程中将会跳过锁定层\n
 *       - layer手动模式下，无法对已经锁定层进行绘制对象的添加
 */
EXPORT_API int osd_lock_layer(handle_t handle, LAYER_HANDLE layer_index, bool lock);

/**
 * @brief 清空指定图层
 * @details 清空图层内容但保留图层本身。
 * @param handle 设备句柄
 * @param layer_index 图层索引
 * @return 成功：0\n
 *         失败：-1（图层不存在或已锁定）
 * @note 仅可清空未锁定图层\n
 *       清空会刷新空数据并重新使能图层（显示空白）
 */
EXPORT_API int osd_clean_layer(handle_t handle, LAYER_HANDLE layer_index);

/**
 * @brief 清空所有图层
 * @details 清空所有未锁定图层内容。
 * @param handle 设备句柄
 * @return 成功：0\n
 *         失败：-1（无效句柄）
 * @note 仅清理未锁定图层\n
 *       锁定图层不受影响
 */
EXPORT_API int osd_clean_all_layer(handle_t handle);

/**
 * @brief 获取OSD设备状态
 * @details 获取OSD设备当前状态。
 * @param handle 设备句柄
 * @param status 输出参数，用于接收状态值
 * @return 成功：0\n
 *         失败：-1
 */
EXPORT_API int osd_get_status(handle_t handle, unsigned char *status);

/**
 * @brief 分配DMA缓冲区
 * @details 分配DMA缓冲区，用于存储编码数据。
 * @param handle 设备句柄
 * @param buffer_handle 返回缓冲区句柄
 * @param buf_size 缓冲区大小（字节）
 * @return 成功：0，buffer_handle有效\n
 *         失败：-1，buffer_handle == nullptr
 * @note 缓冲区可被多个图层复用\n
 *       分配缓冲区后可以等待，例如使用sleep(0.25)确保硬件稳定\n
 *       大小建议：\n
 *       - 四边形编码：例如1024
 *       - RLE位图编码：例如0x20000（与图像大小/复杂度相关，若图像显示一部分则可以考虑增大DMAsize）
 */
EXPORT_API int osd_alloc_buffer(handle_t handle, void * &buffer_handle, int buf_size);

/**
 * @brief 删除DMA缓冲区
 * @details 释放已分配DMA缓冲区。
 * @param handle 设备句柄
 * @param buffer_handle 缓冲区句柄
 * @return 成功：0\n
 *         失败：-1
 * @note 释放前确保没有图层正在使用该缓冲区\n
 *       释放后句柄失效
 */
EXPORT_API int osd_delete_buffer(handle_t handle, void * buffer_handle);

/**
 * @brief 获取DMA缓冲区指针
 * @details 获取DMA缓冲区的虚拟地址指针，可直接访问缓冲区内容。
 * @param handle 设备句柄
 * @param buffer_handle 缓冲区句柄
 * @return 成功：虚拟地址指针\n
 *         失败：nullptr
 * @note 指针有效期：缓冲区释放前\n
 *       不要越界写入
 */
EXPORT_API void *osd_get_buffer_ptr(handle_t handle, void * buffer_handle);

/**
 * @brief 获取DMA缓冲区文件描述符
 * @details 获取DMA缓冲区文件描述符（fd），用于DMA相关操作。
 * @param handle 设备句柄
 * @param buffer_handle 缓冲区句柄
 * @return 成功：fd（>0）\n
 *         失败：-1
 * @note 不要手动close返回的fd\n
 *       缓冲区释放后fd自动失效
 */
EXPORT_API int osd_get_buffer_fd(handle_t handle, void * buffer_handle);

/**
 * @brief 设置图层DMA缓冲区
 * @details 为指定图层设置DMA缓冲区（存储编码数据）。
 * @param handle 设备句柄
 * @param layer_index 图层索引
 * @param dma DMA缓冲区属性
 * @return 成功：0\n
 *         失败：-1（图层不存在）
 * @note dma.dma必填\n
 *       dma.dma_2可选（双缓冲）\n
 *       单缓冲：仅设置dma.dma\n
 *       双缓冲：同时设置dma.dma与dma.dma_2
 */
EXPORT_API int osd_set_layer_buffer(handle_t handle, LAYER_HANDLE layer_index, DMA_BUFFER_ATTR_S dma);

/**
 * @brief 向任意匹配的图形图层添加四边形
 * @details Layer自动分配模式，向任意匹配的图形图层（TYPE_GRAPHIC）添加四边形（自动查找图层）。
 * @param handle 设备句柄
 * @param attr 四边形属性
 * @return 成功：0\n
 *         失败：-1（未找到可用图层或添加失败）
 * @note Layer自动分配模式：自动查找匹配的图层\n
 *       遍历所有未锁定图层，查找enType == TYPE_GRAPHIC\n
 *       若某图层添加失败会继续尝试下一个图层\n
 *       仅当成功添加到某个图层才返回0\n
 *       全部失败返回-1\n
 *       四边形限制：\n
 *       - 必须为凸四边形\n
 *       - 在图层内绘制时，四边形的顶点坐标的单位是像素，坐标原点为图层左上角，向右为正，向下为正\n
 *       - 顶点顺序按照顺时针或者逆时针顺序\n
 *       - 同一行最多4个四边形（OSD最大支持8个layer，每个图层同一行最大支持4个凸四边形）\n
 *       - 每层存在最大数量限制\n
 *       - 在添加绘制对象的过程中将会跳过锁定层
 */
EXPORT_API int osd_add_quad_rangle(handle_t handle, COVER_ATTR_S *attr);

/**
 * @brief 刷新所有图形图层的四边形数据
 * @details Layer自动分配模式，刷新所有图形图层（TYPE_GRAPHIC）的四边形数据到OSD显示内存中。
 * @param handle 设备句柄
 * @return 成功：0\n
 *         失败：-1
 * @note Layer自动分配模式：刷写所有qr对象\n
 *       仅刷新TYPE_GRAPHIC图层\n
 *       刷新包含：编码 → 拷贝DMA → 更新寄存器 → 使能图层\n
 *       将当前帧的多个绘制对象一次刷写到OSD显示内存中
 */
EXPORT_API int osd_flush_quad_rangle(handle_t handle);

/**
 * @brief 向任意匹配的图像图层添加位图
 * @details Layer自动分配模式，向任意匹配的图像图层（TYPE_IMAGE）添加rl对象，支持bmp绘制，位图文件必须为.ssbmp。
 * @param handle 设备句柄
 * @param info 位图信息
 * @return 成功：0\n
 *         失败：-1（未找到可用图层或添加失败）
 * @note Layer自动分配模式：自动查找匹配的图层\n
 *       遍历未锁定图层，查找enType == TYPE_IMAGE\n
 *       位图文件必须为.ssbmp\n
 *       位置以左上角坐标为基准定位，超出区域部分会被裁剪\n
 *       全部失败返回-1\n
 *       在添加绘制对象的过程中将会跳过锁定层\n
 *       SSBMP文件格式：\n
 *       - Magic：4字节0x5353424D（"SSBM"）\n
 *       - width：4字节\n
 *       - height：4字节\n
 *       - colorNum：4字节\n
 *       - data：索引颜色数据
 */
EXPORT_API int osd_add_texture(handle_t handle, BITMAP_INFO_S *info);

/**
 * @brief 刷新所有图像图层的位图数据
 * @details Layer自动分配模式，刷写所有层rl对象到OSD显示内存中，支持bmp绘制。
 * @param handle 设备句柄
 * @return 成功：0\n
 *         失败：-1
 * @note Layer自动分配模式：刷写所有层rl对象\n
 *       仅刷新TYPE_IMAGE图层\n
 *       刷新包含：RLE编码 → 拷贝DMA → 更新寄存器 → 使能图层\n
 *       将当前帧的多个绘制对象一次刷写到OSD显示内存中
 */
EXPORT_API int osd_flush_texture(handle_t handle);

/**
 * @brief 向指定图层添加四边形
 * @details Layer手动分配模式，向指定图层添加四边形，需要指定layer。
 * @param handle 设备句柄
 * @param layer_index 目标图层
 * @param attr 四边形属性
 * @return 成功：0\n
 *         失败：-1（图层不存在或类型不匹配）
 * @note Layer手动分配模式：需要指定layer\n
 *       目标图层必须为TYPE_GRAPHIC\n
 *       无法对已经锁定层进行绘制对象的添加\n
 *       添加后需调用osd_flush_quad_rangle_layer
 */
EXPORT_API int osd_add_quad_rangle_layer(handle_t handle, LAYER_HANDLE layer_index, COVER_ATTR_S *attr);

/**
 * @brief 刷新指定图层的四边形数据
 * @details Layer手动分配模式，刷写一个qr对象到指定layer的OSD显示内存中。
 * @param handle 设备句柄
 * @param layer_index 目标图层
 * @return 成功：0\n
 *         失败：-1（图层不存在或类型不匹配）
 * @note Layer手动分配模式：刷写一个qr对象到指定layer\n
 *       仅刷新TYPE_GRAPHIC图层\n
 *       刷新后自动使能图层\n
 *       原子刷新，保证一致性
 */
EXPORT_API int osd_flush_quad_rangle_layer(handle_t handle, LAYER_HANDLE layer_index);

/**
 * @brief 向指定图层添加位图
 * @details Layer手动分配模式，向指定图层添加位图，需要指定layer。
 * @param handle 设备句柄
 * @param layer_index 目标图层
 * @param info 位图信息
 * @return 成功：0\n
 *         失败：-1（图层不存在或类型不匹配）\n
 *         失败：-2（添加失败，如编码数据过大或文件无效）
 * @note Layer手动分配模式：需要指定layer\n
 *       目标图层必须为TYPE_IMAGE\n
 *       无法对已经锁定层进行绘制对象的添加\n
 *       添加后需调用osd_flush_texture_layer\n
 *       错误码说明：\n
 *       - -1：图层不存在或类型不匹配\n
 *       - -2：位图添加失败（数据过大/文件无效等）
 */
EXPORT_API int osd_add_texture_layer(handle_t handle, LAYER_HANDLE layer_index, BITMAP_INFO_S *info);

/**
 * @brief 刷新指定图层位图数据
 * @details Layer手动分配模式，刷写一个rl对象到指定层OSD显示内存中。
 * @param handle 设备句柄
 * @param layer_index 目标图层
 * @return 成功：0\n
 *         失败：-1（图层不存在或类型不匹配）
 * @note Layer手动分配模式：刷写一个rl对象到指定层\n
 *       仅刷新TYPE_IMAGE图层\n
 *       刷新后自动使能图层\n
 *       原子刷新，保证一致性
 */
EXPORT_API int osd_flush_texture_layer(handle_t handle, LAYER_HANDLE layer_index);

#ifdef __cplusplus
}
#endif

// ============================================================================
// Additional Type Definitions
// ============================================================================

/**
 * @brief 图层不透明度枚举
 * @details OSD IP支持5个不透明度级别
 */
typedef enum ssLAYER_OPACITY
{
    LAYER_OPACITY_00,   /**< 全透明 */
    LAYER_OPACITY_25,   /**< 25%不透明度 */
    LAYER_OPACITY_50,   /**< 50%不透明度 */
    LAYER_OPACITY_75,   /**< 75%不透明度 */
    LAYER_OPACITY_100,  /**< 100%不透明度 */
    LAYER_BUTT          /**< 无效不透明度 */
}LAYER_OPACITY;

/**
 * @brief 步长枚举
 */
typedef enum tagSTEPSIZE
{
    TYPE_STEP_SHORT = 0, /**< 短步长 */
    TYPE_STEP_LONG       /**< 长步长 */
} STEPSIZE;

/**
 * @brief 顶点排列类型枚举
 * @details 描述四边形顶点的几何排列方式
 */
enum VERTEXTYPE
{
    TYPE_LINE = 0,    /**< 四点共线 */
    TYPE_TRIANGLE,    /**< 三点共线 */
    TYPE_CONVERX,     /**< 凸四边形 */
    TYPE_CONCAVE      /**< 凹四边形 */
};

/**
 * @brief 区域优先级枚举
 */
enum RGNPRIORITY
{
    PRIORITY_0 = 0, /**< 优先级0 */
    PRIORITY_1,     /**< 优先级1 */
    PRIORITY_2,     /**< 优先级2 */
    PRIORITY_3,     /**< 优先级3 */
    PRIORITY_4      /**< 优先级4 */
};

/**
 * @brief 整型向量结构体
 */
typedef struct tagVECTOR_INT_S
{
    int x; /**< X分量 */
    int y; /**< Y分量 */
}VECTOR_INT_S;

/**
 * @brief SS位图属性结构体
 * @details 位图头结构，魔数为0x5353424d ('SSBM')
 */
typedef struct tagSSBITMAP_ATTR_S
{
    DWORD bmHead;      /**< 位图头 (0x5353424d 'SSBM') */
    DWORD bmWidth;     /**< 位图宽度 */
    DWORD bmHeigth;    /**< 位图高度 */
    DWORD bmColorNum;  /**< 颜色数量 */
    BYTE* bmData;      /**< 位图数据区指针 */
}SSBITMAP_ATTR_S;

#pragma pack()

} // namespace fdevice


#endif // SS_OSD_LIB_API_H