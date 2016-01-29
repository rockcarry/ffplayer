#ifndef _PKT_QUEUE_H_
#define _PKT_QUEUE_H_

// 包含头文件
#include <semaphore.h>
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

// avformat.h
#include "libavformat/avformat.h"

// 常量定义
#define DEF_PKT_QUEUE_SIZE 120

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

// 函数声明
BOOL pktqueue_create (PKTQUEUE *ppq);
void pktqueue_destroy(PKTQUEUE *ppq);

BOOL pktqueue_isempty_a(PKTQUEUE *ppq);
BOOL pktqueue_isempty_v(PKTQUEUE *ppq);

void pktqueue_write_request(PKTQUEUE *ppq, AVPacket **pppkt);
void pktqueue_write_release(PKTQUEUE *ppq);
void pktqueue_write_done_a(PKTQUEUE *ppq);
void pktqueue_write_done_v(PKTQUEUE *ppq);

void pktqueue_read_request_a(PKTQUEUE *ppq, AVPacket **pppkt);
void pktqueue_read_done_a  (PKTQUEUE *ppq);

void pktqueue_read_request_v(PKTQUEUE *ppq, AVPacket **pppkt);
void pktqueue_read_done_v  (PKTQUEUE *ppq);

#ifdef __cplusplus
}
#endif

#endif





