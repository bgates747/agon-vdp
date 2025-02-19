/*
   Combined "simz.h" + block-based decompression logic
   that aligns with the PC command-line code. 
   (No file I/O, purely in-memory, for ESP32 or similar.)

   You previously had a "simz.h" with:
     - SIMZ_HEADER_SIZE (10)
     - simz_header struct
     - simz_read_header()
     - stubs for simz_decompressit() 
   This file merges all of that plus a corrected
   simz_decompressit() using the same block-based approach
   as your original PC version.
*/

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*********************** simz.h (original content) **************************/

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

/* Basic integer types (some might have been used in your original file) */
// typedef unsigned short uint2;  /* two-byte integer (for large arrays) */
// typedef unsigned int   uint4;  /* four-byte integer (range needed) */
// typedef unsigned int   uint;   /* fast unsigned integer, 2 or 4 bytes */

/******************** SIMZ Range Coder *************************/
/* We'll re-implement the actual decode structure in this file. */

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

/* If the header is invalid, we call no_simz(), which in your original
   code might just exit(1). You can adapt that as needed. */
void no_simz(void) {
    fprintf(stderr, "no_simz: Not a SIMZ file\n");
    exit(1);
}

/*
 * simz_read_header:
 *  Reads and validates the header from the given input buffer.
 *  If the header is invalid, calls no_simz().
 *  Otherwise, fills in *hdr with the header info.
 */
inline void simz_read_header(const uint8_t *input, size_t input_size, simz_header *hdr) {
    if (input_size < SIMZ_HEADER_SIZE) {
        no_simz();  // Not enough data
    }
    if (input[0] != 'S' || input[1] != 'I' || input[2] != 'M' || input[3] != 'Z') {
        no_simz();  // bad magic
    }
    hdr->major = input[4];
    hdr->minor = input[5];
    hdr->decompressed_size = ((uint32_t)input[6]) |
                             ((uint32_t)input[7] << 8) |
                             ((uint32_t)input[8] << 16) |
                             ((uint32_t)input[9] << 24);
}

/****************** SIMZ Decompression API *******************/
/* The function signature you originally used: */
void simz_decompressit(uint8_t **output,
                       uint32_t *output_size,
                       const uint8_t *compressedData,
                       uint32_t compressedSize,
                       uint32_t expectedOutputSize);

#ifdef __cplusplus
}
#endif

#endif // SIMZ_H

/**************** End of "simz.h" portion ********************/


/************************************************************
 * Now implement the *correct* block-based simz_decompressit,
 * mirroring your PC code but using memory buffers instead of files.
 ************************************************************/

/* 
   We define a small range coder struct for decoding from memory.
   (This is *not* the same as your old uniform "simz_decode_byte" approach.)
*/
typedef struct {
    uint32_t low;       // low end of interval
    uint32_t range;     // length of interval
    uint32_t help;      // intermediate
    uint8_t  buffer;    // current input byte

    // In-memory input buffer:
    const uint8_t* input;
    size_t input_size;
    size_t input_pos;
} simz_rangecoder;

/* Constants from your rangecod.c / simz code */
#define SIMZ_CODE_BITS   32
#define SIMZ_TOP_VALUE   ((uint32_t)1 << (SIMZ_CODE_BITS-1))
#define SIMZ_SHIFT_BITS  (SIMZ_CODE_BITS - 9)
#define SIMZ_EXTRA_BITS  (((SIMZ_CODE_BITS-2) % 8) + 1)
#define SIMZ_BOTTOM_VALUE (SIMZ_TOP_VALUE >> 8)

/* Small helper to read one byte from the buffer or 0 if we are out */
static inline uint8_t simz_mem_read(simz_rangecoder *rc) {
    if (rc->input_pos < rc->input_size) {
        return rc->input[rc->input_pos++];
    }
    return 0;
}

/* Normalize state to keep range above SIMZ_BOTTOM_VALUE. */
static inline void simz_dec_normalize(simz_rangecoder *rc) {
    while (rc->range <= SIMZ_BOTTOM_VALUE) {
        rc->low = (rc->low << 8) | ((rc->buffer << SIMZ_EXTRA_BITS) & 0xff);
        rc->buffer = simz_mem_read(rc);
        rc->low |= rc->buffer >> (8 - SIMZ_EXTRA_BITS);
        rc->range <<= 8;
    }
}

/* Initialize the decoder from memory (i.e. "start decoding"). */
static int simz_start_decoding(simz_rangecoder *rc,
                               const uint8_t *data,
                               size_t data_size)
{
    rc->input = data;
    rc->input_size = data_size;
    rc->input_pos = 0;
    rc->low = 0;
    rc->range = 0;
    rc->help = 0;
    rc->buffer = 0;

    if (data_size == 0) {
        return -1;  // no data
    }

    rc->buffer = simz_mem_read(rc);  // first byte

    if (rc->input_pos < rc->input_size) {
        // We can read part of the next byte to init 'low'
        rc->low = (rc->input[rc->input_pos] >> (8 - SIMZ_EXTRA_BITS));
        rc->range = (uint32_t)1 << SIMZ_EXTRA_BITS;
        rc->input_pos++;
    } else {
        // if only 1 byte in total, we do our best
        rc->low = 0;
        rc->range = 0;
    }

    return 0; // success
}

/* Finish decoding: consume any leftover bits. */
static void simz_done_decoding(simz_rangecoder *rc) {
    simz_dec_normalize(rc);
}

/* Get cumulative frequency for the next symbol. 
   total_freq = total frequency for the block (or 2 for the 1-bit test). */
static uint32_t simz_decode_culfreq(simz_rangecoder *rc, uint32_t total_freq) {
    simz_dec_normalize(rc);
    rc->help = rc->range / total_freq;
    uint32_t cf = rc->low / rc->help;
    if (cf >= total_freq) {
        cf = total_freq - 1; // clamp
    }
    return cf;
}

/* Update state after determining which symbol was selected. */
static void simz_decode_update(simz_rangecoder *rc,
                               uint32_t sy_f,
                               uint32_t lt_f,
                               uint32_t tot_f)
{
    uint32_t tmp = rc->help * lt_f;
    rc->low  -= tmp;
    if (lt_f + sy_f < tot_f) {
        rc->range = rc->help * sy_f;
    } else {
        rc->range -= tmp;
    }
}

/* Decode a 16-bit "short" with uniform distribution (used for reading frequencies). */
static uint16_t simz_decode_short(simz_rangecoder *rc) {
    // same logic as decode_culshift(rc,16) with sy_f=1
    simz_dec_normalize(rc);
    rc->help = rc->range >> 16;      // range / (1<<16)
    uint32_t cf = rc->low / rc->help;
    if (cf > 0xFFFF) {
        cf = 0xFFFF; // clamp
    }
    rc->low -= rc->help * cf;
    rc->range = rc->help;           // sy_f=1 => range=rc->help
    return (uint16_t)cf;
}

/**********************************************************************
 * The key function your code calls:
 *   simz_decompressit(&decompressedData, &decompressedSize, 
 *                     compressedData + SIMZ_HEADER_SIZE, 
 *                     compressedSize - SIMZ_HEADER_SIZE, 
 *                     expectedOutputSize);
 *
 * 1) We do NOT read the 10-byte header here, because you do it externally.
 * 2) We expect the rest of the data to contain the block-based structure:
 *      - repeated: 1 bit "block present?" 
 *      - if present: read 256 freq counts, decode that block
 *      - stop if bit=0
 **********************************************************************/
void simz_decompressit(uint8_t **output,
                       uint32_t *output_size,
                       const uint8_t *compressedData,
                       uint32_t compressedSize,
                       uint32_t expectedOutputSize)
{
    // Default to failure in case we return early
    *output = NULL;
    *output_size = 0;

    // Start the range decoder from memory
    simz_rangecoder rc;
    if (simz_start_decoding(&rc, compressedData, compressedSize) != 0) {
        // can't start
        return;
    }

    // Allocate the output buffer based on expected size
    uint8_t *outBuf = (uint8_t*)malloc(expectedOutputSize);
    if (!outBuf) {
        return; // allocation fail
    }

    uint32_t outPos = 0;

    while (1) {
        // read 1-bit "flag" => decode_culfreq(rc, 2)
        uint32_t cf = simz_decode_culfreq(&rc, 2);
        if (cf == 0) {
            // no more blocks
            simz_decode_update(&rc, 1, 0, 2);
            break;
        }
        // else consume that "bit=1" indicating a block present
        simz_decode_update(&rc, 1, 1, 2);

        // read 256 frequency counts (each is 16-bit)
        uint32_t counts[257];
        memset(counts, 0, sizeof(counts));
        uint32_t blockSize = 0;
        for (int i = 0; i < 256; i++) {
            uint16_t freq = simz_decode_short(&rc);
            counts[i] = freq;
        }
        // convert them into a cumulative distribution
        for (int i = 0; i < 256; i++) {
            uint32_t freq = counts[i];
            counts[i] = blockSize;
            blockSize += freq;
        }
        counts[256] = blockSize;

        // decode 'blockSize' symbols
        for (uint32_t i = 0; i < blockSize; i++) {
            if (outPos >= expectedOutputSize) {
                // safety check, block claims more data than header says
                break;
            }
            uint32_t cf_sym = simz_decode_culfreq(&rc, blockSize);
            // find which symbol has counts[sym] <= cf_sym < counts[sym+1]
            int symbol = 0;
            while (counts[symbol+1] <= cf_sym) {
                symbol++;
            }
            // update the coder
            uint32_t freq_of_sym  = counts[symbol+1] - counts[symbol];
            uint32_t start_of_sym = counts[symbol];
            simz_decode_update(&rc, freq_of_sym, start_of_sym, blockSize);

            // store the symbol
            outBuf[outPos++] = (uint8_t)symbol;
        }
    }

    // done
    simz_done_decoding(&rc);

    // success
    *output = outBuf;
    *output_size = expectedOutputSize;
}
