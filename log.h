#ifndef __LOG
#define __LOG

#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#define LOG(format, message) printf(format, message); fflush(stdout);

#define LOGs(message) printf("%s\n", message); fflush(stdout);

static inline void 
debug_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

#else
#define LOG(format, message) ;
#define LOGs(format) ;
static inline void 
debug_printf(const char *fmt, ...) { (void)fmt; }
#endif

#endif  // Include guard
