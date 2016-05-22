// 包含头文件
#include "vdev.h"
#include "log.h"

extern "C" {
#include "libavformat/avformat.h"
}

// 内部常量定义
#define DEF_VDEV_BUF_NUM  3

// 内部类型定义
typedef struct
{
    // common members
    VDEV_COMMON_MEMBERS

    HDC      hdcsrc;
    HDC      hdcdst;
    HBITMAP *hbitmaps;
    BYTE   **pbmpbufs;
} VDEVGDICTXT;

// 内部函数实现
static DWORD WINAPI VideoRenderThreadProc(void *param)
{
    VDEVGDICTXT *c = (VDEVGDICTXT*)param;

    while (!(c->nStatus & VDEV_CLOSE))
    {
        int ret = WaitForSingleObject(c->semr, c->tickframe);
        if (ret != WAIT_OBJECT_0) continue;

        if (c->refresh_flag) {
            c->refresh_flag = 0;
            vdev_refresh_background(c);
        }

        int64_t apts = c->apts;
        int64_t vpts = c->vpts = c->ppts[c->head];
#if CLEAR_VDEV_WHEN_COMPLETED
        if (vpts != -1 && !(c->nStatus & VDEV_COMPLETED)) {
#else
        if (vpts != -1) {
#endif
            SelectObject(c->hdcsrc, c->hbitmaps[c->head]);
            BitBlt(c->hdcdst, c->x, c->y, c->w, c->h, c->hdcsrc, 0, 0, SRCCOPY);
        }

//      log_printf(TEXT("vpts: %lld\n"), vpts);
        if (++c->head == c->bufnum) c->head = 0;
        ReleaseSemaphore(c->semw, 1, NULL);

        if (!(c->nStatus & (VDEV_PAUSE|VDEV_COMPLETED))) {
            // send play progress event
            vdev_player_event(c, PLAY_PROGRESS, c->vpts > c->apts ? c->vpts : c->apts);

            //++ play completed ++//
            if (c->completed_apts != c->apts || c->completed_vpts != c->vpts) {
                c->completed_apts = c->apts;
                c->completed_vpts = c->vpts;
                c->completed_counter = 0;
            }
            else if (++c->completed_counter == 50) {
                log_printf(TEXT("play completed !\n"));
                c->nStatus |= VDEV_COMPLETED;
                vdev_player_event(c, PLAY_COMPLETED, 0);

#if CLEAR_VDEV_WHEN_COMPLETED
                InvalidateRect(c->hwnd, NULL, TRUE);
#endif
            }
            //-- play completed --//

            //++ frame rate & av sync control ++//
            DWORD tickcur  = GetTickCount();
            int   tickdiff = tickcur - c->ticklast;
            c->ticklast = tickcur;
            if (tickdiff - c->tickframe >  2) c->ticksleep--;
            if (tickdiff - c->tickframe < -2) c->ticksleep++;
            if (apts != -1 && vpts != -1) {
                if (apts - vpts >  5) c->ticksleep-=2;
                if (apts - vpts < -5) c->ticksleep+=2;
            }
            if (c->ticksleep > 0) Sleep(c->ticksleep);
            log_printf(TEXT("d: %3lld, s: %d\n"), apts-vpts, c->ticksleep);
            //-- frame rate & av sync control --//
        }
        else Sleep(c->tickframe);
    }

    return NULL;
}

// 接口函数实现
void* vdev_gdi_create(void *surface, int bufnum, int w, int h, int frate)
{
    VDEVGDICTXT *ctxt = (VDEVGDICTXT*)malloc(sizeof(VDEVGDICTXT));
    if (!ctxt) {
        log_printf(TEXT("failed to allocate gdi vdev context !\n"));
        exit(0);
    }

    // init vdev context
    memset(ctxt, 0, sizeof(VDEVGDICTXT));
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->hwnd      = (HWND)surface;
    ctxt->bufnum    = bufnum;
    ctxt->pixfmt    = AV_PIX_FMT_RGB32;
    ctxt->w         = w;
    ctxt->h         = h;
    ctxt->tickframe = 1000 / frate;
    ctxt->ticksleep = ctxt->tickframe;
    ctxt->apts      = -1;
    ctxt->vpts      = -1;

    // alloc buffer & semaphore
    ctxt->ppts     = (int64_t*)malloc(bufnum * sizeof(int64_t));
    ctxt->hbitmaps = (HBITMAP*)malloc(bufnum * sizeof(HBITMAP));
    ctxt->pbmpbufs = (BYTE**  )malloc(bufnum * sizeof(BYTE*  ));

    memset(ctxt->ppts    , 0, bufnum * sizeof(int64_t));
    memset(ctxt->hbitmaps, 0, bufnum * sizeof(HBITMAP));

    ctxt->semr     = CreateSemaphore(NULL, 0     , bufnum, NULL);
    ctxt->semw     = CreateSemaphore(NULL, bufnum, bufnum, NULL);

    ctxt->hdcdst = GetDC(ctxt->hwnd);
    ctxt->hdcsrc = CreateCompatibleDC(ctxt->hdcdst);
    if (!ctxt->ppts || !ctxt->hbitmaps || !ctxt->pbmpbufs || !ctxt->semr || !ctxt->semw || !ctxt->hdcdst || !ctxt->hdcsrc) {
        log_printf(TEXT("failed to allocate resources for vdev-gdi !\n"));
        exit(0);
    }

    // create video rendering thread
    ctxt->hThread = CreateThread(NULL, 0, VideoRenderThreadProc, ctxt, 0, NULL);
    return ctxt;
}

void vdev_gdi_destroy(void *ctxt)
{
    int i;
    VDEVGDICTXT *c = (VDEVGDICTXT*)ctxt;

    // make visual effect rendering thread safely exit
    c->nStatus = VDEV_CLOSE;
    WaitForSingleObject(c->hThread, 100);
    CloseHandle(c->hThread);

    //++ for video
    DeleteDC (c->hdcsrc);
    ReleaseDC(c->hwnd, c->hdcdst);
    for (i=0; i<c->bufnum; i++) {
        if (c->hbitmaps[i]) {
            DeleteObject(c->hbitmaps[i]);
        }
    }
    //-- for video

    // close semaphore
    CloseHandle(c->semr);
    CloseHandle(c->semw);

#if CLEAR_VDEV_WHEN_DESTROYED
    // clear window to background
    InvalidateRect(c->hwnd, NULL, TRUE);
#endif

    // free memory
    free(c->ppts    );
    free(c->hbitmaps);
    free(c->pbmpbufs);
    free(c);
}

void vdev_gdi_request(void *ctxt, void **buffer, int *stride)
{
    VDEVGDICTXT *c = (VDEVGDICTXT*)ctxt;

    WaitForSingleObject(c->semw, -1);

    BITMAP bitmap;
    int bmpw = 0;
    int bmph = 0;
    if (c->hbitmaps[c->tail]) {
        GetObject(c->hbitmaps[c->tail], sizeof(BITMAP), &bitmap);
        bmpw = bitmap.bmWidth ;
        bmph = bitmap.bmHeight;
    }

    if (bmpw != c->w || bmph != c->h) {
        if (c->hbitmaps[c->tail]) {
            DeleteObject(c->hbitmaps[c->tail]);
        }

        BITMAPINFO bmpinfo = {0};
        bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
        bmpinfo.bmiHeader.biWidth       =  c->w;
        bmpinfo.bmiHeader.biHeight      = -c->h;
        bmpinfo.bmiHeader.biPlanes      =  1;
        bmpinfo.bmiHeader.biBitCount    =  32;
        bmpinfo.bmiHeader.biCompression =  BI_RGB;
        c->hbitmaps[c->tail] = CreateDIBSection(c->hdcsrc, &bmpinfo, DIB_RGB_COLORS,
                                        (void**)&c->pbmpbufs[c->tail], NULL, 0);
        GetObject(c->hbitmaps[c->tail], sizeof(BITMAP), &bitmap);
    }

    if (buffer) *buffer = c->pbmpbufs[c->tail];
    if (stride) *stride = bitmap.bmWidthBytes ;
}

void vdev_gdi_post(void *ctxt, int64_t pts)
{
    VDEVGDICTXT *c = (VDEVGDICTXT*)ctxt;
    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    ReleaseSemaphore(c->semr, 1, NULL);
}

void vdev_gdi_setrect(void *ctxt, int x, int y, int w, int h)
{
    VDEV_COMMON_CTXT *c = (VDEV_COMMON_CTXT*)ctxt;
    c->x  = x; c->y  = y;
    c->w  = w; c->h  = h;
    c->sw = w; c->sh = h;
    c->refresh_flag  = 1;
}
