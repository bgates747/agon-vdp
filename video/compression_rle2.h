/*
* RLE compressor/decompressor for 8-bit RGBA2222 data
*
* Encoding rules:
*   - Single pixel (n=1): 
*       top bit = 1 (0x80)
*       bit 6 encodes alpha (1 => fully opaque [bits 7,6=11], 0 => fully transparent [bits 7,6=00])
*       bits 5..0 = color
*
*   - Two pixels (n=2): stored as two singletons (no run used).
*
*   - Run of 3..130 (n >=3): 
*       the run byte is (n - 3) with top bit 0 (so 0 => 3 pixels, 127 => 130 pixels),
*       then one more byte containing the pixel in RGBA2222 format, 
*         but only bits 7,6 are actually used for alpha (1 => 11, 0 => 00).
*
* Example:
*   If count=3, run byte=0, second byte=<pixel>
*   If count=130, run byte=127, second byte=<pixel>
*
* The system displays only fully transparent or fully opaque, so if either bit 7 or bit 6 
* in the original pixel is clear, we treat alpha as 0 => bits 7,6=00, else bits 7,6=11.
*
* Worst-case compressed size is (original file size + 14-byte header).
*/

#ifndef RLE2_H
#define RLE2_H

#include "buffers.h"

#define COMPRESSION_TYPE_RLE2 'r'
#define COMPRESSION_RLE2_HEADER_SIZE 14

void rle2_decompress(uint16_t sourceBufferId, BufferVector &sourceBuffer, uint8_t *buffer, uint32_t orig_size) {
    auto block = sourceBuffer.front(); // bufferConsolidate must have been called ...
    auto p_data = block->getBuffer(); // ... otherwise this won't work
    p_data += COMPRESSION_RLE2_HEADER_SIZE; // skip the header
    uint32_t out_index = 0; // initialize index into the output buffer
    while (out_index < orig_size) {
        uint8_t cmd = *p_data++; // get the next command byte and bump the pointer
        if (cmd & 0x80) { // Singleton: cmd specifies the color of one pixel
            uint8_t alpha = (cmd & 0x40) ? 0xC0 : 0x00; // restore transparency (copy bit 6 into bit 7)
            buffer[out_index++] = alpha | (cmd & 0x3F); // copy the color bits and bump the index
        } else {// Run: cmd is run length + 3, next byte is color
            size_t run = (cmd & 0x7F) + 3; // minimum run length is 3
            uint8_t color = *p_data++; // get the color byte and bump the pointer
            for (size_t j = 0; j < run; j++) {
                buffer[out_index++] = color; // repeat the color and bump the index
            }
        }
    }
}

#endif // RLE2_H
