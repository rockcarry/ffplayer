#ifndef _WAV_QUEUE_H_
#define _WAV_QUEUE_H_

// 包含头文件
#include <inttypes.h>
#include <semaphore.h>
#include "stdefine.h"

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
#define DEF_WAV_QUEUE_SIZE   3
#define DEF_WAV_BUFFER_SIZE  8192

typedef struct {
    long      head;
    long      tail;
    long      size;
    sem_t     semr;
    sem_t     semw;
    int64_t  *ppts;
    WAVEHDR  *pwhdrs;
    void     *adev;
} WAVQUEUE;

// 函数声明
BOOL wavqueue_create (WAVQUEUE *pwq, void *adev, int wavbufsize);
void wavqueue_destroy(WAVQUEUE *pwq);
BOOL wavqueue_isempty(WAVQUEUE *pwq);

//++ 以下三个接口函数用于空闲可写 wavehdr 的管理 ++//
// wavqueue_write_request 取得当前可写的空闲 wavehdr
// wavqueue_write_release 释放当前可写的空闲 wavehdr
// wavqueue_write_post 完成写入操作
// 搭配使用方法：
// wavqueue_write_request 将会返回 wavehdr
// 填充 wavehdr 的数据
// 如果填充成功，则执行
//     wavqueue_write_post
// 如果填充失败，则执行
//     wavqueue_write_release
void wavqueue_write_request(WAVQUEUE *pwq, int64_t **ppts, PWAVEHDR *pwhdr);
void wavqueue_write_release(WAVQUEUE *pwq);
void wavqueue_write_done   (WAVQUEUE *pwq);
//-- 以下三个接口函数用于空闲可写 wavehdr 的管理 --//

//++ 以下三个接口函数用于空闲可读 wavehdr 的管理 ++//
void wavqueue_read_request(WAVQUEUE *pwq, int64_t **ppts, PWAVEHDR *pwhdr);
void wavqueue_read_release(WAVQUEUE *pwq);
void wavqueue_read_done   (WAVQUEUE *pwq);
//-- 以下三个接口函数用于空闲可读 wavehdr 的管理 --//

#ifdef __cplusplus
}
#endif

#endif




