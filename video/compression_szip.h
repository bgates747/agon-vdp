#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "szip/port.h"
#include "szip/sz_stream.h"
#include "szip/sz_mod4.h"
#include "szip/sz_srt.h"
#include "szip/reorder.h"

static char vmayor = 1, vminor = 12;

#define BLOCK_SIZE (1 << SIZE_SHIFT) // TODO: not used anywhere so consider deleting
#define SZIP_HEADER_SIZE 6


/* parameter values */
uint order = 6;
#define VERBOSITY 0
unsigned char recordsize = 1;

extern void debug_log(const char * format, ...);		// Debug log function

static void no_szip() {
    debug_log("probably not an szip file; could be szip version prior to 1.10\n");
    exit(1);
}

static void hex_dump(const unsigned char *buf, uint4 len) {
    uint4 i;
    for (i = 0; i < len; i++) {
        debug_log("%02X ", buf[i]);
        if ((i + 1) % 16 == 0)
        debug_log("\n");
    }
    if (len % 16 != 0)
    debug_log("\n");
}

static void readglobalheader() {
    int ch, vmay;
    ch = sz_stream_getchar();
    if (ch == EOF) return;
    if (ch != 0x53) no_szip();
    if (sz_stream_getchar() != 0x5A) no_szip();
    if (sz_stream_getchar() != 0x0A) no_szip();
    if (sz_stream_getchar() != 0x04) no_szip();
    vmay = sz_stream_getchar();
    if (vmay == EOF || vmay == 0) no_szip();
    ch = sz_stream_getchar();
    if (ch == EOF) no_szip();
    if (vmay > vmayor || (vmay == vmayor && ch > vminor)) {
        debug_log("This file is szip version %d.%d, this program is %d.%d.\n Please update\n",
               vmay, ch, vmayor, vminor);
        exit(1);
    }
    debug_log("readglobalheader: szip version %d.%d\n", vmay, ch);
}

static uint readblockdir(uint4 *buflen) {
    int ch;
    ch = sz_stream_getchar();
    if (ch == EOF) {
        *buflen = 0;
        return 0;
    }
    if (ch != 0x42) no_szip();
    if (sz_stream_getchar() != 0x48) no_szip();
    *buflen = sz_stream_readuint3();
    if (sz_stream_getchar() != 0) no_szip();
    debug_log("readblockdir: block size %d\n", *buflen);
    return 6;
}

static void readszipblock(uint dirsize, uint4 buflen, unsigned char *buffer) {
    unsigned char *out_buffer;  // Explicit output buffer
    uint4 indexlast, charcount[256], bytesleft;

#ifndef MODELGLOBAL
    // Instead of putting 'sz_model m;' on the stack, we now allocate it dynamically.
    sz_model *m = NULL;
#endif

    debug_log("readszipblock: Decoding %d bytes\n", buflen);

    // Read the block header info from your compressed stream:
    indexlast = sz_stream_readuint3();
    order = sz_stream_getchar();
    debug_log("readszipblock: indexlast=%d order=%d\n", indexlast, order);

    // Initialize charcount to zero
    memset(charcount, 0, sizeof(charcount));

#ifndef MODELGLOBAL
    // Dynamically allocate the sz_model
    m = (sz_model *)malloc(sizeof(sz_model));
    if (!m) {
        debug_log("readszipblock: memory allocation for sz_model failed\n");
        exit(1);
    }
    // Initialize the model for DEcompression
    initmodel(m, -1, &recordsize);
#else
    // If MODELGLOBAL is defined, you presumably have a global 'mod'.
    initmodel(&mod, -1, &recordsize);
#endif

    debug_log("readszipblock: model initialized\n");

    // === Begin decoding runs into `buffer` ===
    unsigned char *tmp = buffer;
    bytesleft = buflen;

    // Decode the *first* run
    {
        uint4 runlength;
        uint ch;

#ifndef MODELGLOBAL
        sz_decode(m, &ch, &runlength);
#else
        sz_decode(&mod, &ch, &runlength);
#endif

        if (runlength > bytesleft) {
            debug_log("input file corrupt\n");
            exit(1);
        }
        bytesleft -= runlength;
        charcount[ch] += runlength;
        while (runlength--) {
            *(tmp++) = ch;
        }
    }

#ifndef MODELGLOBAL
    fixafterfirst(m);
#else
    fixafterfirst(&mod);
#endif

    debug_log("readszipblock: first run decoded, bytesleft=%d\n", bytesleft);

    // Decode the rest of the runs
    while (bytesleft) {
        uint4 runlength;
        uint ch;

#ifndef MODELGLOBAL
        sz_decode(m, &ch, &runlength);
#else
        sz_decode(&mod, &ch, &runlength);
#endif

        if (runlength > bytesleft) {
            debug_log("input file corrupt\n");
            exit(1);
        }
        bytesleft -= runlength;
        charcount[ch] += runlength;
        while (runlength--) {
            *(tmp++) = ch;
        }
    }
    debug_log("readszipblock: all runs decoded, bytesleft=%d\n", bytesleft);

    // Done with the model
#ifndef MODELGLOBAL
    deletemodel(m);
#else
    deletemodel(&mod);
#endif
    debug_log("readszipblock: model deleted\n");

    // Allocate a separate output buffer for "unsorting"
    out_buffer = (unsigned char *)malloc(buflen);
    if (out_buffer == NULL) {
        debug_log("memory allocation failure\n");
        exit(1);
    }

    // Perform unsorting into `out_buffer`
    if (recordsize == 1) {
        if (order == 0)
            sz_unsrt_BW(buffer, out_buffer, buflen, indexlast, charcount);
        else
            sz_unsrt(buffer, out_buffer, buflen, indexlast, charcount, order);
    } else {
        if (order == 0)
            sz_unsrt_BW(buffer, out_buffer, buflen, indexlast, charcount);
        else
            sz_unsrt(buffer, out_buffer, buflen, indexlast, charcount, order);

        // Perform the optional delta restoration if (recordsize & 0x80)
        if (recordsize & 0x80) {
            uint4 i;
            unsigned char c = *out_buffer;
            for (i = 1; i < buflen; i++) {
                c = (c + out_buffer[i]) & 0xFF;
                out_buffer[i] = c;
            }
        }
        // Perform "unreorder" step
        unreorder(out_buffer, buffer, buflen, recordsize & 0x7F);
        debug_log("readszipblock: unsorted\n");
    }

    // Copy final output back into `buffer`
    memcpy(buffer, out_buffer, buflen);
    free(out_buffer);

#ifndef MODELGLOBAL
    // Finally, free the dynamically allocated sz_model
    free(m);
#endif

    debug_log("readszipblock: done\n");
}

static void decompressit(unsigned char **inoutbuffer_ptr, uint32_t *outSize) {
    uint4 blocksize = 0;
    readglobalheader();  // Uses global stream

    *outSize = 0;  // Reset output size

    while (1) {
        uint4 blocklen;
        uint dirsize;
        int ch;

        dirsize = readblockdir(&blocklen);
        if (dirsize == 0) break;

        // Allocate or reallocate the output buffer
        if (blocklen > blocksize) {
            if (*inoutbuffer_ptr != NULL) {
                free(*inoutbuffer_ptr);
            }
            *inoutbuffer_ptr = (unsigned char *)malloc(blocklen);
            blocksize = blocklen;
            if (*inoutbuffer_ptr == NULL) {
                debug_log("memory allocation error\n");
                exit(1);
            }
        }

        ch = sz_stream_getchar();
        if (ch == 1) {
            debug_log("decompressit: Reading compressed block, size=%d bytes\n", blocklen);
            readszipblock(dirsize + 1, blocklen, *inoutbuffer_ptr);
        } else {
            debug_log("decompressit: [ERROR] Expected block marker 0x01, got 0x%02X\n", ch);
            no_szip();
        }
        
        *outSize = blocklen;  // Update the output size
        
        // #if VERBOSITY == 1
        // debug_log("decompressit:  Decompressed Data (Hexdump):\n");
        // for (uint32_t i = 0; i < blocklen; i++) {
        //     debug_log("%02X ", (*inoutbuffer_ptr)[i]);
        //     if ((i + 1) % 16 == 0) debug_log("\n"); // Format output in 16-byte rows
        // }
        // debug_log("\n");
        // debug_log(" done\n");
        // #endif
    }
}
