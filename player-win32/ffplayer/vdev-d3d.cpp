// 包含头文件
#include <d3d9.h>
#include "coreplayer.h"
#include "vdev.h"
#include "log.h"

// 内部常量定义
#define DEF_VDEV_BUF_NUM  5

// 内部类型定义
typedef struct
{
    int   width;      /* 宽度 */
    int   height;     /* 高度 */
    HWND  hwnd;       /* 窗口句柄 */

    LPDIRECT3D9            pD3D;
    LPDIRECT3DDEVICE9   pD3DDev;
    LPDIRECT3DSURFACE9 pSurface;
    D3DPRESENT_PARAMETERS d3dpp;

    RECT  rtlast;
    RECT  rtview;

    DWORD apts;
    DWORD vpts;
} DEVD3DCTXT;

// 内部函数实现
void create_device_surface(DEVD3DCTXT *ctxt)
{
    if (ctxt->d3dpp.Windowed)
    {
        ctxt->d3dpp.BackBufferWidth  = ctxt->width;
        ctxt->d3dpp.BackBufferHeight = ctxt->height;
    }

    if (FAILED(ctxt->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ctxt->hwnd,
                    D3DCREATE_SOFTWARE_VERTEXPROCESSING, &ctxt->d3dpp, &ctxt->pD3DDev)))
    {
        log_printf(TEXT("failed to create d3d device !\n"));
        exit(0);
    }

    // clear direct3d device
    ctxt->pD3DDev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);

    // create surface
    if (FAILED(ctxt->pD3DDev->CreateOffscreenPlainSurface(ctxt->width, ctxt->height,
                    D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &ctxt->pSurface, NULL)))
    {
        log_printf(TEXT("failed to create d3d off screen plain surface !\n"));
        exit(0);
    }

    D3DLOCKED_RECT d3d_rect;
    ctxt->pSurface->LockRect(&d3d_rect, NULL, D3DLOCK_DISCARD);
    memset(d3d_rect.pBits, 0, d3d_rect.Pitch * ctxt->height);
    ctxt->pSurface->UnlockRect();
}

// 接口函数实现
void* vdev_d3d_create(void *surface, int w, int h, int frate)
{
    DEVD3DCTXT *ctxt = (DEVD3DCTXT*)malloc(sizeof(DEVD3DCTXT));
    if (!ctxt) {
        log_printf(TEXT("failed to allocate d3d vdev context !\n"));
        exit(0);
    }

    // init d3d vdev context
    memset(ctxt, 0, sizeof(DEVD3DCTXT));
    ctxt->width  = w;
    ctxt->height = h;
    ctxt->hwnd   = (HWND)surface;

    // create d3d
    ctxt->pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!ctxt->pD3D) {
        log_printf(TEXT("failed to allocate d3d object !\n"));
        exit(0);
    }

    // fill d3dpp struct
    D3DDISPLAYMODE d3dmode = {0};
    ctxt->pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3dmode);
    ctxt->d3dpp.BackBufferFormat      = D3DFMT_X8R8G8B8;
    ctxt->d3dpp.BackBufferCount       = 1;
    ctxt->d3dpp.MultiSampleType       = D3DMULTISAMPLE_NONE;
    ctxt->d3dpp.MultiSampleQuality    = 0;
    ctxt->d3dpp.SwapEffect            = D3DSWAPEFFECT_COPY;
    ctxt->d3dpp.hDeviceWindow         = ctxt->hwnd;
    ctxt->d3dpp.Windowed              = TRUE;
    ctxt->d3dpp.EnableAutoDepthStencil= FALSE;
    ctxt->d3dpp.PresentationInterval  = d3dmode.RefreshRate < 60 ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;

    // create d3d device and surface
    create_device_surface(ctxt);

    return ctxt;
}

void vdev_d3d_destroy(void *ctxt)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    c->pSurface->Release();
    c->pD3DDev->Release();
    c->pD3D->Release();
    free(c);
}

void vdev_d3d_bufrequest(void *ctxt, void **buf, int *stride)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;

    WaitForSingleObject(c->semw, -1);

    // lock texture rect
    D3DLOCKED_RECT d3d_rect;
    c->pSurface->LockRect(&d3d_rect, NULL, D3DLOCK_DISCARD);

    if (buf   ) *buf    = d3d_rect.pBits;
    if (stride) *stride = d3d_rect.Pitch;
}


void vdev_d3d_post(void *ctxt, int64_t pts)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    c->ppts[c->tail] = pts;
    if (++c->tail == c->bufnum) c->tail = 0;
    ReleaseSemaphore(c->semr, 1, NULL);
}

void vdev_d3d_setrect(void *ctxt, int x, int y, int w, int h)
{
    ((DEVD3DCTXT*)ctxt)->x = x;
    ((DEVD3DCTXT*)ctxt)->y = y;
    ((DEVD3DCTXT*)ctxt)->w = w;
    ((DEVD3DCTXT*)ctxt)->h = h;
}

void vdev_d3d_pause(void *ctxt, BOOL pause)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    if (pause) {
        c->bStatus |=  DEVGDI_PAUSE;
    }
    else {
        c->bStatus &= ~DEVGDI_PAUSE;
    }
}

void vdev_d3d_reset(void *ctxt)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    while (WAIT_OBJECT_0 == WaitForSingleObject(c->semr, 0));
    ReleaseSemaphore(c->semw, c->bufnum, NULL);
    c->head = c->tail = 0;
    c->apts = c->vpts = 0;
}

void vdev_d3d_getavpts(void *ctxt, int64_t **ppapts, int64_t **ppvpts)
{
    DEVD3DCTXT *c = (DEVD3DCTXT*)ctxt;
    if (ppapts) *ppapts = &c->apts;
    if (ppvpts) *ppvpts = &c->vpts;
}

