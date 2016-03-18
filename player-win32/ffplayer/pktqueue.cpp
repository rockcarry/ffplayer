// 包含头文件
#include <pthread.h>
#include <semaphore.h>
#include "pktqueue.h"
#include "log.h"

// 内部常量定义
#define DEF_PKT_QUEUE_SIZE 256

// 内部类型定义
typedef struct {
    long       fsize;
    long       asize;
    long       vsize;
    long       fhead;
    long       ftail;
    long       ahead;
    long       atail;
    long       vhead;
    long       vtail;
    sem_t      fsem;
    sem_t      asem;
    sem_t      vsem;
    AVPacket  *bpkts; // packet buffers
    AVPacket **fpkts; // free packets
    AVPacket **apkts; // audio packets
    AVPacket **vpkts; // video packets
} PKTQUEUE;

// 函数实现
void* pktqueue_create(int size)
{
    PKTQUEUE *ppq = (PKTQUEUE*)malloc(sizeof(PKTQUEUE));
    if (!ppq) {
        log_printf(TEXT("failed to allocate pktqueue context !\n"));
        exit(0);
    }

    memset(ppq, 0, sizeof(PKTQUEUE));
    ppq->fsize = size ? size : DEF_PKT_QUEUE_SIZE;
    ppq->asize = ppq->vsize = ppq->fsize;

    // alloc buffer & semaphore
    ppq->bpkts = (AVPacket* )malloc(ppq->fsize * sizeof(AVPacket ));
    ppq->fpkts = (AVPacket**)malloc(ppq->fsize * sizeof(AVPacket*));
    ppq->apkts = (AVPacket**)malloc(ppq->asize * sizeof(AVPacket*));
    ppq->vpkts = (AVPacket**)malloc(ppq->vsize * sizeof(AVPacket*));
    sem_init(&ppq->fsem, 0, ppq->fsize);
    sem_init(&ppq->asem, 0, 0         );
    sem_init(&ppq->vsem, 0, 0         );

    // check invalid
    if (  !ppq->bpkts || !ppq->fpkts || !ppq->apkts || !ppq->vpkts
       || !ppq->fsem  || !ppq->asem  || !ppq->vsem )
    {
        log_printf(TEXT("failed to allocate resources for pktqueue !\n"));
        exit(0);
    }

    // clear packets
    memset(ppq->bpkts, 0, ppq->fsize * sizeof(AVPacket ));
    memset(ppq->apkts, 0, ppq->asize * sizeof(AVPacket*));
    memset(ppq->vpkts, 0, ppq->vsize * sizeof(AVPacket*));

    // init fpkts
    int i;
    for (i=0; i<ppq->fsize; i++) {
        ppq->fpkts[i] = &ppq->bpkts[i];
    }

    return ppq;
}

void pktqueue_destroy(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;

    // close
    sem_destroy(&ppq->fsem);
    sem_destroy(&ppq->asem);
    sem_destroy(&ppq->vsem);

    // free
    free(ppq->bpkts);
    free(ppq->fpkts);
    free(ppq->apkts);
    free(ppq->vpkts);
    free(ppq);
}

void pktqueue_reset(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;

    int i;
    while (0 == sem_trywait(&ppq->fsem));
    while (0 == sem_trywait(&ppq->asem));
    while (0 == sem_trywait(&ppq->vsem));
    for (i=0; i<ppq->fsize; i++) {
        sem_post(&ppq->fsem);
    }

    ppq->fhead = ppq->ftail = 0;
    ppq->ahead = ppq->atail = 0;
    ppq->vhead = ppq->vtail = 0;

    for (i=0; i<ppq->fsize; i++) {
        ppq->fpkts[i] = &ppq->bpkts[i];
    }
}

BOOL pktqueue_write_request(void *ctxt, AVPacket **pppkt, int timeout)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    struct timespec t = { 0, timeout * 1000 };
    if (0 != sem_timedwait(&ppq->fsem, &t)) return FALSE;
    if (pppkt) *pppkt = ppq->fpkts[ppq->fhead];
    return TRUE;
}

void pktqueue_write_cancel(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    sem_post(&ppq->fsem);
}

void pktqueue_write_post_a(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->apkts[ppq->atail] = ppq->fpkts[ppq->fhead];

    if (++ppq->fhead == ppq->fsize) ppq->fhead = 0;
    if (++ppq->atail == ppq->asize) ppq->atail = 0;

    sem_post(&ppq->asem);
}

void pktqueue_write_post_v(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->vpkts[ppq->vtail] = ppq->fpkts[ppq->fhead];

    if (++ppq->fhead == ppq->fsize) ppq->fhead = 0;
    if (++ppq->vtail == ppq->vsize) ppq->vtail = 0;

    sem_post(&ppq->vsem);
}

BOOL pktqueue_read_request_a(void *ctxt, AVPacket **pppkt, int timeout)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    struct timespec t = { 0, timeout * 1000 };
    if (0 != sem_timedwait(&ppq->asem, &t)) return FALSE;
    if (pppkt) *pppkt = ppq->apkts[ppq->ahead];
    return TRUE;
}

void pktqueue_read_post_a(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->fpkts[ppq->ftail] = ppq->apkts[ppq->ahead];

    if (++ppq->ahead == ppq->asize) ppq->ahead = 0;
    if (++ppq->ftail == ppq->fsize) ppq->ftail = 0;

    sem_post(&ppq->fsem);
}

BOOL pktqueue_read_request_v(void *ctxt, AVPacket **pppkt, int timeout)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    struct timespec t = { 0, timeout * 1000 };
    if (0 != sem_timedwait(&ppq->vsem, &t)) return FALSE;
    if (pppkt) *pppkt = ppq->vpkts[ppq->vhead];
    return TRUE;
}

void pktqueue_read_post_v(void *ctxt)
{
    PKTQUEUE *ppq = (PKTQUEUE*)ctxt;
    ppq->fpkts[ppq->ftail] = ppq->vpkts[ppq->vhead];

    if (++ppq->vhead == ppq->vsize) ppq->vhead = 0;
    if (++ppq->ftail == ppq->fsize) ppq->ftail = 0;

    sem_post(&ppq->fsem);
}



