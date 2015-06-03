// 包含头文件
#include <pthread.h>
#include <unistd.h>
#include "wavqueue.h"
#include "bmpqueue.h"
#include "corerender.h"

extern "C" {
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

// 内部类型定义
typedef struct
{
    int           nVideoWidth;
    int           nVideoHeight;
    AVPixelFormat nPixelFormat;
    SwrContext   *pSWRContext;
    SwsContext   *pSWSContext;

    WAVQUEUE      WavQueue;

    #define RS_PAUSE  (1 << 0)
    #define RS_SEEK   (1 << 1)
    #define RS_CLOSE  (1 << 2)
    int           nRenderWidth;
    int           nRenderHeight;
    int           nRenderStatus;
    BMPQUEUE      BmpQueue;
    pthread_t     hVideoThread;

    int64_t       i64CurTimeA;
    int64_t       i64CurTimeV;
} RENDER;

// 内部函数实现
static void* VideoRenderThreadProc(void *param)
{
    RENDER *render = (RENDER*)param;

    while (!(render->nRenderStatus & RS_CLOSE))
    {
        if (render->nRenderStatus & RS_PAUSE) {
            usleep(20*1000);
            continue;
        }
    }

    return NULL;
}

// 函数实现
void* renderopen(sp<ANativeWindow> win, int ww, int wh,
                 AVRational frate, int pixfmt, int vw, int vh,
                 int srate, int sndfmt, int64_t ch_layout)
{
    RENDER *render = (RENDER*)malloc(sizeof(RENDER));
    memset(render, 0, sizeof(RENDER));

    /* allocate & init swr context */
    render->pSWRContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
                                             ch_layout, (AVSampleFormat)sndfmt, srate, 0, NULL);
    swr_init(render->pSWRContext);

    // init for video
    render->nVideoWidth  = vw;
    render->nVideoHeight = vh;
    render->nRenderWidth = ww;
    render->nRenderHeight= wh;
    render->nPixelFormat = (PixelFormat)pixfmt;

    // create sws context
    render->pSWSContext = sws_getContext(
        render->nVideoWidth,
        render->nVideoHeight,
        render->nPixelFormat,
        render->nRenderWidth,
        render->nRenderHeight,
        PIX_FMT_RGB32,
        SWS_BILINEAR,
        0, 0, 0);

    // create bmp queue
    bmpqueue_create(&(render->BmpQueue), win);

    render->nRenderStatus = 0;
    pthread_create(&(render->hVideoThread), NULL, VideoRenderThreadProc, render);
    return render;
}

void renderclose(void *hrender)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;

    // free swr context
    swr_free(&(render->pSWRContext));
    //-- audio --//

    //++ video ++//
    // set nRenderStatus to RS_CLOSE
    render->nRenderStatus |= RS_CLOSE;
    if (bmpqueue_isempty(&(render->BmpQueue))) {
        bmpqueue_write_request(&(render->BmpQueue), NULL);
        bmpqueue_write_done   (&(render->BmpQueue));
    }

    // wait for video rendering thread exit
    pthread_join(render->hVideoThread, NULL);

    // free sws context
    if (render->pSWSContext) sws_freeContext(render->pSWSContext);

    // destroy bmp queue
    bmpqueue_destroy(&(render->BmpQueue));
    //-- video --//

    // free context
    free(render);
}

void renderaudiowrite(void *hrender, AVFrame *audio)
{
    if (!hrender) return;
    RENDER  *render  = (RENDER*)hrender;

    if (render->nRenderStatus & RS_SEEK) return;
}

void rendervideowrite(void *hrender, AVFrame *video)
{
    if (!hrender) return;
    RENDER   *render  = (RENDER*)hrender;

    if (render->nRenderStatus & RS_SEEK) return;
}

void renderstart(void *hrender)
{
    if (!hrender) return;
}

void renderpause(void *hrender)
{
    if (!hrender) return;
}

void renderseek(void *hrender, int sec)
{
    if (!hrender) return;
    RENDER *render = (RENDER*)hrender;
}

int rendertime(void *hrender)
{
    if (!hrender) return 0;
    RENDER *render = (RENDER*)hrender;
    int atime = render->i64CurTimeA > 0 ? (int)(render->i64CurTimeA / 1000) : 0;
    int vtime = render->i64CurTimeV > 0 ? (int)(render->i64CurTimeV / 1000) : 0;
    return (atime > vtime ? atime : vtime);
}

