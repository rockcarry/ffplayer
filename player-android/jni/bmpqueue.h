#ifndef _BMP_QUEUE_H_
#define _BMP_QUEUE_H_

// 包含头文件
#include <inttypes.h>
#include <semaphore.h>

// android
#include <android_runtime/AndroidRuntime.h>
#include <system/window.h>

// namespace android
using namespace android;

// 常量定义
#define DEF_BMP_QUEUE_SIZE   3

typedef struct {
    long      head;
    long      tail;
    long      size;
    sem_t     semr;
    sem_t     semw;
    int64_t  *ppts;
    sp<ANativeWindow> win;
} BMPQUEUE;

// 函数声明
bool bmpqueue_create (BMPQUEUE *pbq, sp<ANativeWindow> win, int w, int h);
void bmpqueue_destroy(BMPQUEUE *pbq);
bool bmpqueue_isempty(BMPQUEUE *pbq);

//++ 以下三个接口函数用于空闲可写 bitmap 的管理 ++//
// bmpqueue_write_request 取得当前可写的空闲 bitmap
// bmpqueue_write_release 释放当前可写的空闲 bitmap
// bmpqueue_write_post 完成写入操作
// 搭配使用方法：
// bmpqueue_write_request 将会返回 bitmap
// 填充 bitmap 的数据
// 如果填充成功，则执行
//     bmpqueue_write_post
// 如果填充失败，则执行
//     bmpqueue_write_release
void bmpqueue_write_request(BMPQUEUE *pbq, int64_t **ppts, uint8_t **pbuf, int *stride);
void bmpqueue_write_release(BMPQUEUE *pbq);
void bmpqueue_write_done   (BMPQUEUE *pbq);
//-- 以上三个接口函数用于空闲可写 bitmap 的管理 --//

//++ 以下三个接口函数用于空闲可读 bitmap 的管理 ++//
void bmpqueue_read_request(BMPQUEUE *pbq, int64_t **ppts, ANativeWindowBuffer **buf);
void bmpqueue_read_release(BMPQUEUE *pbq);
void bmpqueue_read_done   (BMPQUEUE *pbq);
//-- 以上三个接口函数用于空闲可读 bitmap 的管理 --//


#endif




