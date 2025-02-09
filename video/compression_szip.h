#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "szip/port.h"
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

/* Struct for buffer-based processing */
typedef struct {
    unsigned char *data;
    uint4 size;
    uint4 pos;
} SzipBufferStream;

/* Read a byte from the buffer */
static int buf_getchar(SzipBufferStream *stream) {
    return (stream->pos < stream->size) ? stream->data[stream->pos++] : EOF;
}

/* Read a 3-byte integer from the buffer */
static uint4 buf_readuint3(SzipBufferStream *stream) {
    uint4 x = buf_getchar(stream);
    x = (x << 8) | buf_getchar(stream);
    x = (x << 8) | buf_getchar(stream);
    return x;
}

static void no_szip() {
    printf("probably not an szip file; could be szip version prior to 1.10\n");
    exit(1);
}

static void readglobalheader(SzipBufferStream *stream) {
    int ch, vmay;
    ch = buf_getchar(stream);
    if (ch == EOF) return;
    if (ch != 0x53) no_szip();
    if (buf_getchar(stream) != 0x5A) no_szip();
    if (buf_getchar(stream) != 0x0A) no_szip();
    if (buf_getchar(stream) != 0x04) no_szip();
    vmay = buf_getchar(stream);
    if (vmay == EOF || vmay == 0) no_szip();
    ch = buf_getchar(stream);
    if (ch == EOF) no_szip();
    if (vmay > vmayor || (vmay == vmayor && ch > vminor)) {
        printf("This file is szip version %d.%d, this program is %d.%d.\n Please update\n",
               vmay, ch, vmayor, vminor);
        exit(1);
    }
}

static uint readblockdir(SzipBufferStream *stream, uint4 *buflen) {
    int ch;
    ch = buf_getchar(stream);
    if (ch == EOF) {
        *buflen = 0;
        return 0;
    }
    if (ch != 0x42) no_szip();
    if (buf_getchar(stream) != 0x48) no_szip();
    *buflen = buf_readuint3(stream);
    if (buf_getchar(stream) != 0) no_szip();
    return 6;
}

static void readszipblock(SzipBufferStream *stream, uint dirsize, uint4 buflen, unsigned char *buffer) {
    unsigned char *tmp;
    uint4 indexlast, charcount[256], bytesleft;
#ifndef MODELGLOBAL
    sz_model m;
#endif
    if (verbosity & 1) printf("Decoding %d bytes ", buflen);
    indexlast = buf_readuint3(stream);
    order = buf_getchar(stream);

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

static void decompressit(SzipBufferStream *stream) {
    unsigned char *inoutbuffer = NULL;

    blocksize = 0;
    readglobalheader(stream);

    while (1) {
        uint4 blocklen;
        uint dirsize;
        int ch;
        dirsize = readblockdir(stream, &blocklen);
        if (dirsize == 0) break;
        if (blocklen > blocksize) {
            if (inoutbuffer != NULL)
                free(inoutbuffer);
            inoutbuffer = (unsigned char *)malloc(blocklen);
            blocksize = blocklen;
            if (inoutbuffer == NULL) {
                printf("memory allocation error\n");
                exit(1);
            }
        }
        ch = buf_getchar(stream);
        if (ch == 1)
            readszipblock(stream, dirsize + 1, blocklen, inoutbuffer);
        else
            no_szip();
        if (verbosity & 1) printf(" done\n");
    }
    free(inoutbuffer);
}
