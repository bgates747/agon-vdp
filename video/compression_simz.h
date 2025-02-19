/*********************** simz.h **************************/
#ifndef SIMZ_H
#define SIMZ_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GCC
#define Inline inline
#else
#define Inline __inline
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

/* Basic integer types */
typedef unsigned short uint2;  /* two-byte integer (for large arrays) */
typedef unsigned int   uint4;  /* four-byte integer (range needed) */
typedef unsigned int   uint;   /* fast unsigned integer, 2 or 4 bytes */

/******************** SIMZ Range Coder *************************/
/* Define SIMZ range coder types */
typedef uint4 simz_code_value; /* 32-bit range coder value */
typedef uint4 simz_freq;       /* Frequency count type */

/* The range coder structure for decoding (in-memory) */
typedef struct {
    simz_code_value low;       /* low end of interval */
    simz_code_value range;     /* length of interval */
    simz_code_value help;      /* intermediate value */
    uint8_t buffer;            /* current input byte */
    uint4 bytecount;           /* count of processed bytes */
    /* In-memory input buffer info: */
    const uint8_t *input;      /* pointer to entire input buffer */
    size_t input_size;         /* total size of input buffer */
    size_t input_pos;          /* current read position in input buffer */
} simz_rangecoder;

/******************** SIMZ Header *****************************/
/*
   SIMZ header format (10 bytes total):
     - Bytes 0-3:  Magic "SIMZ"
     - Byte 4:     Major version number
     - Byte 5:     Minor version number
     - Bytes 6-9:  Total decompressed data size (uint32_t, little-endian)
*/
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint32_t decompressed_size;
} simz_header;

#define SIMZ_HEADER_SIZE 10

/* Error handling: if the header is invalid, call no_simz() */
void no_simz(void) {
    fprintf(stderr, "no_simz: Not a SIMZ file\n");
    exit(1);
}

/*
 * simz_read_header:
 *  Reads and validates the header from the given input buffer.
 *  Expects at least 10 bytes. If the header is invalid, calls no_simz().
 *  Otherwise, fills in *hdr with the header information.
 */
inline void simz_read_header(const uint8_t *input, size_t input_size, simz_header *hdr) {
    if (input_size < SIMZ_HEADER_SIZE) {
        no_simz();  // Not enough data for a valid SIMZ header.
    }
    if (input[0] != 'S' || input[1] != 'I' || input[2] != 'M' || input[3] != 'Z') {
        no_simz();  // Header magic invalid.
    }
    hdr->major = input[4];
    hdr->minor = input[5];
    hdr->decompressed_size = ((uint32_t)input[6]) |
                             ((uint32_t)input[7] << 8) |
                             ((uint32_t)input[8] << 16) |
                             ((uint32_t)input[9] << 24);
}

/****************** Range Coder Decoding Functions ******************/

/* Constants for range coding */
#define SIMZ_CODE_BITS 32
#define SIMZ_TOP_VALUE ((simz_code_value)1 << (SIMZ_CODE_BITS - 1))
#define SIMZ_SHIFT_BITS (SIMZ_CODE_BITS - 9)
#define SIMZ_EXTRA_BITS (((SIMZ_CODE_BITS - 2) % 8) + 1)
#define SIMZ_BOTTOM_VALUE (SIMZ_TOP_VALUE >> 8)

/* Read a byte from the input buffer */
static inline uint8_t simz_read_byte(simz_rangecoder *rc) {
    return (rc->input_pos < rc->input_size) ? rc->input[rc->input_pos++] : 0;
}

/* Normalize the decoder state */
static inline void simz_dec_normalize(simz_rangecoder *rc) {
    while (rc->range <= SIMZ_BOTTOM_VALUE) {
        rc->low = (rc->low << 8) | (((rc->buffer) << SIMZ_EXTRA_BITS) & 0xff);
        rc->buffer = simz_read_byte(rc);
        rc->low |= rc->buffer >> (8 - SIMZ_EXTRA_BITS);
        rc->range <<= 8;
    }
}

/* Get cumulative frequency for next symbol (without updating state) */
simz_freq simz_decode_culfreq(simz_rangecoder *rc, simz_freq tot_f) {
    simz_dec_normalize(rc);
    rc->help = rc->range / tot_f;
    simz_freq tmp = rc->low / rc->help;
    return (tmp >= tot_f) ? tot_f - 1 : tmp;
}

/* Shift-based cumulative frequency */
simz_freq simz_decode_culshift(simz_rangecoder *rc, simz_freq shift) {
    simz_dec_normalize(rc);
    rc->help = rc->range >> shift;
    simz_freq tmp = rc->low / rc->help;
    return (tmp >> shift) ? ((simz_code_value)1 << shift) - 1 : tmp;
}

/* Update the decoder state */
void simz_decode_update(simz_rangecoder *rc, simz_freq sy_f, simz_freq lt_f, simz_freq tot_f) {
    simz_code_value tmp = rc->help * lt_f;
    rc->low -= tmp;
    if (lt_f + sy_f < tot_f)
        rc->range = rc->help * sy_f;
    else
        rc->range -= tmp;
}

/* Decode a single byte */
uint8_t simz_decode_byte(simz_rangecoder *rc) {
    uint8_t tmp = simz_decode_culshift(rc, 8);
    simz_decode_update(rc, 1, tmp, (simz_freq)1 << 8);
    return tmp;
}

/* Finalize decoding: normalize to use up all bytes */
void simz_done_decoding(simz_rangecoder *rc) {
    simz_dec_normalize(rc);
}

/* Initialize the decoder with a given input buffer */
int simz_start_decoding(simz_rangecoder *rc, const uint8_t *input, size_t input_size) {
    if (input_size == 0) return -1;  // Prevent empty input
    rc->input = input;
    rc->input_size = input_size;
    rc->input_pos = 1;   // Start reading from the second byte
    rc->buffer = input[0];
    if (input_size > 1) {
        rc->low = input[1] >> (8 - SIMZ_EXTRA_BITS);
        rc->range = (simz_code_value)1 << SIMZ_EXTRA_BITS;
    } else {
        rc->low = 0;
        rc->range = 0;
    }
    return 0;
}

/****************** SIMZ Decompression API *******************/

/*
 * simz_decompressit:
 *  Decompresses SIMZ-compressed data from a buffer into a newly allocated output buffer.
 *
 * Parameters:
 *    output      - pointer to a uint8_t* that will be set to the newly allocated decompressed data.
 *    output_size - pointer to a uint32_t that will be set to the decompressed data size.
 *    compressedData - pointer to the compressed data buffer (should point just after the header if header is handled separately).
 *    compressedSize - size in bytes of the compressed data.
 *    expectedOutputSize - the expected decompressed data size (from header or metadata).
 *
 * This function initializes the SIMZ range coder, decodes expectedOutputSize bytes,
 * and returns the decompressed data via the output pointer. The caller is responsible for freeing the output buffer.
 */
void simz_decompressit(uint8_t **output, uint32_t *output_size, const uint8_t *compressedData, uint32_t compressedSize, uint32_t expectedOutputSize) {
    simz_rangecoder rc;
    if (simz_start_decoding(&rc, compressedData, compressedSize) != 0) {
        *output = NULL;
        *output_size = 0;
        return;
    }
    uint8_t *out_buf = (uint8_t *)malloc(expectedOutputSize);
    if (!out_buf) {
        *output = NULL;
        *output_size = 0;
        return;
    }
    for (uint32_t i = 0; i < expectedOutputSize; i++) {
        out_buf[i] = simz_decode_byte(&rc);
    }
    simz_done_decoding(&rc);
    *output = out_buf;
    *output_size = expectedOutputSize;
}

#ifdef __cplusplus
}
#endif

#endif // SIMZ_H
