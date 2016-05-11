// 包含头文件
#include "coreplayer.h"
#include "veffect.h"

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
} VEFFECT;

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
    ReleaseDC(ve->hwnd, ve->hdcdst);
    DeleteDC (ve->hdcsrc);
    DeleteObject(ve->hpen);
    DeleteObject(ve->hbmp);
    free(ve);
}

void veffect_render(void *ctxt, int x, int y, int w, int h, int type, void *buf, int len)
{
    VEFFECT *ve = (VEFFECT*)ctxt;
    if (!ve || !buf || !type) return;

    switch (type) {
    case VISUAL_EFFECT_WAVEFORM:
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
            SHORT *sambuf = (SHORT*)buf;
            int    samnum = len / 4;
            int    sample = 0;
            int    delta  = samnum / w > 1 ? samnum / w : 1;
            int    sx, sy, i;

            sample = (sambuf[0] + sambuf[1]) / 2;
            sx     = 0;
            sy     = y + h * (0x7FFF - sample) / 0x10000;
            memset(ve->pbmp, 0, ve->stride * h);
            MoveToEx(ve->hdcsrc, sx, sy, NULL);
            for (i=delta; i<samnum; i+=delta) {
                sample = (sambuf[i*2+0] + sambuf[i*2+1]) / 2;
                sx = x + w * i / samnum;
                sy = y + h * (0x7FFF - sample) / 0x10000;
                LineTo(ve->hdcsrc, sx, sy);
            }
            BitBlt(ve->hdcdst, x, y, w, h, ve->hdcsrc, 0, 0, SRCCOPY);
            //-- draw visual effect
        }
        break;
    case VISUAL_EFFECT_SPECTRUM:
        {
            // todo...
        }
        break;
    }
}

