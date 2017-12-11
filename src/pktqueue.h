#ifndef __FFPLAYER_PKTQUEUE_H__
#define __FFPLAYER_PKTQUEUE_H__

// 包含头文件
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

// avformat.h
#include "libavformat/avformat.h"

// 函数声明
void* pktqueue_create (int   size); // important!! size must be power of 2
void  pktqueue_destroy(void *ctxt);
void  pktqueue_reset  (void *ctxt);

AVPacket* pktqueue_write_dequeue  (void *ctxt); // dequeue a packet for writing
void      pktqueue_write_enqueue_a(void *ctxt, AVPacket *pkt); // enqueue a packet as audio
void      pktqueue_write_enqueue_v(void *ctxt, AVPacket *pkt); // enqueue a packet as video
void      pktqueue_write_cancel   (void *ctxt, AVPacket *pkt); // cancel current packet writing

AVPacket* pktqueue_read_dequeue_a (void *ctxt); // dequeue a audio packet for reading
void      pktqueue_read_enqueue_a (void *ctxt, AVPacket *pkt); // enqueue a audio packet for reading
AVPacket* pktqueue_read_dequeue_v (void *ctxt); // dequeue a video packet for reading
void      pktqueue_read_enqueue_v (void *ctxt, AVPacket *pkt); // enqueue a video packet for reading

#ifdef __cplusplus
}
#endif

#endif





