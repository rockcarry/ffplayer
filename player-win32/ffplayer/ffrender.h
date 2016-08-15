#ifndef __FFPLAYER_FFRENDER_H__
#define __FFPLAYER_FFRENDER_H__

// 包含头文件
#include "ffplayer.h"

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
void*render_open(int vdevtype, void *surface, AVRational frate, int pixfmt, int w, int h,
                 int adevtype, int srate, AVSampleFormat sndfmt, int64_t ch_layout);
void render_close   (void *hrender);
void render_audio   (void *hrender, AVFrame *audio);
void render_video   (void *hrender, AVFrame *video);
void render_setrect (void *hrender, int type, int x, int y, int w, int h);
void render_start   (void *hrender);
void render_pause   (void *hrender);
void render_reset   (void *hrender);
void render_setparam(void *hrender, DWORD id, void *param);
void render_getparam(void *hrender, DWORD id, void *param);
int  render_slowflag(void *hrender);

#ifdef __cplusplus
}
#endif

#endif















