#ifndef SZIP_VDP_H
#define SZIP_VDP_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
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

// Define an output-chunk size for dynamic buffer expansion
#define OUTPUT_CHUNK_SIZE 256

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

// Dynamic output buffer for decompression
typedef struct {
    uint8_t *data;
    size_t used;      // bytes written so far
    size_t allocated; // total allocated size
} DynamicBuffer;

// Helper: Ensure that the dynamic buffer has at least (current used + extra) bytes.
// If not, expand it by OUTPUT_CHUNK_SIZE increments.
static int ensure_dynamic_buffer_capacity(DynamicBuffer *db, size_t extra) {
    if (db->used + extra > db->allocated) {
        size_t new_allocated = db->allocated;
        while (db->used + extra > new_allocated) {
            new_allocated += OUTPUT_CHUNK_SIZE;
        }
        uint8_t *new_data = (uint8_t *) ps_malloc(new_allocated);
        if (!new_data) {
            printf("ERROR: Unable to allocate %zu bytes for dynamic buffer.\n", new_allocated);
            return -1;
        }
        if (db->data) {
            memcpy(new_data, db->data, db->used);
            heap_caps_free(db->data);
        }
        db->data = new_data;
        db->allocated = new_allocated;
    }
    return 0;
}

// Writes a byte to a stream
static inline void szip_write_byte(SzipBufferStream *stream, uint8_t byte) {
    if (stream->pos < stream->size) {
        stream->data[stream->pos++] = byte;
    }
}

// Reads a byte from the stream
static inline int szip_read_byte(SzipBufferStream *stream) {
    return (stream->pos >= stream->size) ? EOF : stream->data[stream->pos++];
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

// Reads and validates the SZIP global header
static inline int szip_read_global_header(SzipBufferStream *stream) {
    if (szip_read_byte(stream) != 0x53 || szip_read_byte(stream) != 0x5A ||
        szip_read_byte(stream) != 0x0A || szip_read_byte(stream) != 0x04)
        return -1;
    int vmay = szip_read_byte(stream);
    int vmin = szip_read_byte(stream);
    return (vmay == EOF || vmin == EOF) ? -1 : 0;
}

// Reads a block header, skipping the orphaned byte, and returns the block type.
// Expected on-disk layout:
//   0x42, 0x48, 3-byte block size, 0x00, block type (0x01 for compressed)
static inline int szip_read_block_header(SzipBufferStream *stream, uint32_t *uncompressed_size, uint8_t *block_type) {
    if (szip_read_byte(stream) != 0x42 || szip_read_byte(stream) != 0x48)
        return -1;
    *uncompressed_size = szip_read_uint3(stream);
    if (szip_read_byte(stream) != 0x00) {
        printf("ERROR: Expected orphaned byte 0x00 but found something else.\n");
        return -1;
    }
    *block_type = szip_read_byte(stream);
    return 0;
}

// Reads a compressed block from the stream.
// Block layout (after block header):
//   3 bytes: index_last (big-endian; expected to be < block_size)
//   1 byte: order
//   ... followed by compressed data runs
// This function writes exactly 'buflen' bytes into the provided destination buffer.

static void szip_read_szip_block(SzipBufferStream *stream, uint8_t *dest, uint32_t buflen, SzipConfig *config) {
    uint32_t index_last = szip_read_uint3(stream);
    uint8_t order = szip_read_byte(stream);
    uint32_t bytes_left = buflen;

    printf("szip_read_szip_block: Initializing model (index_last=%u, order=%u, buflen=%u)\n",
           index_last, order, buflen);

    // ✅ Allocate model on heap
    SzipModel *m = (SzipModel *) ps_malloc(sizeof(SzipModel));
    if (!m) {
        printf("ERROR: Failed to allocate memory for SzipModel.\n");
        return;
    }

    printf("szip_read_szip_block: Allocated model at %p\n", m);

    // ✅ Initialize model
    initmodel(m, -1, &(config->recordsize));
    printf("szip_read_szip_block: Model initialized successfully.\n");

    uint32_t runlength;
    uint32_t ch;
    
    // ✅ Decode first run
    printf("szip_read_szip_block: Decoding first run...\n");
    sz_decode(m, &ch, &runlength);
    printf("szip_read_szip_block: First run decoded: ch=%u, runlength=%u\n", ch, runlength);

    if (runlength > bytes_left) {
        printf("ERROR: runlength (%u) exceeds block size (%u).\n", runlength, bytes_left);
        heap_caps_free(m);
        return;
    }
    bytes_left -= runlength;
    memset(dest, ch, runlength);
    fixafterfirst(m);
    dest += runlength;
    
    // ✅ Decode remaining runs
    while (bytes_left) {
        printf("szip_read_szip_block: Decoding next run (bytes_left=%u)...\n", bytes_left);
        sz_decode(m, &ch, &runlength);
        printf("szip_read_szip_block: Next run decoded: ch=%u, runlength=%u\n", ch, runlength);

        if (runlength > bytes_left) {
            printf("ERROR: runlength (%u) exceeds remaining bytes (%u).\n", runlength, bytes_left);
            heap_caps_free(m);
            return;
        }
        bytes_left -= runlength;
        memset(dest, ch, runlength);
        dest += runlength;
    }

    printf("szip_read_szip_block: All runs decoded. Cleaning up model...\n");
    deletemodel(m);
    heap_caps_free(m);  // ✅ Free model memory

    printf("szip_read_szip_block: Block decompression complete.\n");
}


// DECOMPRESSION FUNCTION WITH DYNAMIC OUTPUT BUFFER
//
// This function processes the entire input buffer (which begins with the global header)
// and then one or more blocks. For each block, it reads the block header (including the
// uncompressed size for that block), ensures that the dynamic output buffer is expanded
// as needed, calls szip_read_szip_block to decompress the block directly into the dynamic
// output buffer at the appropriate offset, and updates the buffer usage.
//
// Upon completion, *output will point to the decompressed data and *output_size will hold its size.
// (The caller is responsible for freeing the output buffer with heap_caps_free.)
uint8_t* szip_decompress_dynamic(uint8_t *input, uint32_t input_size, uint32_t *output_size, SzipConfig *config) {
    SzipBufferStream in_stream = { input, input_size, 0 };
    
    if (szip_read_global_header(&in_stream) < 0) {
        printf("ERROR: Invalid SZIP global header!\n");
        return NULL;
    }

    // Dynamically allocate DynamicBuffer struct to avoid stack usage.
    DynamicBuffer *db = (DynamicBuffer *) ps_malloc(sizeof(DynamicBuffer));
    if (!db) {
        printf("ERROR: Unable to allocate memory for DynamicBuffer struct.\n");
        return NULL;
    }
    db->allocated = OUTPUT_CHUNK_SIZE;
    db->used = 0;
    db->data = (uint8_t *) ps_malloc(db->allocated);
    if (!db->data) {
        printf("ERROR: Unable to allocate initial output buffer.\n");
        heap_caps_free(db);
        return NULL;
    }

    uint32_t uncompressed_size;
    uint8_t block_type;
    // Process each block in the input stream.
    while (in_stream.pos < in_stream.size) {
        if (szip_read_block_header(&in_stream, &uncompressed_size, &block_type) < 0) {
            printf("ERROR: Failed to read block header!\n");
            heap_caps_free(db->data);
            heap_caps_free(db);
            return NULL;
        }
        if (block_type != 0x01) {
            printf("ERROR: Unexpected block type %02X! Expected 0x01 for compressed block.\n", block_type);
            heap_caps_free(db->data);
            heap_caps_free(db);
            return NULL;
        }

        // Ensure dynamic buffer has space for this block.
        if (ensure_dynamic_buffer_capacity(db, uncompressed_size) < 0) {
            heap_caps_free(db->data);
            heap_caps_free(db);
            return NULL;
        }

        // Decompress into heap-allocated buffer (not stack).
        printf("Decompressing block with size %u\n", uncompressed_size);
        szip_read_szip_block(&in_stream, db->data + db->used, uncompressed_size, config);
        db->used += uncompressed_size;
    }
    *output_size = db->used;
    return db->data;
}

#ifdef __cplusplus
}
#endif

#endif // SZIP_VDP_H
