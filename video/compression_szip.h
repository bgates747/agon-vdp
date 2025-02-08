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

// Define SZIP version numbers
#define MAJOR_VERSION 1
#define MINOR_VERSION 12

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
    szip_write_byte(stream, MAJOR_VERSION);
    szip_write_byte(stream, MINOR_VERSION);
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

// Writes a block header, including the extra 0x00 byte
static inline void szip_write_block_header(SzipBufferStream *stream, uint32_t uncompressed_size) {
    szip_write_byte(stream, 0x42); // 'B'
    szip_write_byte(stream, 0x48); // 'H'
    szip_write_uint3(stream, uncompressed_size);
    szip_write_byte(stream, 0x00); // Extra empty 'filename' byte
}

// Reads a block header, correctly skipping the orphaned byte
static inline int szip_read_block_header(SzipBufferStream *stream, uint32_t *uncompressed_size, uint8_t *block_type) {
    if (szip_read_byte(stream) != 0x42 || szip_read_byte(stream) != 0x48)
        return -1;
    
    *uncompressed_size = szip_read_uint3(stream);

    // Skip the extra orphaned byte (0x00)
    if (szip_read_byte(stream) != 0x00) {
        printf("ERROR: Expected orphaned byte 0x00 but found something else.\n");
        return -1;
    }

    // Read the block type (should be 0x01 for compressed)
    *block_type = szip_read_byte(stream);
    return 0;
}

// Reads a compressed block
static void szip_read_szip_block(SzipBufferStream *stream, uint8_t *buffer, uint32_t buflen, SzipConfig *config) {
    uint32_t index_last = szip_read_uint3(stream); // Read 3-byte index_last
    uint8_t order = szip_read_byte(stream); // Read order byte
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

// Decompress function for ESP32
void szip_decompress(uint8_t *input, uint32_t input_size, uint8_t *output, uint32_t *output_size, SzipConfig *config) {
    SzipBufferStream in_stream = {input, input_size, 0};
    SzipBufferStream out_stream = {output, *output_size, 0};

    if (szip_read_global_header(&in_stream) < 0) {
        printf("ERROR: Invalid SZIP global header!\n");
        return;
    }

    uint32_t uncompressed_size;
    uint8_t block_type;

    while (in_stream.pos < in_stream.size) {
        // Read block header correctly
        if (szip_read_block_header(&in_stream, &uncompressed_size, &block_type) < 0) {
            printf("ERROR: Failed to read block header!\n");
            return;
        }

        // Ensure block type is valid before proceeding
        if (block_type != 0x01) {
            printf("ERROR: Unexpected block type %02X! Expected 0x01 for compressed block.\n", block_type);
            return;
        }

        // Read and decompress block data
        szip_read_szip_block(&in_stream, output + out_stream.pos, uncompressed_size, config);
        out_stream.pos += uncompressed_size;
    }

    *output_size = out_stream.pos;
}

#ifdef __cplusplus
}
#endif

#endif // SZIP_VDP_H
