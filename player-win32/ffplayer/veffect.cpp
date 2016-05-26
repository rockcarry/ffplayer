// 包含头文件
#include <math.h>
#include "ffplayer.h"
#include "veffect.h"
#include "fft.h"
#include "log.h"

// 内部类型定义
typedef struct {
    HWND     hwnd;
    int      w;
    int      h;
    HDC      hdcdst;
    HDC      hdcsrc;
    HPEN     hpen;
    HBITMAP  hbmp;
    BYTE    *pbmp;
    int      stride;
    int      data_len;
    float   *data_buf;
    void    *fft;
} VEFFECT;

// 内部函数实现
static void draw_effect(VEFFECT *ve, int x, int y, int w, int h, float factor, float *sample, int n)
{
    // create dc for ve
    if (!ve->hdcsrc) {
        ve->hdcsrc = CreateCompatibleDC(ve->hdcdst);
    }

    // create pen for ve
    if (!ve->hpen) {
        ve->hpen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
        SelectObject(ve->hdcsrc, ve->hpen);
    }

    // create bitmap for ve
    if (!ve->hbmp || ve->w != w || ve->h != h) {
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
    }

    //++ draw visual effect
    int delta  = n / w > 1 ? n / w : 1;
    int px, py, i;

    // clear bitmap
    memset(ve->pbmp, 0, ve->stride * h);

    px = 0;
    py = y + h - (int)(h * sample[0] / factor);
    MoveToEx(ve->hdcsrc, px, py, NULL);
    for (i=delta; i<n; i+=delta) {
        px = x + w * i / n;
        py = y + h - (int)(h * sample[i] / factor);
        LineTo(ve->hdcsrc, px, py);
    }
    BitBlt(ve->hdcdst, x, y, w, h, ve->hdcsrc, 0, 0, SRCCOPY);
    //-- draw visual effect
}

// 函数实现
void* veffect_create(void *surface)
{
    VEFFECT *ve = (VEFFECT*)malloc(sizeof(VEFFECT));
    if (!ve) return NULL;

    memset(ve, 0, sizeof(VEFFECT));
    ve->hwnd   = (HWND) surface;
    ve->hdcdst = GetDC((HWND)surface);
    return ve;
}

void veffect_destroy(void *ctxt)
{
    VEFFECT *ve = (VEFFECT*)ctxt;

    // for dc & object
    ReleaseDC(ve->hwnd, ve->hdcdst);
    DeleteDC (ve->hdcsrc);
    DeleteObject(ve->hpen);
    DeleteObject(ve->hbmp);

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
            draw_effect(ve, x, y, w, h, 0x10000, ve->data_buf, snum);
        }
        break;
    case VISUAL_EFFECT_SPECTRUM:
        {
            short *ssrc = (short*)buf;
            float *fdst = (float*)ve->data_buf;
            int    i;
            for (i=0; i<ve->data_len; i++) {
                *fdst++ = (float)((ssrc[0] + ssrc[1]) / 2);
                *fdst++ = 0;
                ssrc   += 2;
            }
            fft_execute(ve->fft, ve->data_buf, ve->data_buf);
            fdst = (float*)ve->data_buf;
            for (i=0; i<ve->data_len; i++) {
                *fdst = sqrt(fdst[0] * fdst[0] + fdst[1] * fdst[1]);
                fdst += 1;
            }
            draw_effect(ve, x, y, w, h, 0x100000, ve->data_buf, ve->data_len);
        }
        break;
    }
}

