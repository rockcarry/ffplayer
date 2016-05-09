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

    int            nVideoMode;
    int            nVideoWidth;
    int            nVideoHeight;
    int            nFramePeriod;
    AVRational     FrameRate;
    AVPixelFormat  PixelFormat;

    int            nAdevBufAvail;
    BYTE          *pAdevBufCur;
    AUDIOBUF      *pAdevHdrCur;

    int            nRenderXPosCur;
    int            nRenderYPosCur;
    int            nRenderXPosNew;
    int            nRenderYPosNew;
    int            nRenderWidthCur;
    int            nRenderHeightCur;
    int            nRenderWidthNew;
    int            nRenderHeightNew;
    int            nRenderSpeedCur;
    int            nRenderSpeedNew;

    #define RENDER_CLOSE (1 << 0)
    #define RENDER_PAUSE (1 << 1)
    int            nRenderStatus;
} RENDER;

// 内部函数实现
static void render_setspeed(RENDER *render, int speed)
{
    if (speed > 0)
    {
        render->nRenderSpeedNew = speed;
    }
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
    render->vdev = vdev_create(surface, 0, 0, 0, frate.num / frate.den);

    // make adev & vdev sync together
    int64_t *papts = NULL;
    vdev_getavpts(render->vdev, &papts, NULL);
    adev_syncapts(render->adev,  papts);

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

    if (!render->adev) return;
    do {
        if (render->nAdevBufAvail == 0) {
            adev_request(render->adev, &render->pAdevHdrCur);
            apts += render->nFramePeriod * render->nRenderSpeedCur / 100;
            render->nAdevBufAvail = (int  )render->pAdevHdrCur->size;
            render->pAdevBufCur   = (BYTE*)render->pAdevHdrCur->data;
        }

        if (render->nRenderSpeedCur != render->nRenderSpeedNew) {
            render->nRenderSpeedCur = render->nRenderSpeedNew;

            // set vdev frame rate
            int framerate = (render->FrameRate.num * render->nRenderSpeedCur) / (render->FrameRate.den * 100);
            vdev_setparam(render->vdev, PARAM_VDEV_FRAME_RATE, &framerate);

            //++ allocate & init swr context
            if (render->pSWRContext) {
                swr_free(&render->pSWRContext);
            }
            int samprate = 44100 * 100 / render->nRenderSpeedCur;
            render->pSWRContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, samprate,
                render->nChanLayout, render->SampleFormat, render->nSampleRate, 0, NULL);
            swr_init(render->pSWRContext);
            //-- allocate & init swr context
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
    RENDER  *render  = (RENDER*)hrender;
    AVFrame  picture = {0};
    BYTE    *bmpbuf;
    int      stride;
    int      pixfmt;

    do {
        if (  render->nRenderXPosCur  != render->nRenderXPosNew
           || render->nRenderYPosCur  != render->nRenderYPosNew
           || render->nRenderWidthCur != render->nRenderWidthNew
           || render->nRenderHeightCur!= render->nRenderHeightNew ) {
            render->nRenderXPosCur   = render->nRenderXPosNew;
            render->nRenderYPosCur   = render->nRenderYPosNew;
            render->nRenderWidthCur  = render->nRenderWidthNew;
            render->nRenderHeightCur = render->nRenderHeightNew;

            vdev_setrect(render->vdev, render->nRenderXPosCur, render->nRenderYPosCur,
                render->nRenderWidthCur, render->nRenderHeightCur);

            if (render->pSWSContext) sws_freeContext(render->pSWSContext);
            vdev_getparam(render->vdev, PARAM_VDEV_PIXEL_FORMAT, &pixfmt);
            render->pSWSContext = sws_getContext(
                render->nVideoWidth, render->nVideoHeight, render->PixelFormat,
                render->nRenderWidthCur, render->nRenderHeightCur, (AVPixelFormat)pixfmt,
                SWS_BILINEAR, 0, 0, 0);
        }

        vdev_request(render->vdev, (void**)&bmpbuf, &stride);
        if (video->pts != -1) {
            picture.data[0]     = bmpbuf;
            picture.linesize[0] = stride;
            sws_scale(render->pSWSContext, video->data, video->linesize, 0, render->nVideoHeight, picture.data, picture.linesize);
        }
        vdev_post(render->vdev, video->pts);
    } while (render->nRenderStatus & RENDER_PAUSE);
}

void render_setrect(void *hrender, int x, int y, int w, int h)
{
    RENDER *render = (RENDER*)hrender;
    render->nRenderXPosNew   = x;
    render->nRenderYPosNew   = y;
    render->nRenderWidthNew  = w > 1 ? w : 1;;
    render->nRenderHeightNew = h > 1 ? h : 1;;
}

void render_start(void *hrender)
{
    RENDER *render = (RENDER*)hrender;
    render->nRenderStatus &=~RENDER_PAUSE;
    adev_pause(render->adev, FALSE);
    vdev_pause(render->vdev, FALSE);
}

void render_pause(void *hrender)
{
    RENDER *render = (RENDER*)hrender;
    render->nRenderStatus |= RENDER_PAUSE;
    adev_pause(render->adev, TRUE);
    vdev_pause(render->vdev, TRUE);
}

void render_reset(void *hrender)
{
    RENDER *render = (RENDER*)hrender;
    adev_reset(render->adev);
    vdev_reset(render->vdev);
}

void render_setparam(void *hrender, DWORD id, void *param)
{
    RENDER *render = (RENDER*)hrender;
    switch (id)
    {
    case PARAM_VIDEO_MODE:
        render->nVideoMode = *(int*)param;
        break;
    case PARAM_AUDIO_VOLUME:
        adev_setparam(render->adev, PARAM_AUDIO_VOLUME, param);
        break;
    case PARAM_VIDEO_POSITION:
        if (*(int64_t*)param)
        {
            int64_t *papts = NULL;
            int64_t *pvpts = NULL;
            vdev_getavpts(render->vdev, &papts, &pvpts);
            if (render->adev) *papts = *(int64_t*)param;
            if (render->vdev) *pvpts = *(int64_t*)param;
        }
        break;
    case PARAM_PLAYER_SPEED:
        render_setspeed(render, *(int*)param);
        break;
    }
}

void render_getparam(void *hrender, DWORD id, void *param)
{
    RENDER *render = (RENDER*)hrender;
    switch (id)
    {
    case PARAM_VIDEO_MODE:
        *(int*)param = render->nVideoMode;
        break;
    case PARAM_AUDIO_VOLUME:
        adev_getparam(render->adev, PARAM_AUDIO_VOLUME, param);
        break;
    case PARAM_VIDEO_POSITION:
        {
            int64_t *papts, *pvpts;
            vdev_getavpts(render->vdev, &papts, &pvpts);
            *(int64_t*)param = *papts > *pvpts ? *papts : *pvpts;
        }
        break;
    case PARAM_PLAYER_SPEED:
        *(int*)param = render->nRenderSpeedCur;
        break;
    }
}

int render_slowflag(void *hrender)
{
    RENDER *render = (RENDER*)hrender;
    int aflag; adev_getparam(render->adev, PARAM_ADEV_SLOW_FLAG, &aflag);
    int vflag; vdev_getparam(render->vdev, PARAM_VDEV_SLOW_FLAG, &vflag);
    if (aflag > 0 || vflag > 0) return  1;
    if (aflag < 0 && vflag < 0) return -1;
    return 0;
}

