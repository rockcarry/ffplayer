// 包含头文件
#include <d3d9.h>
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

    #define DEVD3D_CLOSE  (1 << 0)
    #define DEVD3D_PAUSE  (1 << 1)
    int      nStatus;
    HANDLE   hThread;

    int      completed_counter;
    int64_t  completed_apts;
    int64_t  completed_vpts;

    LPDIRECT3D9           pD3D9;
    LPDIRECT3DDEVICE9     pD3DDev;
    LPDIRECT3DSURFACE9   *pSurfs;
    IDirect3DSwapChain9  *pSwapChain;
    D3DPRESENT_PARAMETERS d3dpp;
    int                   need_reset;
} DEVD3DCTXT;

// 内部函数实现
static void d3d_draw_surf(DEVD3DCTXT *c, LPDIRECT3DSURFACE9 surf)
{
    RECT rect;

    if (c->need_reset) {
        c->need_reset = 0;
        GetClientRect(c->hwnd, &rect);
        c->d3dpp.BackBufferWidth  = rect.right;
        c->d3dpp.BackBufferHeight = rect.bottom;
        if (c->pSwapChain) c->pSwapChain->Release();
        if (FAILED(c->pD3DDev->CreateAdditionalSwapChain(&c->d3dpp, &c->pSwapChain))) {
            log_printf(TEXT("failed to create swap chain !\n"));
            exit(0);
        }
    }

    IDirect3DSurface9 *pBackBuffer = NULL;
    if (SUCCEEDED(c->pSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
    {
        if (pBackBuffer) {
            rect.left   = c->x;
            rect.top    = c->y;
            rect.right  = c->x + c->w;
            rect.bottom = c->y + c->h;
            if (SUCCEEDED(c->pD3DDev->StretchRect(surf, NULL, pBackBuffer, &rect, D3DTEXF_LINEAR)))
            {
                c->pSwapChain->Present(&rect, &rect, NULL, NULL, NULL);
            }
            pBackBuffer->Release();
        }
    }
}

static DWORD WINAPI VideoRenderThreadProc(void *param)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)param;

    while (!(c->nStatus & DEVD3D_CLOSE))
    {
        //++ play completed ++//
        if (c->completed_apts != c->apts || c->completed_vpts != c->vpts) {
            c->completed_apts = c->apts;
            c->completed_vpts = c->vpts;
            c->completed_counter = 0;
        }
        else if (!(c->nStatus & DEVD3D_PAUSE) && ++c->completed_counter == 50) {
            PostMessage(c->hwnd, MSG_COREPLAYER, PLAY_COMPLETED, 0);
            log_printf(TEXT("play completed !\n"));
        }
        //-- play completed --//

        int ret = WaitForSingleObject(c->semr, c->tickframe);
        if (ret != WAIT_OBJECT_0) continue;

        int64_t apts = c->apts;
        int64_t vpts = c->vpts = c->ppts[c->head];
        if (vpts != -1) {
            do {
                d3d_draw_surf(c, c->pSurfs[c->head]);
                if (c->nStatus & DEVD3D_PAUSE) Sleep(c->tickframe);
            } while (c->nStatus & DEVD3D_PAUSE);
        }

//      log_printf(TEXT("vpts: %lld\n"), vpts);
        if (++c->head == c->bufnum) c->head = 0;
        ReleaseSemaphore(c->semw, 1, NULL);

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
    }

    return NULL;
}

// 接口函数实现
void* vdev_d3d_create(void *surface, int bufnum, int w, int h, int frate)
{
    DEVD3DCTXT *ctxt = (DEVD3DCTXT*)malloc(sizeof(DEVD3DCTXT));
    if (!ctxt) {
        log_printf(TEXT("failed to allocate d3d vdev context !\n"));
        exit(0);
    }

    // init vdev context
    memset(ctxt, 0, sizeof(DEVD3DCTXT));
    bufnum          = bufnum ? bufnum : DEF_VDEV_BUF_NUM;
    ctxt->hwnd      = (HWND)surface;
    ctxt->bufnum    = bufnum;
    ctxt->w         = w;
    ctxt->h         = h;
    ctxt->tickframe = 1000 / frate;
    ctxt->ticksleep = ctxt->tickframe;
    ctxt->need_reset= 1;
    ctxt->apts      =-1;
    ctxt->vpts      =-1;

    // alloc buffer & semaphore
    ctxt->ppts   = (int64_t*)malloc(bufnum * sizeof(int64_t));
    ctxt->pSurfs = (LPDIRECT3DSURFACE9*)malloc(bufnum * sizeof(LPDIRECT3DSURFACE9));

    memset(ctxt->ppts  , 0, bufnum * sizeof(int64_t));
    memset(ctxt->pSurfs, 0, bufnum * sizeof(LPDIRECT3DSURFACE9));

    ctxt->semr = CreateSemaphore(NULL, 0     , bufnum, NULL);
    ctxt->semw = CreateSemaphore(NULL, bufnum, bufnum, NULL);

    // create d3d
    ctxt->pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!ctxt->ppts || !ctxt->pSurfs || !ctxt->semr || !ctxt->semw || !ctxt->pD3D9) {
        log_printf(TEXT("failed to allocate resources for vdev-d3d !\n"));
        exit(0);
    }

    // fill d3dpp struct
    D3DDISPLAYMODE d3dmode = {0};
    ctxt->pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3dmode);
    ctxt->d3dpp.BackBufferFormat      = D3DFMT_UNKNOWN;
    ctxt->d3dpp.BackBufferCount       = 1;
    ctxt->d3dpp.MultiSampleType       = D3DMULTISAMPLE_NONE;
    ctxt->d3dpp.SwapEffect            = D3DSWAPEFFECT_DISCARD;
    ctxt->d3dpp.hDeviceWindow         = ctxt->hwnd;
    ctxt->d3dpp.Windowed              = TRUE;
    ctxt->d3dpp.EnableAutoDepthStencil= FALSE;
    ctxt->d3dpp.PresentationInterval  = d3dmode.RefreshRate < 60 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;

    if (FAILED(ctxt->pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ctxt->hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &ctxt->d3dpp, &ctxt->pD3DDev)) )
    {
        log_printf(TEXT("failed to create d3d device !\n"));
        exit(0);
    }

    // create video rendering thread
    ctxt->hThread = CreateThread(NULL, 0, VideoRenderThreadProc, ctxt, 0, NULL);
    return ctxt;
}

void vdev_d3d_destroy(void *ctxt)
{
    int i;
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    // make rendering thread safely exit
    c->nStatus = DEVD3D_CLOSE;
    WaitForSingleObject(c->hThread, -1);
    CloseHandle(c->hThread);

    for (i=0; i<c->bufnum; i++) {
        if (c->pSurfs[i]) {
            c->pSurfs[i]->Release();
        }
    }

    c->pSwapChain->Release();
    c->pD3DDev   ->Release();
    c->pD3D9     ->Release();

    // close semaphore
    CloseHandle(c->semr);
    CloseHandle(c->semw);

    // free memory
    free(c->ppts  );
    free(c->pSurfs);
    free(c);
}

void vdev_d3d_request(void *ctxt, void **buf, int *stride)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;

    WaitForSingleObject(c->semw, -1);

    D3DSURFACE_DESC desc;
    int sufw = 0;
    int sufh = 0;
    if (c->pSurfs[c->tail]) {
        c->pSurfs[c->tail]->GetDesc(&desc);
        sufw = desc.Width ;
        sufh = desc.Height;
    }

    if (sufw != c->w || sufh != c->h) {
        if (c->pSurfs[c->tail]) {
            c->pSurfs[c->tail]->Release();
        }

        // create surface
        if (FAILED(c->pD3DDev->CreateOffscreenPlainSurface(c->w, c->h, D3DFMT_X8R8G8B8,
                    D3DPOOL_DEFAULT, &c->pSurfs[c->tail], NULL)))
        {
            log_printf(TEXT("failed to create d3d off screen plain surface !\n"));
            exit(0);
        }
    }

    // lock texture rect
    D3DLOCKED_RECT d3d_rect;
    c->pSurfs[c->tail]->LockRect(&d3d_rect, NULL, D3DLOCK_DISCARD);

    if (buf   ) *buf    = d3d_rect.pBits;
    if (stride) *stride = d3d_rect.Pitch;
}


void vdev_d3d_post(void *ctxt, int64_t pts)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    c->pSurfs[c->tail]->UnlockRect();
    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    ReleaseSemaphore(c->semr, 1, NULL);
}

void vdev_d3d_setrect(void *ctxt, int x, int y, int w, int h)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    c->x = x; c->y = y;
    c->w = w; c->h = h;
    c->need_reset = 1;

    RECT rtwin, rect1, rect2, rect3, rect4;
    GetClientRect(c->hwnd, &rtwin);
    rect1.left = 0;   rect1.top = 0;   rect1.right = rtwin.right; rect1.bottom = y;
    rect2.left = 0;   rect2.top = y;   rect2.right = x;           rect2.bottom = y+h;
    rect3.left = x+w; rect3.top = y;   rect3.right = rtwin.right; rect3.bottom = y+h;
    rect4.left = 0;   rect4.top = y+h; rect4.right = rtwin.right; rect4.bottom = rtwin.bottom;
    InvalidateRect(c->hwnd, &rect1, TRUE);
    InvalidateRect(c->hwnd, &rect2, TRUE);
    InvalidateRect(c->hwnd, &rect3, TRUE);
    InvalidateRect(c->hwnd, &rect4, TRUE);
}

void vdev_d3d_pause(void *ctxt, BOOL pause)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    if (pause) {
        c->nStatus |=  DEVD3D_PAUSE;
    }
    else {
        c->nStatus &= ~DEVD3D_PAUSE;
    }
}

void vdev_d3d_reset(void *ctxt)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    while (WAIT_OBJECT_0 == WaitForSingleObject(c->semr, 0));
    ReleaseSemaphore(c->semw, c->bufnum, NULL);
    c->head = c->tail =  0;
    c->apts = c->vpts = -1;
}

void vdev_d3d_getavpts(void *ctxt, int64_t **ppapts, int64_t **ppvpts)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    if (ppapts) *ppapts = &c->apts;
    if (ppvpts) *ppvpts = &c->vpts;
}

void vdev_d3d_setfrate(void *ctxt, int frate)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    c->tickframe = 1000 / frate;
}

int vdev_d3d_slowflag(void *ctxt)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    if (c->ticksleep < -100) return  1;
    if (c->ticksleep >  50 ) return -1;
    return 0;
}
