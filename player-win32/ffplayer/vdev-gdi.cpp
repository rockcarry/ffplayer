// 包含头文件
#include "coreplayer.h"
#include "vdev.h"
#include "log.h"

// 内部常量定义
#define DEF_VDEV_BUF_NUM  3

// 内部类型定义
typedef struct
{
    int      bufnum;
    int      x;
    int      y;
    int      w;
    int      h;

    HWND     hwnd;
    HDC      hdcsrc;
    HDC      hdcdst;
    HBITMAP *hbitmaps;
    BYTE   **pbmpbufs;
    int64_t *ppts;
    int64_t  apts;
    int64_t  vpts;

    int      head;
    int      tail;
    HANDLE   semr;
    HANDLE   semw;

    int      tickframe;
    int      ticksleep;
    int      ticklast;

    #define DEVGDI_CLOSE  (1 << 0)
    #define DEVGDI_PAUSE  (1 << 1)
    int      nStatus;
    HANDLE   hThread;

    int      completed_counter;
    int64_t  completed_apts;
    int64_t  completed_vpts;
    int      refresh_flag;
} DEVGDICTXT;

// 内部函数实现
static void RefreshWindowBackground(HWND hwnd, int x, int y, int w, int h)
{
    RECT rtwin, rect1, rect2, rect3, rect4;
    GetClientRect(hwnd, &rtwin);
    rect1.left = 0;   rect1.top = 0;   rect1.right = rtwin.right; rect1.bottom = y;
    rect2.left = 0;   rect2.top = y;   rect2.right = x;           rect2.bottom = y+h;
    rect3.left = x+w; rect3.top = y;   rect3.right = rtwin.right; rect3.bottom = y+h;
    rect4.left = 0;   rect4.top = y+h; rect4.right = rtwin.right; rect4.bottom = rtwin.bottom;
    InvalidateRect(hwnd, &rect1, TRUE);
    InvalidateRect(hwnd, &rect2, TRUE);
    InvalidateRect(hwnd, &rect3, TRUE);
    InvalidateRect(hwnd, &rect4, TRUE);
}

static DWORD WINAPI VideoRenderThreadProc(void *param)
{
    DEVGDICTXT *c = (DEVGDICTXT*)param;

    while (!(c->nStatus & DEVGDI_CLOSE))
    {
        int ret = WaitForSingleObject(c->semr, c->tickframe);
        if (ret != WAIT_OBJECT_0) continue;

        if (c->refresh_flag) {
            c->refresh_flag = 0;
            RefreshWindowBackground(c->hwnd, c->x, c->y, c->w, c->h);
        }

        int64_t apts = c->apts;
        int64_t vpts = c->vpts = c->ppts[c->head];
        if (vpts != -1) {
            SelectObject(c->hdcsrc, c->hbitmaps[c->head]);
            BitBlt(c->hdcdst, c->x, c->y, c->w, c->h, c->hdcsrc, 0, 0, SRCCOPY);
        }

//      log_printf(TEXT("vpts: %lld\n"), vpts);
        if (++c->head == c->bufnum) c->head = 0;
        ReleaseSemaphore(c->semw, 1, NULL);

        if (!(c->nStatus & DEVGDI_PAUSE)) {
            //++ play completed ++//
            if (c->completed_apts != c->apts || c->completed_vpts != c->vpts) {
                c->completed_apts = c->apts;
                c->completed_vpts = c->vpts;
                c->completed_counter = 0;
            }
            else if (++c->completed_counter == 50) {
                PostMessage(c->hwnd, MSG_COREPLAYER, PLAY_COMPLETED, 0);
                log_printf(TEXT("play completed !\n"));
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
    DEVGDICTXT *ctxt = (DEVGDICTXT*)malloc(sizeof(DEVGDICTXT));
    if (!ctxt) {
        log_printf(TEXT("failed to allocate gdi vdev context !\n"));
        exit(0);
    }

    // init vdev context
    memset(ctxt, 0, sizeof(DEVGDICTXT));
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->hwnd      = (HWND)surface;
    ctxt->bufnum    = bufnum;
    ctxt->w         = w;
    ctxt->h         = h;
    ctxt->tickframe = 1000 / frate;
    ctxt->ticksleep = ctxt->tickframe;

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
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;

    // make rendering thread safely exit
    c->nStatus = DEVGDI_CLOSE;
    WaitForSingleObject(c->hThread, -1);
    CloseHandle(c->hThread);

    DeleteDC (c->hdcsrc);
    ReleaseDC(c->hwnd, c->hdcdst);
    for (i=0; i<c->bufnum; i++) {
        if (c->hbitmaps[i]) {
            DeleteObject(c->hbitmaps[i]);
        }
    }

    // close semaphore
    CloseHandle(c->semr);
    CloseHandle(c->semw);

    // free memory
    free(c->ppts    );
    free(c->hbitmaps);
    free(c->pbmpbufs);
    free(c);
}

void vdev_gdi_request(void *ctxt, void **buffer, int *stride)
{
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;

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
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;
    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    ReleaseSemaphore(c->semr, 1, NULL);
}

void vdev_gdi_setrect(void *ctxt, int x, int y, int w, int h)
{
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;
    c->x = x; c->y = y;
    c->w = w; c->h = h;
    c->refresh_flag= 1;
}

void vdev_gdi_pause(void *ctxt, BOOL pause)
{
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;
    if (pause) {
        c->nStatus |=  DEVGDI_PAUSE;
    }
    else {
        c->nStatus &= ~DEVGDI_PAUSE;
    }
}

void vdev_gdi_reset(void *ctxt)
{
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;
    while (WAIT_OBJECT_0 == WaitForSingleObject(c->semr, 0));
    ReleaseSemaphore(c->semw, c->bufnum, NULL);
    c->head = c->tail = 0;
    c->apts = c->vpts = 0;
}

void vdev_gdi_getavpts(void *ctxt, int64_t **ppapts, int64_t **ppvpts)
{
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;
    if (ppapts) *ppapts = &c->apts;
    if (ppvpts) *ppvpts = &c->vpts;
}

void vdev_gdi_veffect(void *ctxt, void *buf, int len, int type, int x, int y, int w, int h)
{
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;
    if (!c || c->vpts != -1 && !(type & VISUAL_EFFECT_FORCE_DISPLAY)) return;

    switch (type) {
    case VISUAL_EFFECT_WAVEFORM:
        {
            // todo...
        }
        break;
    case VISUAL_EFFECT_SPECTRUM:
        {
            // todo...
        }
        break;
    }
}

void vdev_gdi_setparam(void *ctxt, DWORD id, void *param)
{
    if (!ctxt || !param) return;
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;

    switch (id) {
    case PARAM_VDEV_PIXEL_FORMAT:
        // pixel format is read only
        // do nothing
        break;
    case PARAM_VDEV_FRAME_RATE:
        c->tickframe = 1000 / (*(int*)param > 1 ? *(int*)param : 1);
        break;
    case PARAM_VDEV_SLOW_FLAG:
        // slow flag is read only
        // do nothing
        break;
    }
}

void vdev_gdi_getparam(void *ctxt, DWORD id, void *param)
{
    if (!ctxt || !param) return;
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;

    switch (id) {
    case PARAM_VDEV_PIXEL_FORMAT:
        *(int*)param = AV_PIX_FMT_RGB32;
        break;
    case PARAM_VDEV_FRAME_RATE:
        *(int*)param = 1000 / c->tickframe;
        break;
    case PARAM_VDEV_SLOW_FLAG:
        if      (c->ticksleep < -100) *(int*)param = 1;
        else if (c->ticksleep >  50 ) *(int*)param =-1;
        else                          *(int*)param = 0;
        break;
    }
}

