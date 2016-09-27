#ifndef __FFPLAYER_SNAPSHOT_H__
#define __FFPLAYER_SNAPSHOT_H__

#ifdef __cplusplus
extern "C" {
#endif

// 包含头文件
#include "libavutil/frame.h"

// 函数声明
void* snapshot_init(void);
int   snapshot_take(void *ctxt, char *file, AVFrame *video);
void  snapshot_free(void *ctxt);

#ifdef __cplusplus
}
#endif

#endif


