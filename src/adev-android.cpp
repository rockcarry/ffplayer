// 包含头文件
#include "adev.h"

// 内部类型定义
typedef struct
{
    int reserved;
} ADEV_CONTEXT;

// 接口函数实现
void* adev_create(int type, int bufnum, int buflen)
{    return NULL;
}

void adev_destroy(void *ctxt)
{
}

void adev_request(void *ctxt, AUDIOBUF **ppab)
{
}

void adev_post(void *ctxt, int64_t pts)
{
}

void adev_pause(void *ctxt, int pause)
{
}

void adev_reset(void *ctxt)
{
}

void adev_syncapts(void *ctxt, int64_t *apts)
{
}

void adev_curdata(void *ctxt, void **buf, int *len)
{
}

void adev_setparam(void *ctxt, int id, void *param)
{
}

void adev_getparam(void *ctxt, int id, void *param)
{
}



