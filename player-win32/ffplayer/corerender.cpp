// 包含头文件
#include <pthread.h>
#include "corerender.h"
#include "adev.h"
#include "vdev.h"
#include "log.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

#pragma warning(disable:4311)
#pragma warning(disable:4312)

// 内部类型定义
typedef struct
{
    void         *adev;
    void         *vdev;

    int           nVideoWidth;
    int           nVideoHeight;
    int           nFramePeriod;
    int           nRenderWidth;
    int           nRenderHeight;
    AVPixelFormat PixelFormat;
    SwrContext   *pSWRContext;
    SwsContext   *pSWSContext;

    int           nAdevBufAvail;
    BYTE         *pAdevBufCur;
    AUDIOBUF     *pAdevHdrCur;

    CRITICAL_SECTION  cs;
} RENDER;

// 函数实现
void* render_open(void *surface, AVRational frate, int pixfmt, int w, int h,
                 int srate, AVSampleFormat sndfmt, int64_t ch_layout)
{
    RENDER *render = (RENDER*)malloc(sizeof(RENDER));
    if (!render) {
        log_printf(TEXT("failed to allocate render context !\n"));
        exit(0);
    }

    // clear it first
    memset(render, 0, sizeof(RENDER));

    /* allocate & init swr context */
    render->pSWRContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
                                             ch_layout, sndfmt, srate, 0, NULL);
    swr_init(render->pSWRContext);

    // init for video
    render->nVideoWidth  = w;
    render->nVideoHeight = h;
    render->nFramePeriod = 1000 * frate.den / frate.num;
    render->PixelFormat  = (AVPixelFormat)pixfmt;

    // create adev & vdev
    render->adev = adev_create(0, (int)(44100.0 * frate.den / frate.num + 0.5) * 4);
    render->vdev = vdev_create(surface, 0, render->nRenderWidth, render->nRenderHeight, frate.num / frate.den);

    // make adev & vdev sync together
    int64_t *papts = NULL;
    vdev_getavpts(render->vdev, &papts, NULL);
    adev_syncapts(render->adev,  papts);

    InitializeCriticalSection(&render->cs);
    RECT rect; GetClientRect((HWND)surface, &rect);
    render_setrect(render, rect.left, rect.top, rect.right, rect.bottom);

    return render;
}

void render_close(void *hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;

    //++ audio ++//
    // destroy adev
    adev_destroy(render->adev);

    // free swr context
    swr_free(&render->pSWRContext);
    //-- audio --//

    //++ video ++//
    // destroy vdev
    vdev_destroy(render->vdev);

    // free sws context
    if (render->pSWSContext) {
        sws_freeContext(render->pSWSContext);
    }
    //-- video --//

    // free context
    free(render);
}

void render_audio(void *hrender, AVFrame *audio)
{
    if (!hrender) return;
    RENDER  *render  = (RENDER*)hrender;
    int      sampnum = 0;
    DWORD    apts    = (DWORD)audio->pts;

    do {
        if (render->nAdevBufAvail == 0) {
            adev_request(render->adev, &render->pAdevHdrCur);
            apts += render->nFramePeriod;
            render->nAdevBufAvail = (int  )render->pAdevHdrCur->size;
            render->pAdevBufCur   = (BYTE*)render->pAdevHdrCur->data;
        }

        //++ do resample audio data ++//
        sampnum = swr_convert(render->pSWRContext, (uint8_t**)&render->pAdevBufCur,
            render->nAdevBufAvail / 4, (const uint8_t**)audio->extended_data,
            audio->nb_samples);
        audio->extended_data  = NULL;
        audio->nb_samples     = 0;
        render->nAdevBufAvail -= sampnum * 4;
        render->pAdevBufCur   += sampnum * 4;
        //-- do resample audio data --//

        if (render->nAdevBufAvail == 0) {
            adev_post(render->adev, apts);
        }
    } while (sampnum > 0);
}

void render_video(void *hrender, AVFrame *video)
{
    if (!hrender) return;
    RENDER   *render  = (RENDER*)hrender;
    AVFrame   picture = {0};
    BYTE     *bmpbuf  = NULL;
    int       stride  = 0;

#if 0 // todo..
    int64_t *papts = NULL;
    int64_t *pvpts = NULL;
    vdev_getavpts(render->vdev, &papts, &pvpts);
    if (*papts - *pvpts > 200) {
        log_printf(TEXT("drop video\n"));
    }
#endif

    EnterCriticalSection(&render->cs);
    vdev_request(render->vdev, (void**)&bmpbuf, &stride);
    picture.data[0]     = bmpbuf;
    picture.linesize[0] = stride;
    sws_scale(render->pSWSContext, video->data, video->linesize, 0, render->nVideoHeight, picture.data, picture.linesize);
    vdev_post(render->vdev, video->pts);
    LeaveCriticalSection(&render->cs);
}

void render_setrect(void *hrender, int x, int y, int w, int h)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    EnterCriticalSection(&render->cs);
    if (render->nRenderWidth != w || render->nRenderHeight != h)
    {
        if (render->pSWSContext) {
            sws_freeContext(render->pSWSContext);
        }
        render->pSWSContext = sws_getContext(render->nVideoWidth, render->nVideoHeight, render->PixelFormat,
                                             w, h, AV_PIX_FMT_RGB32, SWS_BILINEAR, 0, 0, 0);
        render->nRenderWidth = w;
        render->nRenderHeight = h;
    }
    vdev_setrect(render->vdev, x, y, w, h);
    LeaveCriticalSection(&render->cs);
}

void render_start(void *hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    adev_pause(render->adev, FALSE);
    vdev_pause(render->vdev, FALSE);
}

void render_pause(void *hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    adev_pause(render->adev, TRUE);
    vdev_pause(render->vdev, TRUE);
}

void render_reset(void *hrender, DWORD sec)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    adev_reset(render->adev);
    vdev_reset(render->vdev);

    int64_t *papts = NULL;
    int64_t *pvpts = NULL;
    vdev_getavpts(render->vdev, &papts, &pvpts);
    *papts = *pvpts = sec * 1000;
}

void render_time(void *hrender, DWORD *time)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
    if (time) {
        int64_t *papts, *pvpts;
        vdev_getavpts(render->vdev, &papts, &pvpts);
        *time = (DWORD)((*papts > *pvpts ? *papts : *pvpts) / 1000);
    }
}



