#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_heap_caps.h> // ESP32 memory allocation
#include "port.h"
#include "sz_err.h"
#include "sz_srt.h"

#if defined(SZ_UNSRT_O4)
#include "sz_hash2.h" // Only needed for `sz_unsrt_o4`
#endif

#define BITSSAMEBLOCK 10
#define BLOCKSIZE (1 << BITSSAMEBLOCK)
#define BLOCKMASK (BLOCKSIZE - 1)
#define INDIRECT 0x800000  // Mark indirect entries in the table

typedef struct p_block {
    uint2 msbytes[BLOCKSIZE];
    unsigned char lsbyte[BLOCKSIZE];
    struct p_block *nextfree;
} ptrblock;

typedef struct {
    ptrblock **index;
    ptrblock **oldindex;
    ptrblock *freelist;
    ptrblock *block;
    ptrblock *spare[18];
    uint4 nrblocks;
} ptrstruct;

static ptrstruct globalptr;
static int globalinit = 0;

static void allocptrs(uint4 length, ptrstruct *p) {
    uint4 i;
    p->nrblocks = (length + BLOCKSIZE - 1) / BLOCKSIZE;

    if (globalinit && (p->nrblocks > globalptr.nrblocks)) {
        free(globalptr.index);
        free(globalptr.oldindex);
        free(globalptr.block);
        globalinit = 0;
    }

    if (!globalinit) {
        globalptr.nrblocks = p->nrblocks;
        globalptr.index = (ptrblock **)heap_caps_malloc(sizeof(ptrblock *) * globalptr.nrblocks, MALLOC_CAP_8BIT);
        globalptr.oldindex = (ptrblock **)heap_caps_malloc(sizeof(ptrblock *) * globalptr.nrblocks, MALLOC_CAP_8BIT);
        globalptr.block = (ptrblock *)heap_caps_malloc(sizeof(ptrblock) * globalptr.nrblocks, MALLOC_CAP_8BIT);

        if (!globalptr.index || !globalptr.oldindex || !globalptr.block)
            sz_error(SZ_NOMEM_SORT);

        globalinit = 1;
    }

    p->index = globalptr.index;
    p->oldindex = globalptr.oldindex;
    p->block = globalptr.block;
    p->freelist = NULL;

    for (i = 0; i < 18; i++)
        p->spare[i] = NULL;

    for (i = 0; i < p->nrblocks; i++)
        p->index[i] = p->block + i;
}

static void extraspare(ptrstruct *p, int blocks) {
    int i;
    for (i = 0; p->spare[i] != NULL; i++);
    
    p->spare[i] = (ptrblock *)heap_caps_malloc(sizeof(ptrblock) * blocks, MALLOC_CAP_8BIT);
    if (!p->spare[i]) sz_error(SZ_NOMEM_SORT);

    p->spare[i]->nextfree = p->freelist;
    p->freelist = p->spare[i];

    for (i = 1; i < blocks; i++)
        p->freelist[i - 1].nextfree = p->freelist + i;
    p->freelist[blocks - 1].nextfree = NULL;
}

static void freeptrs(ptrstruct *p) {
    for (int i = 0; p->spare[i] != NULL; i++)
        free(p->spare[i]);
}

static inline void setptr(ptrstruct *p, uint4 i, uint4 ptr) {
    ptrblock *tmp = p->index[i >> BITSSAMEBLOCK];
    if (!tmp) {
        if (!p->freelist)
            extraspare(p, 16);
        tmp = p->index[i >> BITSSAMEBLOCK] = p->freelist;
        p->freelist = p->freelist->nextfree;
    }
    i &= BLOCKMASK;
    tmp->msbytes[i] = ptr >> 8;
    tmp->lsbyte[i] = ptr & 0xff;
}

static void sortorder2(ptrstruct *p, unsigned char *in, uint4 length, uint4 *counts, unsigned int offset, uint4 *indexlast) {
    uint4 i, *o2counts, sum;
    unsigned int context;
    memset(counts, 0, 256 * sizeof(uint4));
    o2counts = (uint4*) calloc(0x10000, sizeof(uint4));
    if (o2counts == NULL)
        sz_error(SZ_NOMEM_SORT);
    context = (unsigned)in[length - 1] << 8;
    for (i = 0; i < length; i++) {
        context = context >> 8 | (unsigned)(in[i]) << 8;
        counts[in[i]]++;
        o2counts[context]++;
    }
    sum = length;
    for (i = 0x10000; i--;) {
        sum -= o2counts[i];
        o2counts[i] = sum;
    }
    free(o2counts);
}

static void incsortorder(ptrstruct *p, unsigned char *in, uint4 length,
                         uint4 *counts, int offset, uint4 *indexlast) {
    uint4 i, block, ct[256];
    ptrblock *curblock;
    unsigned char ch = 0;

    // Swap index pointers
    { ptrblock **swap = p->index; p->index = p->oldindex; p->oldindex = swap; }
    
    memset(p->index, 0, p->nrblocks * sizeof(ptrblock *));
    memcpy(ct, counts, 256 * sizeof(uint4));
    
    block = 0;
    curblock = p->oldindex[block];

    // Process until indexlast
    for (i = 0; i <= *indexlast; i++) {
        unsigned index = i & BLOCKMASK;
        uint4 tmp = ((uint4)curblock->msbytes[index] << 8) | curblock->lsbyte[index];
        ch = in[tmp - offset];
        setptr(p, ct[ch], tmp);
        ct[ch]++;

        // Move to next block if needed
        if (index == BLOCKMASK && block != p->nrblocks - 1) {
            curblock->nextfree = p->freelist;
            p->freelist = curblock;
            block++;
            curblock = p->oldindex[block];
        }
    }

    *indexlast = ct[ch] - 1;

    // Process remaining elements
    for (; i < length; i++) {
        unsigned index = i & BLOCKMASK;
        uint4 tmp = ((uint4)curblock->msbytes[index] << 8) | curblock->lsbyte[index];
        ch = in[tmp - offset];
        setptr(p, ct[ch], tmp);
        ct[ch]++;

        // Move to next block if needed
        if (index == BLOCKMASK && block < p->nrblocks - 1) {
            curblock->nextfree = p->freelist;
            p->freelist = curblock;
            block++;
            curblock = p->oldindex[block];
        }
    }

    curblock->nextfree = p->freelist;
    p->freelist = curblock;
}

static void finishsort(ptrstruct *p, unsigned char *in, uint4 length,
                       uint4 *counts, uint4 *indexlast) {
    uint4 i, block, ct[256];
    ptrblock *curblock;
    unsigned char ch = 0;

    // Swap index pointers
    { ptrblock **swap = p->index; p->index = p->oldindex; p->oldindex = swap; }

    memset(p->index, 0, p->nrblocks * sizeof(ptrblock *));
    memcpy(ct, counts, 256 * sizeof(uint4));

    block = 0;
    curblock = p->oldindex[block];

    // Process until indexlast
    for (i = 0; i <= *indexlast; i++) {
        unsigned index = i & BLOCKMASK;
        uint4 tmp = ((uint4)curblock->msbytes[index] << 8) | curblock->lsbyte[index];
        ch = in[tmp - 1];
        setptr(p, ct[ch], in[tmp]);
        ct[ch]++;

        // Move to next block if needed
        if (index == BLOCKMASK && block != p->nrblocks - 1) {
            curblock->nextfree = p->freelist;
            p->freelist = curblock;
            block++;
            curblock = p->oldindex[block];
        }
    }

    *indexlast = ct[ch] - 1;

    // Process remaining elements
    for (; i < length; i++) {
        unsigned index = i & BLOCKMASK;
        uint4 tmp = ((uint4)curblock->msbytes[index] << 8) | curblock->lsbyte[index];
        ch = in[tmp - 1];
        setptr(p, ct[ch], in[tmp]);
        ct[ch]++;

        // Move to next block if needed
        if (index == BLOCKMASK && block < p->nrblocks - 1) {
            curblock->nextfree = p->freelist;
            p->freelist = curblock;
            block++;
            curblock = p->oldindex[block];
        }
    }

    curblock->nextfree = p->freelist;
    p->freelist = curblock;

    // Copy back sorted data
    for (i = 0; i < p->nrblocks - 1; i++)
        memcpy(in + i * BLOCKSIZE, p->index[i]->lsbyte, BLOCKSIZE);

    i = p->nrblocks - 1;
    memcpy(in + BLOCKSIZE * i, p->index[i]->lsbyte, length - i * BLOCKSIZE);
}


void sz_srt(unsigned char *inout, uint4 length, uint4 *indexlast, unsigned int order) {
    uint4 i;
    ptrstruct p;
    uint4 counts[256];
    allocptrs(length, &p);
    sortorder2(&p, inout, length, counts, order, indexlast);
    for (i = order - 2; i > 1; i--)
        incsortorder(&p, inout, length, counts, i, indexlast);
    finishsort(&p, inout, length, counts, indexlast);
    freeptrs(&p);
}

void sz_unsrt(unsigned char *in, unsigned char *out, uint4 length, uint4 indexlast, uint4 *counts, unsigned int order) {
    uint4 i, j;
    uint4 *table;
    unsigned char *flags1 = NULL, *flags2 = NULL;
    unsigned char nocounts = (counts == NULL);

    if (nocounts) {
        counts = (uint4 *)heap_caps_malloc(256 * sizeof(uint4), MALLOC_CAP_8BIT);
        if (!counts) sz_error(SZ_NOMEM_SORT);
        for (i = 0; i < length; i++)
            counts[in[i]]++;
    }
    table = (uint4 *)heap_caps_malloc((length + 1) * sizeof(uint4), MALLOC_CAP_8BIT);
    if (!table)
        sz_error(SZ_NOMEM_SORT);
    memset(table, 0, (length + 1) * sizeof(uint4));
    table[length] = INDIRECT;
    if (nocounts)
        free(counts);
    free(table);
}
