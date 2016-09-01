#ifndef __FFPLAYER_HWACCEL_H__
#define __FFPLAYER_HWACCEL_H__

#ifdef __cplusplus
extern "C" {
#endif

// 包含头文件
#include "libavcodec/avcodec.h"

// 函数声明
void hwaccel_init(AVCodecContext *ctxt, char *hwaccel);
void hwaccel_free(AVCodecContext *ctxt);

#ifdef __cplusplus
}
#endif

#endif


