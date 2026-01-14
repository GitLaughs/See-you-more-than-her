#ifndef __C_API_SS_BUFFER_H_
#define __C_API_SS_BUFFER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef void* BufferHandle;
typedef void* BufPoolHandle;

typedef enum {
  BUFFER_TYPE_SYSTEM,
  BUFFER_TYPE_DMABUF,
  BUFFER_TYPE_SRAM,

  /* Keep it last */
  BUFFER_TYPE_MAX
} BufferType;

typedef enum {
  /* system buffer */
  BUFFER_AREA_COMMON = 0,

  /* dmabuf */
  BUFFER_AREA_OSD,
  BUFFER_AREA_AI,

  /* sram buf */
  BUFFER_AREA_SRAM,

  /*  Keep it last */
  BUFFER_AREA_MAX,
  BUFFER_AREA_NUM
} BufferArea;


/* __attribute__((visbility("default"))) */
BufferHandle SS_MB_CreateBuffer(size_t size, BufferArea buffer_area);
int32_t SS_MB_ReleaseBuffer(BufferHandle mb);
void *SS_MB_GetPtr(BufferHandle mb);
int32_t SS_MB_GetBufType(BufferHandle mb);
uint32_t SS_MB_GetSramBufHandle(BufferHandle mb);
int32_t SS_MB_GetFD(BufferHandle mb);
int32_t SS_MB_BeginCPUAccess(BufferHandle mb, int32_t rdonly);
int32_t SS_MB_EndCPUAccess(BufferHandle mb, int32_t rdonly);

BufPoolHandle SS_MB_PoolCreate(int32_t cnt, int32_t size, BufferArea buf_area);
BufferHandle SS_MB_PoolGetBlk(BufPoolHandle mbp);
int32_t SS_MB_PoolPutBlk(void *mb);
int32_t SS_MB_PoolDestroy(void *mbphandle);


#ifdef __cplusplus
}
#endif

#endif // __C_API_SS_BUFFER_H_
