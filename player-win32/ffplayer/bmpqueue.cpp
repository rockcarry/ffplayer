// 包含头文件
#include "bmpqueue.h"

// 函数实现
BOOL bmpqueue_create(BMPQUEUE *pbq, HDC hdc, int w, int h, int cdepth)
{
    BYTE        infobuf[sizeof(BITMAPINFO) + 3 * sizeof(RGBQUAD)] = {0};
    BITMAPINFO *bmpinfo = (BITMAPINFO*)infobuf;
    DWORD      *quad    = (DWORD*)&(bmpinfo->bmiColors[0]);
    int         i;

    // default size
    if (pbq->size == 0) pbq->size = DEF_BMP_QUEUE_SIZE;

    // alloc buffer & semaphore
    pbq->ppts     = (int64_t*)malloc(pbq->size * sizeof(int64_t));
    pbq->hbitmaps = (HBITMAP*)malloc(pbq->size * sizeof(HBITMAP));
    pbq->pbmpbufs = (BYTE**  )malloc(pbq->size * sizeof(BYTE*  ));
    sem_init(&(pbq->semr), 0, 0        );
    sem_init(&(pbq->semw), 0, pbq->size);

    // check invalid
    if (!pbq->ppts || !pbq->hbitmaps || !pbq->pbmpbufs) {
        bmpqueue_destroy(pbq);
        return FALSE;
    }

    bmpinfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmpinfo->bmiHeader.biWidth       = w;
    bmpinfo->bmiHeader.biHeight      =-h;
    bmpinfo->bmiHeader.biPlanes      = 1;
    bmpinfo->bmiHeader.biBitCount    = cdepth;
    bmpinfo->bmiHeader.biCompression = cdepth == 16 ? BI_BITFIELDS : BI_RGB;

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

void bmpqueue_destroy(BMPQUEUE *pbq)
{
    int i;

    for (i=0; i<pbq->size; i++) {
        if (pbq->hbitmaps[i]) DeleteObject(pbq->hbitmaps[i]);
    }

    if (pbq->ppts    ) free(pbq->ppts    );
    if (pbq->hbitmaps) free(pbq->hbitmaps);
    if (pbq->pbmpbufs) free(pbq->pbmpbufs);
    sem_destroy(&(pbq->semr));
    sem_destroy(&(pbq->semw));

    // clear members
    memset(pbq, 0, sizeof(BMPQUEUE));
}

BOOL bmpqueue_isempty(BMPQUEUE *pbq)
{
    int value = 0;
    sem_getvalue(&(pbq->semr), &value);
    return (value <= 0);
}

void bmpqueue_write_request(BMPQUEUE *pbq, int64_t **ppts, BYTE **pbuf, int *stride)
{
    sem_wait(&(pbq->semw));

    if (ppts) *ppts = &(pbq->ppts[pbq->tail]);
    if (pbuf) *pbuf = pbq->pbmpbufs[pbq->tail];

    if (stride && pbq->hbitmaps) {
        BITMAP bitmap = {0};
        GetObject(pbq->hbitmaps[pbq->tail], sizeof(BITMAP), &bitmap);
        *stride = bitmap.bmWidthBytes;
    }
}

void bmpqueue_write_release(BMPQUEUE *pbq)
{
    sem_post(&(pbq->semw));
}

void bmpqueue_write_done(BMPQUEUE *pbq)
{
    if (++pbq->tail == pbq->size) pbq->tail = 0;
    sem_post(&(pbq->semr));
}

void bmpqueue_read_request(BMPQUEUE *pbq, int64_t **ppts, HBITMAP *hbitmap)
{
    sem_wait(&(pbq->semr));
    if (ppts   ) *ppts    = &(pbq->ppts[pbq->head]);
    if (hbitmap) *hbitmap = pbq->hbitmaps[pbq->head];
}

void bmpqueue_read_release(BMPQUEUE *pbq)
{
    sem_post(&(pbq->semr));
}

void bmpqueue_read_done(BMPQUEUE *pbq)
{
    if (++pbq->head == pbq->size) pbq->head = 0;
    sem_post(&(pbq->semw));
}





