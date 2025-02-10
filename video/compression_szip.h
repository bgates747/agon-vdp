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
uint4 blocksize = 32768; // 32 KB = 0x8000, ESP32-friendly default
uint order = 6;
#define VERBOSITY 1
uint compress = 1;
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
    return 6;
}

static void readszipblock(uint dirsize, uint4 buflen, unsigned char *buffer) {
    unsigned char *tmp;
    uint4 indexlast, charcount[256], bytesleft;
#ifndef MODELGLOBAL
    sz_model m;
#endif
    debug_log("Decoding %d bytes ", buflen);
    indexlast = sz_stream_readuint3();
    order = sz_stream_getchar();

    memset(charcount, 0, sizeof(charcount));
    initmodel(&m, -1, &recordsize);

    #if VERBOSITY == 1
        if (order != 6) debug_log("-o%d ", order);
        if ((recordsize & 0x7F) != 1) debug_log("-r%d ", recordsize & 0x7F);
        if (recordsize & 0x80) debug_log("-i ");
        debug_log("...");
    #endif

    tmp = buffer;
    bytesleft = buflen;
    {   uint4 runlength;
        uint ch;
        sz_decode(&m, &ch, &runlength);
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
    fixafterfirst(&m);
    while (bytesleft) {
        uint4 runlength;
        uint ch;
        sz_decode(&m, &ch, &runlength);
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
    deletemodel(&m);

    debug_log(" processing ...");

    if (recordsize == 1) {
        if (order == 0)
            sz_unsrt_BW(buffer, NULL, buflen, indexlast, charcount);
        else
            sz_unsrt(buffer, NULL, buflen, indexlast, charcount, order);
    } else {
        tmp = (unsigned char *)malloc(buflen);
        if (tmp == NULL) {
            debug_log("memory allocation failure\n");
            exit(1);
        }
        if (order == 0)
            sz_unsrt_BW(buffer, tmp, buflen, indexlast, charcount);
        else
            sz_unsrt(buffer, tmp, buflen, indexlast, charcount, order);
        if (recordsize & 0x80) {
            uint4 i;
            unsigned char c = *tmp;
            for (i = 1; i < buflen; i++) {
                c = (c + tmp[i]) & 0xFF;
                tmp[i] = c;
            }
        }
        unreorder(tmp, buffer, buflen, recordsize & 0x7F);
        free(tmp);
    }
}

static void decompressit(unsigned char **inoutbuffer_ptr, uint32_t *outSize) {
    blocksize = 0;
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
            debug_log("[DEBUG] Reading compressed block, size=%d bytes\n", blocklen);
            readszipblock(dirsize + 1, blocklen, *inoutbuffer_ptr);
        } else {
            debug_log("[ERROR] Expected block marker 0x01, got 0x%02X\n", ch);
            no_szip();
        }
        
        *outSize = blocklen;  // Update the output size
        
        #if VERBOSITY == 1
        debug_log("[DEBUG] Decompressed Data (Hexdump):\n");
        for (uint32_t i = 0; i < blocklen; i++) {
            debug_log("%02X ", (*inoutbuffer_ptr)[i]);
            if ((i + 1) % 16 == 0) debug_log("\n"); // Format output in 16-byte rows
        }
        debug_log("\n");
        debug_log(" done\n");
        #endif
    }
}
