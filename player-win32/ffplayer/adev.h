#ifndef _NES_ADEV_H_
#define _NES_ADEV_H_

// 包含头文件
#include "corerender.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
// 类型定义
typedef struct {
    BYTE  *lpdata;
    DWORD  buflen;
} AUDIOBUF;
#endif

// 函数声明
void* adev_create  (int bufnum, int buflen);
void  adev_destroy (void *ctxt);
void  adev_request (void *ctxt, AUDIOBUF **ppab);
void  adev_post    (void *ctxt, int64_t pts);
void  adev_pause   (void *ctxt, BOOL pause);
void  adev_reset   (void *ctxt);
void  adev_syncapts(void *ctxt, int64_t *apts);

#ifdef __cplusplus
}
#endif

#endif

