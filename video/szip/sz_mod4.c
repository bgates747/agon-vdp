#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "sz_mod4.h"

#define RLSHIFT 10
#define MTFSHIFT 10

#define FULLFLAG (m->cache - 1)
#define MTFFLAG (m->cache - 2)

#ifdef MODELGLOBAL
SzipModel mod;
#define dumpcache(m) M_dumpcache()
#define decodewhat(m) M_decodewhat()
#define finishupdate(m, a) M_finishupdate(a)
#define addtomtf(m, a) M_addtomtf(a)
#define encodeother(m, a) M_encodeother(a)
#define readrunlength(m) M_readrun()
#define activatenext(m, a) M_activatenext(a)
#define MOD mod
#else
#define MOD (*m)
#endif

/* Initialize the model */
/* headersize -1 means decompression */
/* first is the first byte written by the arithmetic coder */
void initmodel(SzipModel *m, int headersize, uint8_t *first) {  
    int i;

    /* Initialize the arithmetic coder */
    m->compress = (headersize >= 0);
    if (m->compress)
        start_encoding(&(m->ac), *first, headersize);
    else
        *first = start_decoding(&(m->ac));

    /* Initialize the full model */
    initbitmodel(&(m->full), ALPHABETSIZE, 40 * ALPHABETSIZE, 10 * ALPHABETSIZE, NULL);
    for (i = 0; i < ALPHABETSIZE; i++)
        m->lastseen[i] = FULLFLAG;

    /* Initialize the cache with symbols CACHESIZE-1 to 0 */
    cacheptr tmp = m->cache;
    for (i = 0; i < CACHESIZE - 1; i++) {
        tmp->next = tmp + 1;
        tmp->prev = tmp - 1;
        tmp->symbol = CACHESIZE - 2 - i;
        m->lastseen[tmp->symbol] = tmp;
        bitdeactivate(&(m->full), tmp->symbol);
        tmp->sy_f = 1;
        tmp->weight = 1;
        tmp->what = 0;
        tmp++;
    }
    m->cache[0].prev = m->cache + (CACHESIZE - 1);
    tmp->next = m->cache;
    tmp->prev = tmp - 1;
    tmp->sy_f = 0;

    m->newest = m->cache + (CACHESIZE - 2);
    m->lastnew = m->cache + (CACHESIZE - 7);
    m->cachetotf = CACHESIZE; // Initially decremented by 1 later

    /* Initialize the whatmodel */
    m->whatmod[0] = 41; // 1 + 22*1 + 3*6
    m->whatmod[1] = 8;  // 1 + 1*1 + 1*6
    m->whatmod[2] = 15; // 1 + 2*1 + 2*6

    /* Make 2 old and 2 new full hits for what */
    for (i = 0; i < 2; i++) {
        m->cache[i].what = 2;
        m->lastnew[i].what = 2;
    }
    /* Make 1 old and 1 new hit for MTF */
    m->cache[2].what = 1;
    m->lastnew[2].what = 1;

    /* Initialize the MTF models with symbols CACHESIZE .. (CACHESIZE+MTFSIZE<<1) */
    m->mtfhist[0].next = MTFHISTSIZE - 1;
    m->mtfhist[0].sym = CACHESIZE;
    for (i = 1; i < MTFSIZE << 1; i++) {
        m->mtfhist[i].next = i - 1;
        m->mtfhist[i].sym = CACHESIZE + i;
    }
    for (; i < MTFHISTSIZE; i++)
        m->mtfhist[i].next = 0xffff;
    
    m->mtfsize = MTFSIZE << 1;
    m->mtfsizeact = 0;
    m->mtffirst = (MTFSIZE << 1) - 1;
    initqsmodel(&(m->mtfmod), MTFSIZE, MTFSHIFT, 400, NULL, m->compress);

    /* Initialize the run-length models */
    for (i = 0; i < 5; i++)
        initqsmodel(m->rlemod + i, 7, RLSHIFT, 150, NULL, m->compress);
}


/* Call after encoding/decoding first run */
void fixafterfirst(SzipModel *m) {
    m->cachetotf--;
}

/* Delete the model */
void deletemodel(SzipModel *m) {
    int i;
    if (m->compress) {
        m->ac.bytecount = done_encoding(&(m->ac));
    } else {
        done_decoding(&(m->ac));
    }

    deletebitmodel(&(m->full));
    deleteqsmodel(&(m->mtfmod));
    for (i = 0; i < 5; i++) {
        deleteqsmodel(m->rlemod + i);
    }
}

/* Encode a run of equal symbols */
void sz_encode(SzipModel *m, uint32_t symbol, uint32_t runlength) {
    cacheptr tmp;

    if ((tmp = m->lastseen[symbol]) >= m->cache) {
        uint32_t lt_f;
        cacheptr old = tmp;
        tmp = tmp->next;
        while (tmp != m->newest) {
            lt_f += tmp->sy_f;
            tmp = tmp->next;
        }
        encode_freq(&(m->ac), old->sy_f, lt_f, m->cachetotf - tmp->sy_f);
        tmp = tmp->next;
        tmp->what = 0;
        tmp->weight = runlength;
        tmp->sy_f = tmp->weight + old->sy_f;
        old->sy_f = 0;
        m->newest = tmp;
    } else {
        tmp = m->newest->next;
        tmp->what = 2;
        tmp->weight = runlength;
        tmp->sy_f = tmp->weight;
        m->newest = tmp;
    }
    m->lastseen[symbol] = tmp;
}

/* Decode a run of equal symbols */
void sz_decode(SzipModel *m, uint32_t *symbol, uint32_t *runlength) {
    uint32_t sym;
    sym = decode_culshift(&(m->ac), 6);

    if (sym < m->whatmod[0]) {
        uint32_t lt_f, tot_f;
        cacheptr tmp = m->newest;
        tot_f = m->cachetotf - tmp->sy_f;
        sym = decode_culfreq(&(m->ac), tot_f);
        tmp = tmp->prev;
        lt_f = tmp->sy_f;
        while (lt_f <= sym) {
            tmp = tmp->prev;
            lt_f += tmp->sy_f;
        }
        decode_update(&(m->ac), tmp->sy_f, lt_f - tmp->sy_f, tot_f);
        tmp->sy_f = 0;
        *symbol = tmp->symbol;
    } else {
        uint32_t sy_f, lt_f;
        sym = qsgetsym(&(m->mtfmod), decode_culshift(&(m->ac), MTFSHIFT));
        qsgetfreq(&(m->mtfmod), sym, &sy_f, &lt_f);
        decode_update_shift(&(m->ac), sy_f, lt_f, MTFSHIFT);
        qsupdate(&(m->mtfmod), sym);
        *symbol = sym;
    }
}
