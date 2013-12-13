// 包含头文件
#include <windows.h>
#include "bmpbufqueue.h"

// 函数实现
BOOL bmpbufqueue_create(BMPBUFQUEUE *pbq, HDC hdc, int w, int h, int cdepth)
{
    BYTE        infobuf[sizeof(BITMAPINFO) + 3 * sizeof(RGBQUAD)] = {0};
    BITMAPINFO *bmpinfo = (BITMAPINFO*)infobuf;
    DWORD      *quad    = (DWORD*)&(bmpinfo->bmiColors[0]);
    int         i;

    // default size
    if (pbq->size == 0) pbq->size = DEF_BMPBUF_QUEUE_SIZE;

    // alloc buffer & semaphore
    pbq->ppts     = (int64_t*)malloc(pbq->size * sizeof(int64_t));
    pbq->hbitmaps = (HBITMAP*)malloc(pbq->size * sizeof(HBITMAP));
    pbq->pbmpbufs = (BYTE**  )malloc(pbq->size * sizeof(BYTE*  ));
    pbq->semr     = CreateSemaphore(NULL, 0        , pbq->size, NULL);
    pbq->semw     = CreateSemaphore(NULL, pbq->size, pbq->size, NULL);

    // check invalid
    if (!pbq->ppts || !pbq->hbitmaps || !pbq->pbmpbufs || !pbq->semr || !pbq->semw) {
        bmpbufqueue_destroy(pbq);
        return FALSE;
    }

    bmpinfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmpinfo->bmiHeader.biWidth       = w;
    bmpinfo->bmiHeader.biHeight      =-h;
    bmpinfo->bmiHeader.biPlanes      = 1;
    bmpinfo->bmiHeader.biBitCount    = cdepth;
    bmpinfo->bmiHeader.biCompression = BI_BITFIELDS;

    quad[0] = 0xF800; // RGB565
    quad[1] = 0x07E0;
    quad[2] = 0x001F;

    // clear
    memset(pbq->ppts    , 0, pbq->size * sizeof(int64_t));
    memset(pbq->hbitmaps, 0, pbq->size * sizeof(HBITMAP));
    memset(pbq->pbmpbufs, 0, pbq->size * sizeof(BYTE*  ));

    // init
    for (i=0; i<pbq->size; i++) {
        pbq->hbitmaps[i] = CreateDIBSection(hdc, bmpinfo, DIB_RGB_COLORS,
            (void**)&(pbq->pbmpbufs[i]), NULL, 0);
        if (!pbq->hbitmaps[i] || !pbq->pbmpbufs[i]) break;
    }
    pbq->size = i; // size

    return TRUE;
}

void bmpbufqueue_destroy(BMPBUFQUEUE *pbq)
{
    int i;

    for (i=0; i<pbq->size; i++) {
        if (pbq->hbitmaps[i]) DeleteObject(pbq->hbitmaps[i]);
    }

    if (pbq->ppts    ) free(pbq->ppts    );
    if (pbq->hbitmaps) free(pbq->hbitmaps);
    if (pbq->pbmpbufs) free(pbq->pbmpbufs);
    if (pbq->semr    ) CloseHandle(pbq->semr);
    if (pbq->semw    ) CloseHandle(pbq->semw);

    // clear members
    memset(pbq, 0, sizeof(BMPBUFQUEUE));
}

BOOL bmpbufqueue_isempty(BMPBUFQUEUE *pbq)
{
    return (pbq->curnum <= 0);
}

void bmpbufqueue_write_request(BMPBUFQUEUE *pbq, int64_t **ppts, BYTE **pbuf, int *stride)
{
    WaitForSingleObject(pbq->semw, -1);

    if (ppts) *ppts = &(pbq->ppts[pbq->tail]);
    if (pbuf) *pbuf = pbq->pbmpbufs[pbq->tail];

    if (stride && pbq->hbitmaps) {
        BITMAP bitmap = {0};
        GetObject(pbq->hbitmaps[pbq->tail], sizeof(BITMAP), &bitmap);
        *stride = bitmap.bmWidthBytes;
    }
}

void bmpbufqueue_write_release(BMPBUFQUEUE *pbq)
{
    ReleaseSemaphore(pbq->semw, 1, NULL);
}

void bmpbufqueue_write_done(BMPBUFQUEUE *pbq)
{
    InterlockedIncrement(&(pbq->tail));
    InterlockedCompareExchange(&(pbq->tail), 0, pbq->size);
    InterlockedIncrement(&(pbq->curnum));
    ReleaseSemaphore(pbq->semr, 1, NULL);
}

void bmpbufqueue_read_request(BMPBUFQUEUE *pbq, int64_t **ppts, HBITMAP *hbitmap)
{
    WaitForSingleObject(pbq->semr, -1);
    if (ppts   ) *ppts    = &(pbq->ppts[pbq->head]);
    if (hbitmap) *hbitmap = pbq->hbitmaps[pbq->head];
}

void bmpbufqueue_read_release(BMPBUFQUEUE *pbq)
{
    ReleaseSemaphore(pbq->semr, 1, NULL);
}

void bmpbufqueue_read_done(BMPBUFQUEUE *pbq)
{
    InterlockedIncrement(&(pbq->head));
    InterlockedCompareExchange(&(pbq->head), 0, pbq->size);
    InterlockedDecrement(&(pbq->curnum));
    ReleaseSemaphore(pbq->semw, 1, NULL);
}





