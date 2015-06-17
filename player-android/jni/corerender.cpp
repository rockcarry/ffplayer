#define LOG_TAG "ffplayer_render"

// 包含头文件
#include <pthread.h>
#include <utils/Log.h>
#include <ui/GraphicBufferMapper.h>
#include "corerender.h"

extern "C" {
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

// 内部常量定义
#define DEF_VIDEO_BUFFER_COUNT  3

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

    // video
    int               nRenderWidth;
    int               nRenderHeight;
    sp<ANativeWindow> pRenderWin;
    pthread_t         hVideoThread;

    unsigned long     ulCurTick;
    unsigned long     ulLastTick;
    int               iFrameTick;
    int               iSleepTick;

    int64_t           i64CurTimeA;
    int64_t           i64CurTimeV;
} RENDER;

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
        render->nRenderWidth, render->nRenderHeight, AV_PIX_FMT_RGB0,
        SWS_BILINEAR, 0, 0, 0);

    render->iFrameTick = 1000 * frate.den / frate.num;
    render->iSleepTick = render->iFrameTick;

    native_window_set_usage(win.get(),
        GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);
    native_window_set_scaling_mode(win.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    native_window_set_buffers_geometry(win.get(), ww, wh, HAL_PIXEL_FORMAT_RGBX_8888);
    native_window_set_buffer_count(win.get(), DEF_VIDEO_BUFFER_COUNT);

    render->nRenderStatus = 0;
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

    // wait for video rendering thread exit
    pthread_join(render->hVideoThread, NULL);

    // free sws context
    if (render->pSWSContext) sws_freeContext(render->pSWSContext);
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

    if (render->nRenderStatus & RS_SEEK) return;

    ANativeWindowBuffer *buf;
    if (0 == native_window_dequeue_buffer_and_wait(render->pRenderWin.get(), &buf))
    {
        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
        Rect bounds(render->nRenderWidth, render->nRenderHeight);
		void *dst = NULL;
        if (0 == mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst)) {
            picture.data[0]     = (uint8_t*)dst;
            picture.linesize[0] = buf->stride * 4;
            sws_scale(render->pSWSContext, video->data, video->linesize, 0, render->nVideoHeight, picture.data, picture.linesize);
            mapper.unlock(buf->handle);
        }
        render->pRenderWin->queueBuffer(render->pRenderWin.get(), buf, -1);
    }
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

