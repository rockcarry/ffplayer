#ifndef _CORERENDER_H_
#define _CORERENDER_H_

// 包含头文件
#include <android_runtime/AndroidRuntime.h>
#include <android_runtime/android_view_Surface.h>
#include <gui/Surface.h>

extern "C" {
#include "libavformat/avformat.h"
}

using namespace android;

// 类型定义
typedef struct
{
    int16_t *data;
    int32_t  size;
} AUDIOBUF;

// 函数声明
void* renderopen(sp<ANativeWindow> win, int ww, int wh,
                 AVRational frate, int pixfmt, int vw, int vh,
                 int srate, int sndfmt, int64_t ch_layout);

void renderclose     (void *hrender);
void renderaudiowrite(void *hrender, AVFrame *audio);
void rendervideowrite(void *hrender, AVFrame *video);
void renderstart     (void *hrender);
void renderpause     (void *hrender);
void renderseek      (void *hrender, int sec);
int  rendertime      (void *hrender);

#endif















