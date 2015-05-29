#ifndef _CORERENDER_H_
#define _CORERENDER_H_

// 包含头文件
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

// 类型定义
typedef struct
{
    int16_t *data;
    int32_t  size;
} AUDIOBUF;

// 函数声明
void* renderopen(void *surface, AVRational frate, int pixfmt, int w, int h,
                  int64_t ch_layout, AVSampleFormat sndfmt, int srate);

void renderclose     (void *hrender);
void renderaudiowrite(void *hrender, AVFrame *audio);
void rendervideowrite(void *hrender, AVFrame *video);
void rendersetrect   (void *hrender, int x, int y, int w, int h);
void renderstart     (void *hrender);
void renderpause     (void *hrender);
void renderseek      (void *hrender, DWORD  sec );
void rendertime      (void *hrender, DWORD *time);

#ifdef __cplusplus
}
#endif

#endif















