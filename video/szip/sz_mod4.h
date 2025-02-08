#ifndef SZ_MODEL4_H
#define SZ_MODEL4_H

#include <stdint.h>
#include "port.h"
#include "qsmodel.h"
#include "bitmodel.h"
#include "rangecod.h"

#define ALPHABETSIZE 256
#define CACHESIZE 32
#define MTFSIZE 20
#define MTFHISTSIZE 4096  /* Must be a power of 2 */
// #define MODELGLOBAL // We disable because this causes all sorts of problems with functions which take model as an argument


typedef struct {
    uint32_t sym;
    uint32_t next;
} MTFEntry;

typedef struct CacheEntryS *CachePtr;

typedef struct CacheEntryS {
    uint8_t symbol, sy_f, weight, what;
    CachePtr next, prev;
} CacheEntry;

typedef struct {
    uint32_t whatmod[3];    // Probabilities for the submodels
    CachePtr newest;        // Points to the newest element in cache
    CachePtr lastnew;       // Points to the last element with higher weight
    uint32_t cachetotf;     // Total frequency count in cache
    uint32_t mtffirst;      // Position of the newest entry in mtfhist
    uint32_t mtfsize;       // Size of the MTF list
    uint32_t mtfsizeact;    // Size of the active MTF list
    CachePtr lastseen[ALPHABETSIZE]; // Tracks symbol positions in cache
    CacheEntry cache[CACHESIZE];     // Cache
    MTFEntry mtfhist[MTFHISTSIZE];   // MTF history
    BitModel full;          // Fallback model
    QSModel mtfmod;         // Probabilities for MTF ranks
    QSModel rlemod[5];      // Run-length encoding models
    RangeCoder ac;
    uint32_t compress;      // 1 for compression, 0 for decompression
} SzipModel;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MODELGLOBAL
#define initmodel(m, a, b) M_initmodel(a, b)
#define fixafterfirst(m) M_fixafterfirst()
#define deletemodel(m) M_deletemodel()
#define sz_finishrun(m) M_sz_finishrun()
#define sz_encode(m, a, b) M_sz_encode(a, b)
#define sz_decode(m, a, b) M_sz_decode(a, b)
#endif

/**
 * Initialize the model.
 * 
 * @param m - Pointer to the SzipModel struct.
 * @param headersize - -1 for decompression, otherwise specifies header size.
 * @param first - Pointer to the first byte written by the arithmetic coder.
 */
void initmodel(SzipModel *m, int headersize, uint8_t *first);

/**
 * Call this after encoding/decoding the first run.
 * 
 * @param m - Pointer to the SzipModel struct.
 */
void fixafterfirst(SzipModel *m);

/**
 * Free memory and delete the model.
 * 
 * @param m - Pointer to the SzipModel struct.
 */
void deletemodel(SzipModel *m);

/**
 * Encode a run of equal symbols.
 * 
 * @param m - Pointer to the SzipModel struct.
 * @param symbol - Symbol to encode.
 * @param runlength - Length of the run.
 */
void sz_encode(SzipModel *m, uint32_t symbol, uint32_t runlength);

/**
 * Decode a run of equal symbols.
 * 
 * @param m - Pointer to the SzipModel struct.
 * @param symbol - Pointer to store the decoded symbol.
 * @param runlength - Pointer to store the decoded run length.
 */
void sz_decode(SzipModel *m, uint32_t *symbol, uint32_t *runlength);

#ifdef __cplusplus
}
#endif

#endif // SZ_MODEL4_H
