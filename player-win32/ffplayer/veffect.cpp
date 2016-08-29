// 包含头文件
#include <math.h>
#include "ffplayer.h"
#include "veffect.h"
#include "fft.h"
#include "log.h"

// 内部常量定义
#define MAX_GRID_COUNT_X  64
#define MAX_GRID_COUNT_Y  16

// 内部类型定义
typedef struct {
    HWND     hwnd;
    int      w;
    int      h;
    HDC      hdcdst;
    HDC      hdcsrc;
    HPEN     hpen0;
    HPEN     hpen1;
    HBITMAP  hbmp;
    HBITMAP  hfill;
    BYTE    *pbmp;
    int      stride;
    int      data_len;
    float   *data_buf;
    void    *fft;
} VEFFECT;

// 内部函数实现
static void resize_veffect_ifneeded(VEFFECT *ve, int w, int h)
{
    if (!ve->hbmp || ve->w != w || ve->h != h)
    {
        //++ re-create bitmap for draw buffer
        BITMAPINFO bmpinfo = {0};
        bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
        bmpinfo.bmiHeader.biWidth       =  w;
        bmpinfo.bmiHeader.biHeight      = -h;
        bmpinfo.bmiHeader.biPlanes      =  1;
        bmpinfo.bmiHeader.biBitCount    =  32;
        bmpinfo.bmiHeader.biCompression =  BI_RGB;
        HBITMAP hbmp = CreateDIBSection(ve->hdcsrc, &bmpinfo, DIB_RGB_COLORS, (void**)&ve->pbmp, NULL, 0);
        HANDLE  hobj = SelectObject(ve->hdcsrc, hbmp);
        if (hobj) DeleteObject(hobj);

        BITMAP bitmap = {0};
        GetObject(hbmp, sizeof(BITMAP), &bitmap);
        ve->hbmp   = hbmp;
        ve->w      = w;
        ve->h      = h;
        ve->stride = bitmap.bmWidthBytes;
        //-- re-create bitmap for draw buffer

        //++ re-create bitmap for gradient fill
        if (ve->hfill) DeleteObject(ve->hfill);
        bmpinfo.bmiHeader.biSize        =  sizeof(BITMAPINFOHEADER);
        bmpinfo.bmiHeader.biWidth       =  w / MAX_GRID_COUNT_X;
        bmpinfo.bmiHeader.biHeight      = -h / MAX_GRID_COUNT_Y * MAX_GRID_COUNT_Y;
        bmpinfo.bmiHeader.biPlanes      =  1;
        bmpinfo.bmiHeader.biBitCount    =  32;
        bmpinfo.bmiHeader.biCompression =  BI_RGB;
        ve->hfill = CreateDIBSection(ve->hdcsrc, &bmpinfo, DIB_RGB_COLORS, NULL, NULL, 0);
        HDC hdc   = CreateCompatibleDC(ve->hdcsrc);
        SelectObject(hdc, ve->hfill);
        TRIVERTEX     vert[2];
        GRADIENT_RECT grect;
        vert[0].x        = 0;
        vert[0].y        = 0;
        vert[0].Red      = 0xffff;
        vert[0].Green    = 0xdddd;
        vert[0].Blue     = 0x1111;
        vert[0].Alpha    = 0x0000;
        vert[1].x        = bmpinfo.bmiHeader.biWidth;
        vert[1].y        =-bmpinfo.bmiHeader.biHeight;
        vert[1].Red      = 0x8888;
        vert[1].Green    = 0xffff;
        vert[1].Blue     = 0x1111;
        vert[1].Alpha    = 0x0000;
        grect.UpperLeft  = 0;
        grect.LowerRight = 1;
        GradientFill(hdc, vert, 2, &grect, 1, GRADIENT_FILL_RECT_V);
        DeleteDC(hdc);
        //-- re-create bitmap for gradient fill
    }
}

static void draw_waveform(VEFFECT *ve, int x, int y, int w, int h, float divisor, float *sample, int n)
{
    // resize veffect if needed
    resize_veffect_ifneeded(ve, w, h);

    //++ draw visual effect
    int delta  = n / w > 1 ? n / w : 1;
    int px, py, i;

    // clear bitmap
    memset(ve->pbmp, 0, ve->stride * h);

    px = 0;
    py = y + h - (int)(h * sample[0] / divisor);
    SelectObject(ve->hdcsrc, ve->hpen0);
    MoveToEx(ve->hdcsrc, px, py, NULL );
    for (i=delta; i<n; i+=delta) {
        px = x + w * i / n;
        py = y + h - (int)(h * sample[i] / divisor);
        LineTo(ve->hdcsrc, px, py);
    }
    BitBlt(ve->hdcdst, x, y, w, h, ve->hdcsrc, 0, 0, SRCCOPY);
    //-- draw visual effect
}

static void draw_spectrum(VEFFECT *ve, int x, int y, int w, int h, float *sample, int n)
{
    int    amplitude;
    int    gridw, gridh;
    int    d = n / MAX_GRID_COUNT_X;
    float *fsrc = sample;
    int    i, j, tx, ty;
    int    sw, sh, sx, sy;
    HDC    hdc;

    // resize veffect if needed
    resize_veffect_ifneeded(ve, w, h);

    // clear bitmap
    memset(ve->pbmp, 0, ve->stride * h);

    // calucate for grid
    gridw = w / MAX_GRID_COUNT_X; if (gridw == 0) gridw = 1;
    gridh = h / MAX_GRID_COUNT_Y; if (gridh == 0) gridh = 1;
    sw = gridw * MAX_GRID_COUNT_X; sx = x + (w - sw) / 2;
    sh = gridh * MAX_GRID_COUNT_Y; sy = y + (h - sh) / 2;

    hdc = CreateCompatibleDC(ve->hdcsrc);
    SelectObject(hdc       , ve->hfill);
    SelectObject(ve->hdcsrc, ve->hpen1);

    for (ty=sy; ty<=sy+sh; ty+=gridh) {
        MoveToEx(ve->hdcsrc, sx, ty, NULL);
        LineTo  (ve->hdcsrc, sx + sw, ty );
    }

    // calculate amplitude
    for (i=0; i<MAX_GRID_COUNT_X; i++) {
        amplitude = 0;
        for (j=0; j<d; j++) {
            amplitude += (int)(*fsrc++ * sh/0x100000);
        }
        amplitude /= d;
        if (amplitude > sh) amplitude = sh;
        tx = sx + (i + 0) * gridw;
        ty = sy + sh - amplitude;
        BitBlt(ve->hdcsrc, tx, ty, gridw, amplitude,
               hdc, 0, sh - amplitude, SRCCOPY);
    }

    for (tx=sx; tx<=sx+sw; tx+=gridw) {
        MoveToEx(ve->hdcsrc, tx, sy, NULL);
        LineTo  (ve->hdcsrc, tx, sy + sh );
    }

    BitBlt(ve->hdcdst, x, y, w, h, ve->hdcsrc, 0, 0, SRCCOPY);
    DeleteDC(hdc);
}

// 函数实现
void* veffect_create(void *surface)
{
    VEFFECT *ve = (VEFFECT*)malloc(sizeof(VEFFECT));
    if (!ve) return NULL;

    memset(ve, 0, sizeof(VEFFECT));
    ve->hwnd   = (HWND) surface;
    ve->hdcdst = GetDC((HWND)surface);
    ve->hdcsrc = CreateCompatibleDC(ve->hdcdst);
    ve->hpen0  = CreatePen(PS_SOLID, 1, RGB(0 , 255, 0 ));
    ve->hpen1  = CreatePen(PS_SOLID, 1, RGB(32, 32 , 64));

    return ve;
}

void veffect_destroy(void *ctxt)
{
    VEFFECT *ve = (VEFFECT*)ctxt;

    // for dc & object
    ReleaseDC(ve->hwnd, ve->hdcdst);
    DeleteDC (ve->hdcsrc);
    DeleteObject(ve->hpen0);
    DeleteObject(ve->hpen1);
    DeleteObject(ve->hfill);
    DeleteObject(ve->hbmp );

    // free fft
    fft_free(ve->fft);

    // free data_buf
    free(ve->data_buf);

    // free
    free(ve);
}

void veffect_render(void *ctxt, int x, int y, int w, int h, int type, void *buf, int len)
{
    VEFFECT *ve = (VEFFECT*)ctxt;

    if (!ve->data_buf) {
        ve->data_len = 1 << (int)(log(len/4.0)/log(2.0));
        ve->data_buf = (float*)malloc(ve->data_len * sizeof(float) * 2);
        ve->fft      = fft_init(ve->data_len);
    }

    switch (type) {
    case VISUAL_EFFECT_DISABLE:
        {
            RECT rect = { x, y, x + w, y + h};
            InvalidateRect(ve->hwnd, &rect, TRUE);
        }
        break;
    case VISUAL_EFFECT_WAVEFORM:
        {
            short *ssrc = (short*)buf;
            float *fdst = (float*)ve->data_buf;
            int    snum = len / 4;
            int    i;
            for (i=0; i<snum; i++) {
                *fdst = (float)(((int)ssrc[0] + (int)ssrc[1]) / 2 + 0x7fff);
                fdst += 1;
                ssrc += 2;
            }
            draw_waveform(ve, x, y, w, h, 0x10000, ve->data_buf, snum);
        }
        break;
    case VISUAL_EFFECT_SPECTRUM:
        {
            short *ssrc = (short*)buf;
            float *fsrc = (float*)ve->data_buf;
            float *fdst = (float*)ve->data_buf;
            int    i;
            for (i=0; i<ve->data_len; i++) {
                *fdst++ = (float)((ssrc[0] + ssrc[1]) / 2);
                *fdst++ = 0;
                ssrc   += 2;
            }
            fft_execute(ve->fft, ve->data_buf, ve->data_buf);
            fsrc = fdst = (float*)ve->data_buf;
            for (i=0; i<ve->data_len; i++) {
                *fdst = sqrt(fsrc[0] * fsrc[0] + fsrc[1] * fsrc[1]);
                fdst += 1;
                fsrc += 2;
            }
//          draw_waveform(ve, x, y, w, h, 0x100000, ve->data_buf, ve->data_len);
            draw_spectrum(ve, x, y, w, h, ve->data_buf, ve->data_len * 47 / 128); // 44100 * 47 / 128 = 16K
        }
        break;
    }
}

