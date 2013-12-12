#ifndef _WAVBUF_QUEUE_H_
#define _WAVBUF_QUEUE_H_

// 包含头文件
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
#define DEF_WAVBUF_QUEUE_SIZE   32
#define DEF_WAVBUF_BUFFER_SIZE  4096

typedef struct {
    long      head;
    long      tail;
    long      size;
    long      curnum;
    HANDLE    semr;
    HANDLE    semw;
    WAVEHDR  *pwhdrs;
    HWAVEOUT  hwavout;
} WAVBUFQUEUE;

// 函数声明
BOOL wavbufqueue_create (WAVBUFQUEUE *pwq, HWAVEOUT h);
void wavbufqueue_destroy(WAVBUFQUEUE *pwq);
void wavbufqueue_flush  (WAVBUFQUEUE *pwq);
BOOL wavbufqueue_isempty(WAVBUFQUEUE *pwq);

//++ 以下三个接口函数用于空闲可写 wavehdr 的管理 ++//
// wavbufqueue_write_request 取得当前可写的空闲 wavehdr
// wavbufqueue_write_release 释放当前可写的空闲 wavehdr
// wavbufqueue_write_post 完成写入操作
// 搭配使用方法：
// wavbufqueue_write_request 将会返回 wavehdr
// 填充 wavehdr 的数据
// 如果填充成功，则执行
//     wavbufqueue_write_post
// 如果填充失败，则执行
//     wavbufqueue_write_release
void wavbufqueue_write_request(WAVBUFQUEUE *pwq, PWAVEHDR *pwhdr);
void wavbufqueue_write_release(WAVBUFQUEUE *pwq);
void wavbufqueue_write_done   (WAVBUFQUEUE *pwq);
//-- 以下三个接口函数用于空闲可写 wavehdr 的管理 --//

//++ 以下三个接口函数用于空闲可读 wavehdr 的管理 ++//
void wavbufqueue_read_request(WAVBUFQUEUE *pwq, PWAVEHDR *pwhdr);
void wavbufqueue_read_release(WAVBUFQUEUE *pwq);
void wavbufqueue_read_done   (WAVBUFQUEUE *pwq);
//-- 以下三个接口函数用于空闲可读 wavehdr 的管理 --//

#ifdef __cplusplus
}
#endif

#endif




