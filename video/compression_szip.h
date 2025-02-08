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

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct {
    uint8_t marker[3];   // "SZ\n"
    uint8_t type;        // Always 0x04
    uint8_t major;       // Version major
    uint8_t minor;       // Version minor
} SzipFileHeader;
#pragma pack(pop)

// Struct for managing buffer-based streaming operations
typedef struct {
    uint8_t *data;
    size_t size;
    size_t pos;
} SzipBufferStream;

// SZIP Compression Configuration
typedef struct {
    uint32_t block_size;
    uint8_t order;
    uint8_t verbosity;
    uint8_t recordsize;
} SzipConfig;

// Reads a byte from the stream
static inline int szip_read_byte(SzipBufferStream *stream) {
    return (stream->pos >= stream->size) ? EOF : stream->data[stream->pos++];
}

// Writes a byte to the stream
static inline void szip_write_byte(SzipBufferStream *stream, uint8_t byte) {
    if (stream->pos < stream->size) {
        stream->data[stream->pos++] = byte;
    }
}

// Reads a 3-byte big-endian integer
static inline uint32_t szip_read_uint3(SzipBufferStream *stream) {
    uint32_t value = szip_read_byte(stream);
    value = (value << 8) | szip_read_byte(stream);
    value = (value << 8) | szip_read_byte(stream);
    return value;
}

// Writes a 3-byte big-endian integer
static inline void szip_write_uint3(SzipBufferStream *stream, uint32_t value) {
    szip_write_byte(stream, (value >> 16) & 0xFF);
    szip_write_byte(stream, (value >> 8) & 0xFF);
    szip_write_byte(stream, value & 0xFF);
}

// Writes the global SZIP header
static inline void szip_write_global_header(SzipBufferStream *stream) {
    szip_write_byte(stream, 0x53); // 'S'
    szip_write_byte(stream, 0x5A); // 'Z'
    szip_write_byte(stream, 0x0A); // Newline
    szip_write_byte(stream, 0x04); // Type
    szip_write_byte(stream, 0x01); // Version Major
    szip_write_byte(stream, 0x0B); // Version Minor
}

// Reads and validates the SZIP global header
static inline int szip_read_global_header(SzipBufferStream *stream) {
    if (szip_read_byte(stream) != 0x53 || szip_read_byte(stream) != 0x5A ||
        szip_read_byte(stream) != 0x0A || szip_read_byte(stream) != 0x04)
        return -1;

    int vmay = szip_read_byte(stream);
    int vmin = szip_read_byte(stream);
    return (vmay == EOF || vmin == EOF) ? -1 : 0;
}

// Writes a block header
static inline void szip_write_block_header(SzipBufferStream *stream, uint32_t uncompressed_size) {
    szip_write_byte(stream, 0x42); // 'B'
    szip_write_byte(stream, 0x48); // 'H'
    szip_write_uint3(stream, uncompressed_size);
}

// Reads a block header
static inline int szip_read_block_header(SzipBufferStream *stream, uint32_t *uncompressed_size) {
    if (szip_read_byte(stream) != 0x42 || szip_read_byte(stream) != 0x48)
        return -1;
    *uncompressed_size = szip_read_uint3(stream);
    return 0;
}

// Compress a block using SZIP
static void szip_write_szip_block(SzipBufferStream *stream, uint8_t *buffer, uint32_t buflen, SzipConfig *config) {
    uint32_t index_last;
    SzipModel m;

    szip_write_byte(stream, 1); // 1 means szip block
    szip_write_uint3(stream, index_last); // Placeholder, filled after sorting
    szip_write_byte(stream, config->order);

    initmodel(&m, 0, &(config->recordsize));

    uint8_t *end = buffer + buflen;
    *end = ~*(end - 1); // Ensure end of run

    uint8_t *begin = buffer;
    uint8_t ch = *(buffer++);
    while (*buffer == ch) buffer++;
    sz_encode(&m, ch, buffer - begin);

    fixafterfirst(&m);
    while (buffer < end) {
        begin = buffer;
        ch = *(buffer++);
        while (*buffer == ch) buffer++;
        sz_encode(&m, ch, buffer - begin);
    }

    deletemodel(&m);
}

// Decompress a block using SZIP
static void szip_read_szip_block(SzipBufferStream *stream, uint8_t *buffer, uint32_t buflen, SzipConfig *config) {
    uint32_t index_last = szip_read_uint3(stream);
    uint8_t order = szip_read_byte(stream);
    uint32_t bytes_left = buflen;
    SzipModel m;

    initmodel(&m, -1, &(config->recordsize));

    uint32_t runlength;
    uint32_t ch;
    sz_decode(&m, &ch, &runlength);
    if (runlength > bytes_left) return;
    bytes_left -= runlength;
    memset(buffer, ch, runlength);

    fixafterfirst(&m);
    buffer += runlength;

    while (bytes_left) {
        sz_decode(&m, &ch, &runlength);
        if (runlength > bytes_left) return;
        bytes_left -= runlength;
        memset(buffer, ch, runlength);
        buffer += runlength;
    }

    deletemodel(&m);
}

// Compress function for ESP32
void szip_compress(uint8_t *input, uint32_t input_size, uint8_t *output, uint32_t *output_size, SzipConfig *config) {
    SzipBufferStream in_stream = {input, input_size, 0};
    SzipBufferStream out_stream = {output, *output_size, 0};

    szip_write_global_header(&out_stream);

    uint32_t remaining = input_size;
    while (remaining > 0) {
        uint32_t chunk_size = (remaining > config->block_size) ? config->block_size : remaining;
        szip_write_block_header(&out_stream, chunk_size);
        szip_write_szip_block(&out_stream, input + (input_size - remaining), chunk_size, config);
        remaining -= chunk_size;
    }

    *output_size = out_stream.pos;
}

// Decompress function for ESP32
void szip_decompress(uint8_t *input, uint32_t input_size, uint8_t *output, uint32_t *output_size, SzipConfig *config) {
    SzipBufferStream in_stream = {input, input_size, 0};
    SzipBufferStream out_stream = {output, *output_size, 0};

    if (szip_read_global_header(&in_stream) < 0) return;

    uint32_t uncompressed_size;
    while (in_stream.pos < in_stream.size) {
        if (szip_read_block_header(&in_stream, &uncompressed_size) < 0) return;
        szip_read_szip_block(&in_stream, output + out_stream.pos, uncompressed_size, config);
        out_stream.pos += uncompressed_size;
    }

    *output_size = out_stream.pos;
}

#ifdef __cplusplus
}
#endif

#endif // SZIP_VDP_H
