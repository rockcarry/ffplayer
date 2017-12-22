/* 标准头文件 */
#ifndef __STDEFINE_H__
#define __STDEFINE_H__


#ifdef WIN32
// headers
#include <windows.h>
#include <inttypes.h>

// usleep
#define usleep(us)  Sleep((us)/1000)

// configs
#define CONFIG_ENABLE_VEFFECT   1
#define CONFIG_ENABLE_SNAPSHOT  1
#endif


#ifdef ANDROID
// headers
#undef  LOG_TAG
#define LOG_TAG "ffplayer"
#include <limits.h>
#include <utils/Log.h>

#define MAX_PATH   PATH_MAX
#define sprintf_s  sprintf
#define strcpy_s   strcpy

typedef struct {
    long left;
    long top;
    long right;
    long bottom;
} RECT;

// configs
#define CONFIG_ENABLE_VEFFECT   0
#define CONFIG_ENABLE_SNAPSHOT  0
#endif


#define DO_USE_VAR(a) do { a = a; } while (0)


#endif


