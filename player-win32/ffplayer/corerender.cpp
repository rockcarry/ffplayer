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
    void          *adev;
    void          *vdev;

    SwrContext    *pSWRContext;
    SwsContext    *pSWSContext;

    int            nSampleRate;
    AVSampleFormat SampleFormat;
    int64_t        nChanLayout;

    int            nVideoWidth;
    int            nVideoHeight;
    int            nFramePeriod;
    AVRational     FrameRate;
    AVPixelFormat  PixelFormat;

    int            nAdevBufAvail;
    BYTE          *pAdevBufCur;
    AUDIOBUF      *pAdevHdrCur;

    int            nRenderWidth;
    int            nRenderHeight;
    int            nRenderSpeed;

    CRITICAL_SECTION  cs1;
    CRITICAL_SECTION  cs2;
} RENDER;

// 内部函数实现
static void render_setspeed(RENDER *render, int speed)
{
    EnterCriticalSection(&render->cs1);
    if (render->nRenderSpeed != speed) {
        int samprate  = 44100 * 100 / speed;
        int framerate = (render->FrameRate.num * speed) / (render->FrameRate.den * 100);

        if (render->pSWRContext) {
            swr_free(&render->pSWRContext);
        }

        /* allocate & init swr context */
        render->pSWRContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, samprate,
            render->nChanLayout, render->SampleFormat, render->nSampleRate, 0, NULL);
        swr_init(render->pSWRContext);

        // set vdev frame rate
        vdev_setfrate(render->vdev, framerate);

        render->nRenderSpeed = speed;
    }
    LeaveCriticalSection(&render->cs1);
}

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

    // init for video
    render->nVideoWidth  = w;
    render->nVideoHeight = h;
    render->nFramePeriod = 1000 * frate.den / frate.num;
    render->FrameRate    = frate;
    render->PixelFormat  = (AVPixelFormat)pixfmt;

    // init for audio
    render->nSampleRate  = srate;
    render->SampleFormat = sndfmt;
    render->nChanLayout  = ch_layout;

    // create adev & vdev
    render->adev = adev_create(0, (int)(44100.0 * frate.den / frate.num + 0.5) * 4);
    render->vdev = vdev_create(surface, 0, render->nRenderWidth, render->nRenderHeight, frate.num / frate.den);

    // make adev & vdev sync together
    int64_t *papts = NULL;
    vdev_getavpts(render->vdev, &papts, NULL);
    adev_syncapts(render->adev,  papts);

    InitializeCriticalSection(&render->cs1);
    InitializeCriticalSection(&render->cs2);
    RECT rect; GetClientRect((HWND)surface, &rect);
    render_setrect (render, rect.left, rect.top, rect.right, rect.bottom);
    render_setspeed(render, 100);

    return render;
}

void render_close(void *hrender)
{
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
    RENDER *render  = (RENDER*)hrender;
    int     sampnum = 0;
    DWORD   apts    = (DWORD)audio->pts;

    EnterCriticalSection(&render->cs1);
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
    LeaveCriticalSection(&render->cs1);
}

void render_video(void *hrender, AVFrame *video)
{
    RENDER  *render  = (RENDER*)hrender;
    AVFrame  picture = {0};
    BYTE    *bmpbuf  = NULL;
    int      stride  = 0;

#if 0 // todo..
    int64_t *papts = NULL;
    int64_t *pvpts = NULL;
    vdev_getavpts(render->vdev, &papts, &pvpts);
    if (*papts - *pvpts > 200) {
        log_printf(TEXT("drop video\n"));
    }
#endif

    vdev_request(render->vdev, (void**)&bmpbuf, &stride);
    EnterCriticalSection(&render->cs2);
    picture.data[0]     = bmpbuf;
    picture.linesize[0] = stride;
    sws_scale(render->pSWSContext, video->data, video->linesize, 0, render->nVideoHeight, picture.data, picture.linesize);
    LeaveCriticalSection(&render->cs2);
    vdev_post(render->vdev, video->pts);
}

void render_setrect(void *hrender, int x, int y, int w, int h)
{
    RENDER *render = (RENDER*)hrender;
    EnterCriticalSection(&render->cs2);
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
    LeaveCriticalSection(&render->cs2);
}

void render_start(void *hrender)
{
    RENDER *render = (RENDER*)hrender;
    adev_pause(render->adev, FALSE);
    vdev_pause(render->vdev, FALSE);
}

void render_pause(void *hrender)
{
    RENDER *render = (RENDER*)hrender;
    adev_pause(render->adev, TRUE);
    vdev_pause(render->vdev, TRUE);
}

void render_reset(void *hrender)
{
    RENDER *render = (RENDER*)hrender;
    adev_reset(render->adev);
    vdev_reset(render->vdev);
}

void render_time(void *hrender, int64_t *time)
{
    RENDER *render = (RENDER*)hrender;
    if (time) {
        int64_t *papts, *pvpts;
        vdev_getavpts(render->vdev, &papts, &pvpts);
        *time = *papts > *pvpts ? *papts : *pvpts;
    }
}

void render_setparam(void *hrender, DWORD id, void *param)
{
    RENDER *render = (RENDER*)hrender;
    switch (id)
    {
    case PARAM_RENDER_TIME:
        {
            int64_t *papts = NULL;
            int64_t *pvpts = NULL;
            vdev_getavpts(render->vdev, &papts, &pvpts);
            *papts = *pvpts = *(int64_t*)param;
        }
        break;
    case PARAM_RENDER_SPEED:
        render_setspeed(render, *(int*)param);
        break;
    }
}

void render_getparam(void *hrender, DWORD id, void *param)
{
    RENDER *render = (RENDER*)hrender;
    switch (id)
    {
    case PARAM_RENDER_TIME:
        {
            int64_t *papts, *pvpts;
            vdev_getavpts(render->vdev, &papts, &pvpts);
            *(int64_t*)param = *papts > *pvpts ? *papts : *pvpts;
        }
        break;
    case PARAM_RENDER_SPEED:
        *(int*)param = render->nRenderSpeed;
        break;
    }
}


