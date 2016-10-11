#ifndef __FFPLAYER_SNAPSHOT_H__
#define __FFPLAYER_SNAPSHOT_H__

#ifdef __cplusplus
extern "C" {
#endif

// 包含头文件
#include "libavutil/frame.h"

// 函数声明
int take_snapshot(char *file, AVFrame *video);

#ifdef __cplusplus
}
#endif

#endif


