// szip_debug.h
#ifndef SZIP_DEBUG_H
#define SZIP_DEBUG_H

#include <stdarg.h>
#include <stdio.h>
#include "port.h"

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

void szip_hex_dump(const unsigned char *buf, uint4 len) {
    uint4 i;
    for (i = 0; i < len; i++) {
        szip_debug_log("%02X ", buf[i]);
        if ((i + 1) % 16 == 0)
        szip_debug_log("\n");
    }
    if (len % 16 != 0)
    szip_debug_log("\n");
}

#endif  // SZIP_DEBUG_H
