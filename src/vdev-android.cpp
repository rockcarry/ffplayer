// 包含头文件
#include <gui/Surface.h>
#include <ui/GraphicBufferMapper.h>
#include <android_runtime/android_view_Surface.h>
#include "vdev.h"

extern "C" {
#include "libavformat/avformat.h"
}

using namespace android;

// for jni
JNIEXPORT JavaVM* get_jni_jvm(void);
JNIEXPORT JNIEnv* get_jni_env(void);

// 内部常量定义
#define DEF_VDEV_BUF_NUM    3
#define DEF_WIN_PIX_FMT     HAL_PIXEL_FORMAT_YCrCb_420_SP // HAL_PIXEL_FORMAT_RGBX_8888 or HAL_PIXEL_FORMAT_RGB_565 or HAL_PIXEL_FORMAT_YCrCb_420_SP or HAL_PIXEL_FORMAT_YV12
#define VDEV_GRALLOC_USAGE  (GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_NEVER | GRALLOC_USAGE_HW_TEXTURE)

// 内部类型定义
typedef struct
{
    // common members
    VDEV_COMMON_MEMBERS

    // android natvie window
    sp<ANativeWindow> wincur;
    sp<ANativeWindow> winnew;

    // android natvie window buffer
    ANativeWindowBuffer **bufs;
} VDEVCTXT;

// 内部函数实现
inline int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

inline int android_pixfmt_to_ffmpeg_pixfmt(int srcfmt)
{
    // dst fmt
    int dst_fmt = 0;
    switch (srcfmt) {
    case HAL_PIXEL_FORMAT_RGB_565:      dst_fmt = AV_PIX_FMT_RGB565;  break;
    case HAL_PIXEL_FORMAT_RGBX_8888:    dst_fmt = AV_PIX_FMT_BGR32;   break;
    case HAL_PIXEL_FORMAT_YV12:         dst_fmt = AV_PIX_FMT_YUV420P; break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP: dst_fmt = AV_PIX_FMT_NV21;    break;
    }
    return dst_fmt;
}

static void* video_render_thread_proc(void *param)
{
    VDEVCTXT *c = (VDEVCTXT*)param;

    while (1) {
        sem_wait(&c->semr);
        if (c->status & VDEV_CLOSE) break;

        int64_t vpts = c->vpts = c->ppts[c->head];
        if (vpts != -1) {
            if (c->bufs[c->head] && c->wincur.get()) c->wincur->queueBuffer (c->wincur.get(), c->bufs[c->head], -1);
        } else {
            if (c->bufs[c->head] && c->wincur.get()) c->wincur->cancelBuffer(c->wincur.get(), c->bufs[c->head], -1);
        }
        // set to null
        c->bufs[c->head] = NULL;

        av_log(NULL, AV_LOG_DEBUG, "vpts: %lld\n", vpts);
        if (++c->head == c->bufnum) c->head = 0;
        sem_post(&c->semw);

        // handle av-sync & frame rate & complete
        vdev_avsync_and_complete(c);
    }

    // need call DetachCurrentThread
    get_jni_jvm()->DetachCurrentThread();
    return NULL;
}

static void vdev_android_lock(void *ctxt, uint8_t *buffer[8], int linesize[8])
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;

    sem_wait(&c->semw);
    ANativeWindowBuffer *buf = NULL;
    uint8_t             *dst = NULL;

    if (c->wincur.get() != c->winnew.get()) {
        c->wincur = c->winnew;
        if (c->winnew.get()) {
            native_window_set_usage             (c->winnew.get(), VDEV_GRALLOC_USAGE);
            native_window_set_buffer_count      (c->winnew.get(), c->bufnum + 1);
            native_window_set_buffers_format    (c->winnew.get(), DEF_WIN_PIX_FMT);
            native_window_set_buffers_dimensions(c->winnew.get(), c->sw, c->sh);
            native_window_set_scaling_mode      (c->winnew.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
        }
    }
    if (!c->wincur.get()) return;

    if (0 == native_window_dequeue_buffer_and_wait(c->wincur.get(), &buf)) {
        Rect bounds(buf->width, buf->height);
        if (0 != GraphicBufferMapper::get().lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, (void**)&dst)) {
            av_log(NULL, AV_LOG_WARNING, "ANativeWindow failed to lock buffer !\n");
        } else {
            c->bufs[c->tail] = buf;
        }
    } else {
        av_log(NULL, AV_LOG_WARNING, "ANativeWindow failed to dequeue buffer !\n");
    }

    int yheight = buf->height;
    int uheight = yheight / 2;
    int ystride = buf->stride;
    int ustride = ALIGN(ystride/2, 16);

    switch (c->pixfmt) {
    case AV_PIX_FMT_YUV420P:
        buffer  [0] = dst;
        buffer  [2] = dst + ystride * yheight;
        buffer  [1] = dst + ystride * yheight + ustride * uheight;
        linesize[0] = ystride;
        linesize[1] = ustride;
        linesize[2] = ustride;
        break;
    case AV_PIX_FMT_NV21:
        buffer  [0] = dst;
        buffer  [1] = dst + ystride * yheight;
        linesize[0] = ystride;
        linesize[1] = ystride;
        break;
    case AV_PIX_FMT_BGR32:
        buffer  [0] = dst;
        linesize[0] = buf->stride * 4;
        break;
    case AV_PIX_FMT_RGB565:
        buffer  [0] = dst;
        linesize[0] = buf->stride * 2;
        break;
    }
}

static void vdev_android_unlock(void *ctxt, int64_t pts)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;

    ANativeWindowBuffer *buf = c->bufs[c->tail];
    if (buf) GraphicBufferMapper::get().unlock(buf->handle);

    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    sem_post(&c->semr);
}

static void vdev_android_destroy(void *ctxt)
{
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    JNIEnv *env = get_jni_env();

    // make visual effect & rendering thread safely exit
    c->status = VDEV_CLOSE;
    sem_post(&c->semr);
    pthread_join(c->thread, NULL);

    // close semaphore
    sem_destroy(&c->semr);
    sem_destroy(&c->semw);

    // free memory
    free(c->ppts);
    free(c->bufs);
    delete(c);
}

// 接口函数实现
void* vdev_android_create(void *surface, int bufnum, int w, int h, int frate)
{
    VDEVCTXT *ctxt = new VDEVCTXT();
    if (!ctxt) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocate vdev context !\n");
        exit(0);
    }

    // init vdev context
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->surface   = surface;
    ctxt->bufnum    = bufnum;
    ctxt->pixfmt    = android_pixfmt_to_ffmpeg_pixfmt(DEF_WIN_PIX_FMT);
    ctxt->w         = w ? w : 1;
    ctxt->h         = h ? h : 1;
    ctxt->sw        = w ? w : 1;
    ctxt->sh        = h ? h : 1;
    ctxt->tickframe = 1000 / frate;
    ctxt->ticksleep = ctxt->tickframe;
    ctxt->apts      = -1;
    ctxt->vpts      = -1;
    ctxt->tickavdiff= -ctxt->tickframe * 4; // 4 should equals to (DEF_ADEV_BUF_NUM - 1)
    ctxt->lock      = vdev_android_lock;
    ctxt->unlock    = vdev_android_unlock;
    ctxt->destroy   = vdev_android_destroy;

    // alloc buffer & semaphore
    ctxt->ppts = (int64_t*)calloc(bufnum, sizeof(int64_t));
    ctxt->bufs = (ANativeWindowBuffer**)calloc(bufnum, sizeof(ANativeWindowBuffer*));

    // create semaphore
    sem_init(&ctxt->semr, 0, 0     );
    sem_init(&ctxt->semw, 0, bufnum);

    // create video rendering thread
    pthread_create(&ctxt->thread, NULL, video_render_thread_proc, ctxt);
    return ctxt;
}

void vdev_android_setwindow(void *ctxt, jobject surface)
{
    if (!ctxt) return;
    VDEVCTXT *c = (VDEVCTXT*)ctxt;
    JNIEnv *env = get_jni_env();
    c->winnew   = surface ? android_view_Surface_getSurface(env, surface) : NULL;
}

