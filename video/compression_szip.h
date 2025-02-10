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
uint order = 6, verbosity = 0, compress = 1;
unsigned char recordsize = 1;

static void no_szip() {
    printf("probably not an szip file; could be szip version prior to 1.10\n");
    exit(1);
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
        printf("This file is szip version %d.%d, this program is %d.%d.\n Please update\n",
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
    if (verbosity & 1) printf("Decoding %d bytes ", buflen);
    indexlast = sz_stream_readuint3();
    order = sz_stream_getchar();

    memset(charcount, 0, sizeof(charcount));
    initmodel(&m, -1, &recordsize);

    if (verbosity & 1) {
        if (order != 6) printf("-o%d ", order);
        if ((recordsize & 0x7F) != 1) printf("-r%d ", recordsize & 0x7F);
        if (recordsize & 0x80) printf("-i ");
        printf("...");
    }

    tmp = buffer;
    bytesleft = buflen;
    {   uint4 runlength;
        uint ch;
        sz_decode(&m, &ch, &runlength);
        if (runlength > bytesleft) {
            printf("input file corrupt\n");
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
            printf("input file corrupt\n");
            exit(1);
        }
        bytesleft -= runlength;
        charcount[ch] += runlength;
        while (runlength--) {
            *(tmp++) = ch;
        }
    }
    deletemodel(&m);

    if (verbosity & 1) printf(" processing ...");

    if (recordsize == 1) {
        if (order == 0)
            sz_unsrt_BW(buffer, NULL, buflen, indexlast, charcount);
        else
            sz_unsrt(buffer, NULL, buflen, indexlast, charcount, order);
    } else {
        tmp = (unsigned char *)malloc(buflen);
        if (tmp == NULL) {
            printf("memory allocation failure\n");
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
                printf("memory allocation error\n");
                exit(1);
            }
        }

        ch = sz_stream_getchar();
        if (ch == 1)
            readszipblock(dirsize + 1, blocklen, *inoutbuffer_ptr);
        else
            no_szip();

        *outSize = blocklen;  // Update the output size

        if (verbosity & 1) printf(" done\n");
    }
}
