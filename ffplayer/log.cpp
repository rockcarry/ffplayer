/* 包含头文件 */
#include <windows.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <tchar.h>
#include "log.h"

// 内部常量定义
#define LOG_MODE_DISABLE  0
#define LOG_MODE_FILE     1
#define LOG_MODE_DEBUGER  2

// 内部全部变量定义
static FILE *s_log_fp   = NULL;
static DWORD s_log_mode = LOG_MODE_DISABLE;

/* 函数实现 */
void log_init(TCHAR *file)
{
    if (!s_log_fp) {
        if (_tcscmp(file, TEXT("DEBUGER")) == 0) {
            s_log_mode = LOG_MODE_DEBUGER;
        }
        else {
            _tfopen_s(&s_log_fp, file, TEXT("w"));
            if (s_log_fp) {
                s_log_mode = LOG_MODE_FILE;
            }
        }
    }
}

void log_done(void)
{
    if (s_log_fp) {
        fflush(s_log_fp);
        fclose(s_log_fp);
        s_log_fp   = NULL;
        s_log_mode = 0;
    }
}

#define MAX_LOG_BUF 1024
void log_printf(TCHAR *format, ...)
{
    TCHAR buf[MAX_LOG_BUF];
    va_list valist;

    // if debug log is not enable, directly return
    if (s_log_mode == LOG_MODE_DISABLE) return;

    va_start(valist, format);
    _vstprintf_s(buf, MAX_LOG_BUF, format, valist);
    va_end(valist);

    switch (s_log_mode)
    {
    case LOG_MODE_FILE:
       _fputts(buf, s_log_fp);
        fflush(s_log_fp);
        break;

    case LOG_MODE_DEBUGER:
        OutputDebugString(buf);
        break;
    }
}
