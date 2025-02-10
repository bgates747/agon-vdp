// szip_debug.h
#ifndef SZIP_DEBUG_H
#include <stdarg.h>
#include <stdio.h>

#define SZ_DEBUG 1  // Enable debugging output

void szip_debug_log(const char *format, ...) {
    #if SZ_DEBUG == 1
    va_list ap;
    va_start(ap, format);

    // Print directly to stderr for immediate debugging output
    vfprintf(stderr, format, ap);
    fflush(stderr);  // Force immediate output

    va_end(ap);
    #endif
}

#endif  // SZIP_DEBUG_H
