#ifndef SZ_SRT_H
#define SZ_SRT_H

#include "port.h"

#ifdef __cplusplus
extern "C" {
#endif

// Sorts `inout`, producing sorted bytes in-place. Must be length + order bytes long.
// `length`: Number of bytes in `inout`
// `indexlast`: Pointer returning the position of the last context (for unsorting)
// `order`: Order of context used in sorting (must be >=3)
void sz_srt(unsigned char *inout, uint4 length, uint4 *indexlast, unsigned int order);

// Unsorts `in`, producing output in `out` (or stdout if `out == NULL`).
// `length`: Number of bytes in `in`
// `indexlast`: Position of last context (from `sz_srt`)
// `counts`: Byte occurrence counts (if NULL, they are computed)
// `order`: Context order used in sorting (must be >=3)
void sz_unsrt(unsigned char *in, unsigned char *out, uint4 length, uint4 indexlast,
              uint4 *counts, unsigned int order);

// Alternate sorting methods
#define SZ_SRT_O4
#define SZ_SRT_BW

#if defined(SZ_SRT_O4)
// Alternative order-4 sorting method
void sz_srt_o4(unsigned char *inout, uint4 length, uint4 *indexlast);
#endif

#if defined(SZ_UNSRT_O4)
// Alternative order-4 unsorting using hashing
void sz_unsrt_o4(unsigned char *in, unsigned char *out, uint4 length, uint4 indexlast,
                 uint4 *counts);
#endif

#if defined(SZ_SRT_BW)
// Unlimited-context sort (BWT variant)
void sz_srt_BW(unsigned char *inout, uint4 length, uint4 *indexfirst);

// Unsorting function for BWT variant
void sz_unsrt_BW(unsigned char *in, unsigned char *out, uint4 length,
                 uint4 indexfirst, uint4 *counts);
#endif

#ifdef __cplusplus
}
#endif

#endif // SZ_SRT_H
