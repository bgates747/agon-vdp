#ifndef SZIP_DEBUG_H
#define SZIP_DEBUG_H

#include <stdarg.h>
#include <stdio.h>
#include "port.h"

#define SZ_DEBUG 0  // Enable debugging output

// Declare the functions (not define)
void szip_debug_log(const char *format, ...);
void szip_hex_dump(const unsigned char *buf, uint4 len);

#endif  // SZIP_DEBUG_H
