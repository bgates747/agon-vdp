#include "port.h"
#include <esp_system.h>

#define swap(x,y) {uint4 tmp = *(x); *(x) = *(y); *(y) = tmp;}

/* prototypes for local routines */
static void shortsort(uint4 *lo, uint4 *hi, unsigned char *data, uint4 minmatch);

static inline int qscmp(uint4 a, uint4 b, unsigned char *data, uint4 *ml) {
    unsigned char *a1 = data + a;
    unsigned char *b1 = data + b;
    while (a1 > data && *a1 == *b1) {
        a1--; 
        b1--;
    }
    *ml = data + a - a1;
    return (*a1 <= *b1) ? -1 : 1;
}

static int qscompare(uint4 a, uint4 b, unsigned char *data, uint4 *ml) {
    return (a < b) ? qscmp(a, b, data, ml) : -qscmp(b, a, data, ml);
}

static void checksort(uint4 *lo, uint4 *hi, unsigned char *data) {
    uint4 ml;
    while (lo < hi) {
        if (qscompare(*lo, *(lo + 1), data, &ml) != -1) {
            ml = 1;
        }
        lo++;
    }
}

#define CUTOFF 8 /* cutoff for insertion sort */

static void qsort_u4(uint4 *base, uint4 num, unsigned char *data, uint4 minmatch) {
    uint4 *lo, *hi, *mid, *loguy, *higuy;
    uint4 size, *lostk[30], *histk[30], mm[30];
    uint4 lomm, himm;
    int stkptr = 0;

    if (num < 2) return;
    lo = base;
    hi = base + (num - 1);

recurse:
    size = (hi - lo) + 1;
    if (size <= CUTOFF) {
        shortsort(lo, hi, data, minmatch);
    } else {
        uint4 ml;
        mid = lo + (esp_random() % size);
        swap(mid, lo);

        loguy = lo;
        higuy = hi + 1;
        lomm = num - minmatch;
        himm = num - minmatch;
        ml = num - minmatch;

        for (;;) {
            do {
                if (ml < lomm) lomm = ml;
                loguy++;
            } while (loguy <= hi && qscompare(*loguy - minmatch, *lo - minmatch, data, &ml) <= 0);

            do {
                if (ml < himm) himm = ml;
                higuy--;
            } while (higuy > lo && qscompare(*higuy - minmatch, *lo - minmatch, data, &ml) >= 0);

            if (higuy < loguy) break;
            swap(loguy, higuy);
        }
        if (ml < lomm) lomm = ml;

        swap(lo, higuy);

        if (higuy - 1 - lo >= hi - loguy) {
            if (lo + 1 < higuy) {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy - 1;
                mm[stkptr] = lomm + minmatch;
                ++stkptr;
            }
            if (loguy < hi) {
                lo = loguy;
                minmatch += himm;
                goto recurse;
            }
        } else {
            if (loguy < hi) {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
                mm[stkptr] = himm + minmatch;
                ++stkptr;
            }
            if (lo + 1 < higuy) {
                hi = higuy - 1;
                minmatch += lomm;
                goto recurse;
            }
        }
    }

    --stkptr;
    if (stkptr >= 0) {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        minmatch = mm[stkptr];
        goto recurse;
    }
}

static void shortsort(uint4 *lo, uint4 *hi, unsigned char *data, uint4 minmatch) {
    uint4 *p, *max, ml;

    while (hi > lo) {
        max = lo;
        for (p = lo + 1; p <= hi; p++) {
            if (qscompare(*p - minmatch, *max - minmatch, data, &ml) > 0) {
                max = p;
            }
        }
        swap(max, hi);
        hi--;
    }
}
