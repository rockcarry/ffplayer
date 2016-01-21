// 包含头文件
#include "pktqueue.h"

// 函数实现
BOOL pktqueue_create(PKTQUEUE *ppq)
{
    int i;

    // default size
    if (ppq->fsize == 0) {
        ppq->fsize = ppq->asize = ppq->vsize = DEF_PKT_QUEUE_SIZE;
    }

    // alloc buffer & semaphore
    ppq->bpkts = (AVPacket* )malloc(ppq->fsize * sizeof(AVPacket ));
    ppq->fpkts = (AVPacket**)malloc(ppq->fsize * sizeof(AVPacket*));
    ppq->apkts = (AVPacket**)malloc(ppq->asize * sizeof(AVPacket*));
    ppq->vpkts = (AVPacket**)malloc(ppq->vsize * sizeof(AVPacket*));
    sem_init(&(ppq->fsem), 0, ppq->fsize);
    sem_init(&(ppq->asem), 0, 0         );
    sem_init(&(ppq->vsem), 0, 0         );

    // check invalid
    if (!ppq->bpkts || !ppq->fpkts || !ppq->apkts || !ppq->vpkts) {
        pktqueue_destroy(ppq);
        return FALSE;
    }

    // clear packets
    memset(ppq->bpkts, 0, ppq->fsize * sizeof(AVPacket ));
    memset(ppq->apkts, 0, ppq->asize * sizeof(AVPacket*));
    memset(ppq->vpkts, 0, ppq->vsize * sizeof(AVPacket*));

    // init fpkts
    for (i=0; i<ppq->fsize; i++) {
        ppq->fpkts[i] = &(ppq->bpkts[i]);
    }
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
    sem_destroy(&(ppq->fsem));
    sem_destroy(&(ppq->asem));
    sem_destroy(&(ppq->vsem));

    // clear members
    memset(ppq, 0, sizeof(PKTQUEUE));
}

BOOL pktqueue_isempty_a(PKTQUEUE *ppq)
{
    int value = 0;
    sem_getvalue(&(ppq->asem), &value);
    return (value <= 0);
}

BOOL pktqueue_isempty_v(PKTQUEUE *ppq)
{
    int value = 0;
    sem_getvalue(&(ppq->vsem), &value);
    return (value <= 0);
}

void pktqueue_write_request(PKTQUEUE *ppq, AVPacket **pppkt)
{
    sem_wait(&(ppq->fsem));
    if (pppkt) *pppkt = ppq->fpkts[ppq->fhead];
}

void pktqueue_write_release(PKTQUEUE *ppq)
{
    sem_post(&(ppq->fsem));
}

void pktqueue_write_done_a(PKTQUEUE *ppq)
{
    ppq->apkts[ppq->atail] = ppq->fpkts[ppq->fhead];

    if (++ppq->fhead == ppq->fsize) ppq->fhead = 0;
    if (++ppq->atail == ppq->asize) ppq->atail = 0;

    sem_post(&(ppq->asem));
}

void pktqueue_write_done_v(PKTQUEUE *ppq)
{
    ppq->vpkts[ppq->vtail] = ppq->fpkts[ppq->fhead];

    if (++ppq->fhead == ppq->fsize) ppq->fhead = 0;
    if (++ppq->vtail == ppq->vsize) ppq->vtail = 0;

    sem_post(&(ppq->vsem));
}

void pktqueue_read_request_a(PKTQUEUE *ppq, AVPacket **pppkt)
{
    sem_wait(&(ppq->asem));
    if (pppkt) *pppkt = ppq->apkts[ppq->ahead];
}

void pktqueue_read_done_a(PKTQUEUE *ppq)
{
    ppq->fpkts[ppq->ftail] = ppq->apkts[ppq->ahead];

    if (++ppq->ahead == ppq->asize) ppq->ahead = 0;
    if (++ppq->ftail == ppq->fsize) ppq->ftail = 0;

    sem_post(&(ppq->fsem));
}

void pktqueue_read_request_v(PKTQUEUE *ppq, AVPacket **pppkt)
{
    sem_wait(&(ppq->vsem));
    if (pppkt) *pppkt = ppq->vpkts[ppq->vhead];
}

void pktqueue_read_done_v(PKTQUEUE *ppq)
{
    ppq->fpkts[ppq->ftail] = ppq->vpkts[ppq->vhead];

    if (++ppq->vhead == ppq->vsize) ppq->vhead = 0;
    if (++ppq->ftail == ppq->fsize) ppq->ftail = 0;

    sem_post(&(ppq->fsem));
}



