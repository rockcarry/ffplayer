#ifndef _FFPLAYER_LOG_H_
#define _FFPLAYER_LOG_H_

/* º¯ÊýÉùÃ÷ */
void log_init  (TCHAR *file);
void log_done  (void);
void log_printf(TCHAR *format, ...);

#endif
