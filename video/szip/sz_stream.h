#ifndef SZ_STREAM_H
#define SZ_STREAM_H

#include "port.h"
#define EOF (-1)

/* Struct for buffer-based processing */
typedef struct {
    unsigned char *data;
    uint4 size;
    uint4 pos;
} SzipBufferStream;

/* Declare global stream variable */
extern SzipBufferStream *szip_global_stream;  // Global stream pointer

/* Read a byte from the global stream buffer */
static inline int sz_stream_getchar() {
    return (szip_global_stream->pos < szip_global_stream->size) 
        ? szip_global_stream->data[szip_global_stream->pos++] 
        : EOF;
}

/* Read a 3-byte integer from the global stream buffer */
static inline uint4 sz_stream_readuint3() {
    uint4 x = sz_stream_getchar();
    x = (x << 8) | sz_stream_getchar();
    x = (x << 8) | sz_stream_getchar();
    return x;
}

#endif // SZ_STREAM_H
