// 包含头文件
#include <pthread.h>
#include <unistd.h>
#include <time.h>
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
    int               nVideoWidth;
    int               nVideoHeight;
    int               iPixelFormat;
    SwrContext       *pSWRContext;
    SwsContext       *pSWSContext;

    #define RS_PAUSE  (1 << 0)
    #define RS_SEEK   (1 << 1)
    #define RS_CLOSE  (1 << 2)
    int               nRenderStatus;

    // audio
    WAVQUEUE          WavQueue;

    // video
    int               nRenderWidth;
    int               nRenderHeight;
    sp<ANativeWindow> pRenderWin;
    BMPQUEUE          BmpQueue;
    pthread_t         hVideoThread;

    unsigned long     ulCurTick;
    unsigned long     ulLastTick;
    int               iFrameTick;
    int               iSleepTick;

    int64_t           i64CurTimeA;
    int64_t           i64CurTimeV;
} RENDER;

// 内部函数实现
static unsigned long GetTickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void* VideoRenderThreadProc(void *param)
{
    RENDER *render = (RENDER*)param;

    while (!(render->nRenderStatus & RS_CLOSE))
    {
        if (render->nRenderStatus & RS_PAUSE) {
            usleep(20*1000);
            continue;
        }
        ANativeWindowBuffer *buf  = NULL;
        int64_t             *ppts = NULL;
        bmpqueue_read_request(&(render->BmpQueue), &ppts, &buf);
        if (!(render->nRenderStatus & RS_SEEK)) {
            render->i64CurTimeV = *ppts; // the current play time
            render->pRenderWin->queueBuffer(render->pRenderWin.get(), buf, -1);
        }
        bmpqueue_read_done(&(render->BmpQueue));

        // ++ frame rate control ++ //
        render->ulLastTick = render->ulCurTick;
        render->ulCurTick  = GetTickCount();
        int64_t diff = render->ulCurTick - render->ulLastTick;
        if (diff > render->iFrameTick) {
            render->iSleepTick--;
        }
        else if (diff < render->iFrameTick) {
            render->iSleepTick++;
        }
        // -- frame rate control -- //

        // ++ av sync control ++ //
        diff = render->i64CurTimeV - render->i64CurTimeA;
        if ((diff > 0 ? diff : -diff) < 60000) {
            if (diff >  5) render->iSleepTick++;
            if (diff < -5) render->iSleepTick--;
        }
        // -- av sync control -- //

        //++ for seek ++//
        if (render->nRenderStatus & RS_SEEK) {
            render->iSleepTick = render->iFrameTick;
        }
        //-- for seek --//

        // do sleep
        if (render->iSleepTick > 0) usleep(render->iSleepTick * 1000);

        ALOGD("i64CurTimeV = %lld", render->i64CurTimeV);
        ALOGD("%lld, %d", diff, render->iSleepTick);
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
    render->pRenderWin = win; // save win

    /* allocate & init swr context */
    render->pSWRContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
                                             ch_layout, (AVSampleFormat)sndfmt, srate, 0, NULL);
    swr_init(render->pSWRContext);

    // init for video
    render->nVideoWidth  = vw;
    render->nVideoHeight = vh;
    render->nRenderWidth = ww;
    render->nRenderHeight= wh;
    render->iPixelFormat = pixfmt;

    // create sws context
    render->pSWSContext = sws_getContext(
        render->nVideoWidth, render->nVideoHeight, (AVPixelFormat)render->iPixelFormat,
        render->nRenderWidth, render->nRenderHeight, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, 0, 0, 0);

    render->iFrameTick = 1000 * frate.den / frate.num;
    render->iSleepTick = render->iFrameTick;

    // create bmp queue
    bmpqueue_create(&(render->BmpQueue), win, render->nRenderWidth, render->nRenderHeight);

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
        bmpqueue_write_request(&(render->BmpQueue), NULL, NULL, NULL);
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
    AVPicture picture = {{0}, {0}};
    uint8_t  *buffer  = NULL;
    int       stride  = 0;
    int64_t  *ppts    = NULL;

    if (render->nRenderStatus & RS_SEEK) return;

    bmpqueue_write_request(&(render->BmpQueue), &ppts, &buffer, &stride);
    *ppts               = video->pts;
    picture.data[0]     = buffer;
    picture.linesize[0] = stride;
    sws_scale(render->pSWSContext, video->data, video->linesize, 0, render->nVideoHeight, picture.data, picture.linesize);
    bmpqueue_write_done(&(render->BmpQueue));
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

