/* 标准头文件 */
#ifndef __STDEFINE_H__
#define __STDEFINE_H__

#if defined(WIN32)
#include <windows.h>
#include <mmsystem.h>
#include <tchar.h>
#define  usleep(us)  Sleep((us)/1000)
#else
// todo..
#endif

#endif


