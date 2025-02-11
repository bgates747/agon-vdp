#include "szip_debug.h"

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
