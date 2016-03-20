// 包含头文件
#include "coreplayer.h"
#include "vdev.h"
#include "log.h"

// 内部常量定义
#define DEF_VDEV_BUF_NUM  5

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
    BOOL     bStatus;
    HANDLE   hThread;

    int      completed_counter;
    int64_t  completed_apts;
    int64_t  completed_vpts;
} DEVGDICTXT;

// 内部函数实现
static DWORD WINAPI VideoRenderThreadProc(void *param)
{
    DEVGDICTXT *c = (DEVGDICTXT*)param;

    while (!(c->bStatus & DEVGDI_CLOSE))
    {
        if (c->bStatus & DEVGDI_PAUSE) {
            BITMAP bitmap; GetObject(c->hbitmaps[c->tail], sizeof(BITMAP), &bitmap);
            SelectObject(c->hdcsrc, c->hbitmaps[c->head]);
            StretchBlt(c->hdcdst, c->x, c->y, c->w, c->h, c->hdcsrc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
            Sleep(c->tickframe); continue;
        }

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

        int ret = WaitForSingleObject(c->semr, c->tickframe);
        if (ret != WAIT_OBJECT_0) continue;

        int64_t apts = c->apts;
        int64_t vpts = c->vpts = c->ppts[c->head];
        SelectObject(c->hdcsrc, c->hbitmaps[c->head]);
        BitBlt(c->hdcdst, c->x, c->y, c->w, c->h, c->hdcsrc, 0, 0, SRCCOPY);
//      log_printf(TEXT("vpts: %lld\n"), vpts);
        if (++c->head == c->bufnum) c->head = 0;
        ReleaseSemaphore(c->semw, 1, NULL);

        DWORD tickcur  = GetTickCount();
        int   tickdiff = tickcur - c->ticklast;
        c->ticklast = tickcur;
        if (tickdiff - c->tickframe >  1) c->ticksleep--;
        if (tickdiff - c->tickframe < -1) c->ticksleep++;
        if (apts != -1) {
            if (apts - vpts >  1) c->ticksleep-=2;
            if (apts - vpts < -1) c->ticksleep+=2;
        }
        if (c->ticksleep > 0) Sleep(c->ticksleep);
        log_printf(TEXT("diff: %5lld, sleep: %d\n"), apts-vpts, c->ticksleep);
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
    c->bStatus |= DEVGDI_CLOSE;
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

    RECT rtwin, rect1, rect2, rect3, rect4;
    GetClientRect(c->hwnd, &rtwin);
    rect1.left = 0;   rect1.top = 0;   rect1.right = rtwin.right; rect1.bottom = y;
    rect2.left = 0;   rect2.top = y;   rect2.right = x;           rect2.bottom = y+h;
    rect3.left = x+w; rect3.top = y;   rect3.right = rtwin.right; rect3.bottom = y+h;
    rect4.left = 0;   rect4.top = y+h; rect4.right = rtwin.right; rect4.bottom = rtwin.bottom;
    InvalidateRect(c->hwnd, &rect1, FALSE);
    InvalidateRect(c->hwnd, &rect2, FALSE);
    InvalidateRect(c->hwnd, &rect3, FALSE);
    InvalidateRect(c->hwnd, &rect4, FALSE);
}

void vdev_gdi_pause(void *ctxt, BOOL pause)
{
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;
    if (pause) {
        c->bStatus |=  DEVGDI_PAUSE;
    }
    else {
        c->bStatus &= ~DEVGDI_PAUSE;
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

void vdev_gdi_setfrate(void *ctxt, int frate)
{
    DEVGDICTXT *c = (DEVGDICTXT*)ctxt;
    c->tickframe = 1000 / frate;
}
