#ifndef _PLAYER_H_
#define _PLAYER_H_

// 包含头文件
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
// message
#define MSG_COREPLAYER  (WM_APP + 1)

// player status
enum {
    PLAYER_STOP,
    PLAYER_PLAY,
    PLAYER_PAUSE,
    PLAYER_SEEK,
};

// render mode
enum {
    RENDER_LETTERBOX,
    RENDER_STRETCHED,
};

// param
enum {
    PARAM_VIDEO_WIDTH,
    PARAM_VIDEO_HEIGHT,
    PARAM_GET_DURATION,
    PARAM_GET_POSITION,
    PARAM_RENDER_MODE,
};

// 函数声明
HANDLE playeropen    (char *file, HWND hwnd);
void   playerclose   (HANDLE hplayer);
void   playerplay    (HANDLE hplayer);
void   playerpause   (HANDLE hplayer);
void   playerstop    (HANDLE hplayer);
void   playerseek    (HANDLE hplayer, DWORD sec);
void   playersetrect (HANDLE hplayer, int x, int y, int w, int h);
void   playersetparam(HANDLE hplayer, DWORD id, DWORD param);
void   playergetparam(HANDLE hplayer, DWORD id, void *param);

#ifdef __cplusplus
}
#endif

#endif















