#ifndef SZIP_VDP_H
#define SZIP_VDP_H

#include <stdint.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>
#include "szip/port.h"
#include "szip/sz_mod4.h"
#include "szip/sz_srt.h"
#include "szip/reorder.h"

#define SZIP_BLOCK_SIZE (1 << 16) // Define SIZE_SHIFT if needed

extern void debug_log(const char * format, ...);

typedef void (*WriteCompressedByte)(void*, uint8_t);
typedef bool (*WriteDecompressedByte)(void*, uint8_t);

typedef struct {
    void* context;
    WriteCompressedByte write_fcn;
    uint8_t* buffer;
    uint32_t buffer_size;
} SzipCompressionData;

typedef struct {
    void* context;
    WriteDecompressedByte write_fcn;
    uint8_t* buffer;
    uint32_t buffer_size;
} SzipDecompressionData;

static void writeuint3(uint4 x) {
    putchar((char)((x>>16)&0xff));
    putchar((char)((x>>8)&0xff));
    putchar((char)(x&0xff));
}

static uint4 readuint3() {
    uint4 x;
    x = getchar();
    x = x<<8 | getchar();
    x = x<<8 | getchar();
    return x;
}

void szip_init_compression(SzipCompressionData* cd, void* context, WriteCompressedByte write_fcn) {
    memset(cd, 0, sizeof(SzipCompressionData));
    cd->context = context;
    cd->write_fcn = write_fcn;
    cd->buffer = (uint8_t*) ps_malloc(SZIP_BLOCK_SIZE);
    cd->buffer_size = SZIP_BLOCK_SIZE;
    if (!cd->buffer) debug_log("szip_init_compression: buffer allocation failed\n");
}

void szip_compress(SzipCompressionData* cd, uint8_t* input, uint32_t input_size) {
    if (!cd->buffer) return;

    uint4 indexlast;
    sz_model m;
    initmodel(&m, 0, NULL);

    sz_srt(input, input_size, &indexlast, 6);
    writeuint3(indexlast);
    putchar(6);

    uint8_t* end = input + input_size;
    uint8_t* ptr = input;
    while (ptr < end) {
        uint8_t ch = *ptr++;
        uint4 runlength = 1;
        while (ptr < end && *ptr == ch) {
            runlength++;
            ptr++;
        }
        sz_encode(&m, ch, runlength);
    }
    deletemodel(&m);
}

void szip_init_decompression(SzipDecompressionData* dd, void* context, WriteDecompressedByte write_fcn) {
    memset(dd, 0, sizeof(SzipDecompressionData));
    dd->context = context;
    dd->write_fcn = write_fcn;
    dd->buffer = (uint8_t*) ps_malloc(SZIP_BLOCK_SIZE);
    dd->buffer_size = SZIP_BLOCK_SIZE;
    if (!dd->buffer) debug_log("szip_init_decompression: buffer allocation failed\n");
}

void szip_decompress(SzipDecompressionData* dd, uint8_t* input, uint32_t input_size) {
    if (!dd->buffer) return;

    sz_model m;
    initmodel(&m, -1, NULL);

    uint4 indexlast = readuint3();
    uint order = getchar();

    uint8_t* output = dd->buffer;
    uint32_t bytes_left = input_size;
    while (bytes_left) {
        uint4 runlength;
        uint ch;
        sz_decode(&m, &ch, &runlength);
        if (runlength > bytes_left) {
            debug_log("Decompression error\n");
            return;
        }
        bytes_left -= runlength;
        while (runlength--) {
            *(output++) = ch;
        }
    }
    deletemodel(&m);
}

#endif // SZIP_VDP_H
