#ifndef _BMPBUF_QUEUE_H_
#define _BMPBUF_QUEUE_H_

// 包含头文件
#include <windows.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
#define DEF_BMPBUF_QUEUE_SIZE   32

typedef struct {
    long      head;
    long      tail;
    long      size;
    long      curnum;
    HANDLE    semr;
    HANDLE    semw;
    int64_t  *ppts;
    HBITMAP  *hbitmaps;
    BYTE    **pbmpbufs;
} BMPBUFQUEUE;

// 函数声明
BOOL bmpbufqueue_create (BMPBUFQUEUE *pbq, HDC hdc, int w, int h, int cdepth);
void bmpbufqueue_destroy(BMPBUFQUEUE *pbq);
void bmpbufqueue_flush  (BMPBUFQUEUE *pbq);
BOOL bmpbufqueue_isempty(BMPBUFQUEUE *pbq);

//++ 以下三个接口函数用于空闲可写 bitmap 的管理 ++//
// bmpbufqueue_write_request 取得当前可写的空闲 bitmap
// bmpbufqueue_write_release 释放当前可写的空闲 bitmap
// bmpbufqueue_write_post 完成写入操作
// 搭配使用方法：
// bmpbufqueue_write_request 将会返回 bitmap
// 填充 bitmap 的数据
// 如果填充成功，则执行
//     bmpbufqueue_write_post
// 如果填充失败，则执行
//     bmpbufqueue_write_release
void bmpbufqueue_write_request(BMPBUFQUEUE *pbq, int64_t **ppts, BYTE **pbuf, int *stride);
void bmpbufqueue_write_release(BMPBUFQUEUE *pbq);
void bmpbufqueue_write_done   (BMPBUFQUEUE *pbq);
//-- 以上三个接口函数用于空闲可写 bitmap 的管理 --//

//++ 以下三个接口函数用于空闲可读 bitmap 的管理 ++//
void bmpbufqueue_read_request(BMPBUFQUEUE *pbq, int64_t **ppts, HBITMAP *hbitmap);
void bmpbufqueue_read_release(BMPBUFQUEUE *pbq);
void bmpbufqueue_read_done   (BMPBUFQUEUE *pbq);
//-- 以上三个接口函数用于空闲可读 bitmap 的管理 --//

#ifdef __cplusplus
}
#endif

#endif




