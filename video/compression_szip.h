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

extern void debug_log(const char * format, ...);

#pragma pack(push, 1)
typedef struct {
    uint8_t marker[3];   // "SZ\n"
    uint8_t type;        // Always 0x04
    uint8_t major;       // Version major
    uint8_t minor;       // Version minor
} CompressionSzipFileHeader;
#pragma pack(pop)

typedef struct {
    void* context;
    uint32_t input_count;
    uint32_t output_count;
    uint32_t block_size;
    uint8_t* input_buffer;
    uint8_t* output_buffer;
    uint32_t input_pos;
    uint32_t output_pos;
    uint8_t recordsize;
    sz_model model;
} SzipDecompressionData;

void szip_init_decompression(SzipDecompressionData* dd, void* context) {
    memset(dd, 0, sizeof(SzipDecompressionData));
    dd->context = context;
    dd->recordsize = 1;
}

#endif // SZIP_VDP_H
