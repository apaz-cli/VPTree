#ifndef __LOG
#define __LOG

#define DEBUG 0

#if DEBUG
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#define LOG(format, message) \
    printf(format, message); \
    fflush(stdout);

#define LOGs(message)        \
    printf("%s\n", message); \
    fflush(stdout);

void debug_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

#else
void debug_printf(const char *fmt, ...) {}
#endif
#ifndef LOG
#define LOG(format, message) ;
#define LOGs(format) ;
#endif
#endif
