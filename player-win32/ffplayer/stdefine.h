/* 标准头文件 */
#ifndef _STDEFINE_H_
#define _STDEFINE_H_

#if defined(WIN32)
#include <windows.h>
#include <mmsystem.h>
#include <tchar.h>
#define  usleep(us)  Sleep((us)/1000)
#else
// todo..
#endif

#endif


