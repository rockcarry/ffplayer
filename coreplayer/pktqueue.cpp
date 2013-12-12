// 包含头文件
#include <windows.h>
#include "pktqueue.h"

// 函数实现
BOOL pktqueue_create(PKTQUEUE *ppq)
{
    int i;

    // default size
    if (ppq->fsize == 0) ppq->fsize = DEF_PKT_QUEUE_FSIZE;
    if (ppq->asize == 0) ppq->asize = DEF_PKT_QUEUE_ASIZE;
    if (ppq->vsize == 0) ppq->vsize = DEF_PKT_QUEUE_VSIZE;

    // alloc buffer & semaphore
    ppq->bpkts = (AVPacket* )malloc(ppq->fsize * sizeof(AVPacket ));
    ppq->fpkts = (AVPacket**)malloc(ppq->fsize * sizeof(AVPacket*));
    ppq->apkts = (AVPacket**)malloc(ppq->asize * sizeof(AVPacket*));
    ppq->vpkts = (AVPacket**)malloc(ppq->vsize * sizeof(AVPacket*));
    ppq->fsemr = CreateSemaphore(NULL, ppq->fsize, ppq->fsize, NULL);
    ppq->asemr = CreateSemaphore(NULL, 0         , ppq->asize, NULL);
    ppq->asemw = CreateSemaphore(NULL, ppq->asize, ppq->asize, NULL);
    ppq->vsemr = CreateSemaphore(NULL, 0         , ppq->vsize, NULL);
    ppq->vsemw = CreateSemaphore(NULL, ppq->vsize, ppq->vsize, NULL);

    // check invalid
    if (  !ppq->bpkts || !ppq->fpkts || !ppq->apkts || !ppq->vpkts
       || !ppq->fsemr || !ppq->asemr || !ppq->asemw || !ppq->vsemr || !ppq->vsemw) {
        pktqueue_destroy(ppq);
        return FALSE;
    }

    // clear packets
    memset(ppq->bpkts, 0, ppq->fsize * sizeof(AVPacket ));
    memset(ppq->apkts, 0, ppq->asize * sizeof(AVPacket*));
    memset(ppq->vpkts, 0, ppq->vsize * sizeof(AVPacket*));

    // init fpkts & fpktnum
    for (i=0; i<ppq->fsize; i++) {
        ppq->fpkts[i] = &(ppq->bpkts[i]);
    }
    ppq->fpktnum = ppq->fsize;
    return TRUE;
}

void pktqueue_destroy(PKTQUEUE *ppq)
{
    // free
    if (ppq->bpkts) free(ppq->bpkts);
    if (ppq->fpkts) free(ppq->fpkts);
    if (ppq->apkts) free(ppq->apkts);
    if (ppq->vpkts) free(ppq->vpkts);

    // close
    if (ppq->fsemr) CloseHandle(ppq->fsemr);
    if (ppq->asemr) CloseHandle(ppq->asemr);
    if (ppq->asemw) CloseHandle(ppq->asemw);
    if (ppq->vsemr) CloseHandle(ppq->vsemr);
    if (ppq->vsemw) CloseHandle(ppq->vsemw);

    // clear members
    memset(ppq, 0, sizeof(PKTQUEUE));
}

void pktqueue_flush(PKTQUEUE *ppq)
{
    while (ppq->apktnum > 0) {
        pktqueue_read_request_a(ppq, NULL);
        pktqueue_read_done_a(ppq);
    }
    while (ppq->vpktnum > 0) {
        pktqueue_read_request_v(ppq, NULL);
        pktqueue_read_done_v(ppq);
    }
}

void pktqueue_write_request(PKTQUEUE *ppq, AVPacket **pppkt)
{
    WaitForSingleObject(ppq->fsemr, -1);
    if (pppkt) *pppkt = ppq->fpkts[ppq->fhead];
}

void pktqueue_write_release(PKTQUEUE *ppq)
{
    ReleaseSemaphore(ppq->fsemr, 1, NULL);
}

void pktqueue_write_done_a(PKTQUEUE *ppq)
{
    WaitForSingleObject(ppq->asemw, -1);
    ppq->apkts[ppq->atail] = ppq->fpkts[ppq->fhead];

    InterlockedIncrement(&(ppq->fhead));
    InterlockedCompareExchange(&(ppq->fhead), 0, ppq->fsize);
    InterlockedDecrement(&(ppq->fpktnum));

    InterlockedIncrement(&(ppq->atail));
    InterlockedCompareExchange(&(ppq->atail), 0, ppq->asize);
    InterlockedIncrement(&(ppq->apktnum));

    ReleaseSemaphore(ppq->asemr, 1, NULL);
}

void pktqueue_write_done_v(PKTQUEUE *ppq)
{
    WaitForSingleObject(ppq->vsemw, -1);
    ppq->vpkts[ppq->vtail] = ppq->fpkts[ppq->fhead];

    InterlockedIncrement(&(ppq->fhead));
    InterlockedCompareExchange(&(ppq->fhead), 0, ppq->fsize);
    InterlockedDecrement(&(ppq->fpktnum));

    InterlockedIncrement(&(ppq->vtail));
    InterlockedCompareExchange(&(ppq->vtail), 0, ppq->vsize);
    InterlockedIncrement(&(ppq->vpktnum));

    ReleaseSemaphore(ppq->vsemr, 1, NULL);
}

BOOL pktqueue_isempty_a(PKTQUEUE *ppq)
{
    return (ppq->apktnum <= 0);
}

BOOL pktqueue_isempty_v(PKTQUEUE *ppq)
{
    return (ppq->vpktnum <= 0);
}

void pktqueue_read_request_a(PKTQUEUE *ppq, AVPacket **pppkt)
{
    WaitForSingleObject(ppq->asemr, -1);
    if (pppkt) *pppkt = ppq->apkts[ppq->ahead];
}

void pktqueue_read_done_a(PKTQUEUE *ppq)
{
    ppq->fpkts[ppq->ftail] = ppq->apkts[ppq->ahead];

    InterlockedIncrement(&(ppq->ahead));
    InterlockedCompareExchange(&(ppq->ahead), 0, ppq->asize);
    InterlockedDecrement(&(ppq->apktnum));

    InterlockedIncrement(&(ppq->ftail));
    InterlockedCompareExchange(&(ppq->ftail), 0, ppq->fsize);
    InterlockedIncrement(&(ppq->fpktnum));

    ReleaseSemaphore(ppq->asemw, 1, NULL);
    ReleaseSemaphore(ppq->fsemr, 1, NULL);
}

void pktqueue_read_request_v(PKTQUEUE *ppq, AVPacket **pppkt)
{
    WaitForSingleObject(ppq->vsemr, -1);
    if (pppkt) *pppkt = ppq->vpkts[ppq->vhead];
}

void pktqueue_read_done_v(PKTQUEUE *ppq)
{
    ppq->fpkts[ppq->ftail] = ppq->vpkts[ppq->vhead];

    InterlockedIncrement(&(ppq->vhead));
    InterlockedCompareExchange(&(ppq->vhead), 0, ppq->vsize);
    InterlockedDecrement(&(ppq->vpktnum));

    InterlockedIncrement(&(ppq->ftail));
    InterlockedCompareExchange(&(ppq->ftail), 0, ppq->fsize);
    InterlockedIncrement(&(ppq->fpktnum));

    ReleaseSemaphore(ppq->vsemw, 1, NULL);
    ReleaseSemaphore(ppq->fsemr, 1, NULL);
}



