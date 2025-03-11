#ifndef SZIP_H
#define SZIP_H

#include "agon.h" // for debug_log
#include <cstring>
#include <esp_heap_caps.h>
#include <stdint.h>
#include <esp32-hal-psram.h>

// =================================================================================================
// port.h
// -------------------------------------------------------------------------------------------------
#if !defined port_h
#define port_h

#define Inline __inline

#include <sys/types.h>
#define uint2 u_int16_t
#define uint4 u_int32_t


#endif // port_h
// =================================================================================================
// bitmodel.h
// -------------------------------------------------------------------------------------------------
#ifndef BITMODEL_H
#define BITMODEL_H

// #include "port.h"

#define EXCLUDEONUPDATE

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
    int n,             /* number of symbols */
        totalfreq,     /* total frequency count (without excluded symbols) */
        max_totf,      /* maximum allowed total frequency count */
        incr,          /* increment per update */
        mask;          /* initial bitmask used for search */
    uint2 *f,          /* frequency for the symbol; first bit set if excluded */
        *cf;           /* array of cumulative frequencies */
} bitmodel;

void initbitmodel( bitmodel *m, int n, int max_totf, int rescale, int *init );
void resetbitmodel( bitmodel *m, int *init);
void deletebitmodel( bitmodel *m );
void bitgetfreq( bitmodel *m, int sym, int *sy_f, int *lt_f);
#define bittotf(m) ((m)->totalfreq)
int bitgetsym( bitmodel *m, int lt_f );
void bitupdate( bitmodel *m, int sym );

#ifdef EXCLUDEONUPDATE
void bitupdate_ex( bitmodel *m, int sym );
void bitdeactivate( bitmodel *m, int sym );
void bitreactivate( bitmodel *m, int sym );
#endif // EXCLUDEONUPDATE

#endif // BITMODEL_H

#ifdef __cplusplus
}
#endif // __cplusplus
// =================================================================================================
// qsmodel.h
// -------------------------------------------------------------------------------------------------
#ifndef QSMODEL_H
#define QSMODEL_H

// #include "port.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
    int n,             /* number of symbols */
        left,          /* symbols to next rescale */
        nextleft,      /* symbols with other increment */
        rescale,       /* intervals between rescales */
        targetrescale, /* should be interval between rescales */
        incr,          /* increment per update */
        searchshift;   /* shift for lt_freq before using as index */
    uint2 *cf,         /* array of cumulative frequencies */
        *newf,         /* array for collecting ststistics */
        *search;       /* structure for searching on decompression */
} qsmodel;

void initqsmodel( qsmodel *m, int n, int lg_totf, int rescale, int *init, int compress );
void resetqsmodel( qsmodel *m, int *init);
void deleteqsmodel( qsmodel *m );
void qsgetfreq( qsmodel *m, int sym, int *sy_f, int *lt_f );
int qsgetsym( qsmodel *m, int lt_f );
void qsupdate( qsmodel *m, int sym );

#endif // QSMODEL_H

#ifdef __cplusplus
}
#endif // __cplusplus

typedef struct {
    const unsigned char *sourceBuffer;  /* pointer to the input data */
    size_t sourceSize;    /* total size of input data */
    size_t sourcePos;     /* current read position */
} szip_stream;

#endif // SZIP_H
// =================================================================================================
// rangecod.h
// -------------------------------------------------------------------------------------------------
#ifndef rangecod_h
#define rangecod_h

extern char coderversion[];

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef uint4 code_value;       /* Type of an rangecode value       */
                                /* must accomodate 32 bits          */
/* it is highly recommended that the total frequency count is less  */
/* than 1 << 19 to minimize rounding effects.                       */
/* the total frequency count MUST be less than 1<<23                */

typedef uint4 freq; 

/* make the following private in the arithcoder object in C++	    */

typedef struct {
    uint4 low,           /* low end of interval */
          range,         /* length of interval */
          help;          /* bytes_to_follow resp. intermediate value */
    unsigned char buffer;/* buffer for input/output */
/* the following is used only when encoding */
    uint4 bytecount;     /* counter for outputed bytes  */
/* insert fields you need for input/output below this line! */
// actually don't do that; it's a bad idea since rangecoder gets reinitialized for every block
// see the new `szip_stream` struct for the Right Way to do this
} rangecoder;

int start_decoding( rangecoder *rc, szip_stream *stream );
freq decode_culfreq( rangecoder *rc, freq tot_f, szip_stream *stream );
freq decode_culshift( rangecoder *rc, freq shift, szip_stream *stream );
void decode_update( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f);
#define decode_update_shift(rc,f1,f2,f3) decode_update((rc),(f1),(f2),(freq)1<<(f3));
unsigned char decode_byte( rangecoder *rc, szip_stream *stream );
unsigned short decode_short( rangecoder *rc, szip_stream *stream );
void done_decoding( rangecoder *rc, szip_stream *stream );

#endif // rangecod_h

#ifdef __cplusplus
}
#endif // __cplusplus
// =================================================================================================
// reorder.h
// -------------------------------------------------------------------------------------------------
// #include "port.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void reorder(unsigned char *in, unsigned char *out, uint4 length, uint recordsize);
void unreorder(unsigned char *in, unsigned char *out, uint4 length, uint recordsize);

#ifdef __cplusplus
}
#endif // __cplusplus
// =================================================================================================
// sz_err.h
// -------------------------------------------------------------------------------------------------
// SZ error messages
#ifndef ERR_H
#define ERR_H

#define sz_error(x) do{fprintf(stderr,"Error #%x\n",x); abort();}while(0)

// those are ok:
#define NOMEM			0x6500
#define SZ_NOMEM_HASH		0x6502
#define SZ_NOMEM_SORT		0x6503

// those are a bug:
#define UNEXPECTED		0x6600
#define SZ_NOTCYCLIC		0x6601
#define SZ_NOTFOUND			0x6602
#define SZ_NOTIMPLEMENTED	0x6603
#define SZ_DOUBLEINDIRECT   0x6604
#define AR_OUTSTANDING		0x6605

#endif // ERR_H

// =================================================================================================
// sz_mod4.h
// -------------------------------------------------------------------------------------------------
/* sz_model4.h (c) Michael Schindler 1998 */
#ifndef SZ_MODEL4_H
#define SZ_MODEL4_H

// #include "szip_config.h"
// #include "port.h"
// #include "qsmodel.h"
// #include "bitmodel.h"
// #include "rangecod.h"

#define ALPHABETSIZE 256
#define CACHESIZE 32
#define MTFSIZE 20
#define MTFHISTSIZE 256  /* must pe power of 2 */
// #define MODELGLOBAL

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
    uint sym, next;
} mtfentry;

typedef struct cacheS *cacheptr;

typedef struct cacheS {
    unsigned char symbol, sy_f, weight, what;
    cacheptr next, prev;
} cacheentry;

typedef struct {
    uint whatmod[3];  /* probabilities for the submodels */
    cacheptr newest,  /* points to newest element in cache */
             lastnew; /* points to last element with heigher weight */
    uint cachetotf;   /* total frequency count in cache */
    uint mtffirst;    /* where to find the newest entry in mtfhist */
    uint mtfsize;     /* size of mtflist */
    uint mtfsizeact;  /* size of active mtflist */
    cacheptr lastseen[ALPHABETSIZE]; /* tell if and where symbol is in cache */
    cacheentry cache[CACHESIZE]; /* cache */
    mtfentry mtfhist[MTFHISTSIZE];
    bitmodel full;    /* fallback model */
    qsmodel mtfmod;   /* probabilities for mtf ranks */
    qsmodel rlemod[5];
    rangecoder ac;
    uint compress;    /* 1 on compression, 0 on decompression */
} sz_model;

void initmodel(sz_model *m, int headersize, unsigned char *first, szip_stream *stream);
void fixafterfirst(sz_model *m);
void deletemodel(sz_model *m, szip_stream *stream);
void sz_decode(sz_model *m, uint *symbol, uint4 *runlength, szip_stream *stream);

#endif // SZ_MODEL4_H

#ifdef __cplusplus
}
#endif // __cplusplus
// =================================================================================================
// sz_srt.h
// -------------------------------------------------------------------------------------------------
#ifndef SZ_SRT_H
#define SZ_SRT_H
// #include "port.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void sz_srt(unsigned char *inout, uint4 length, uint4 *indexlast, unsigned int order);
void sz_unsrt(unsigned char *in, unsigned char *out, uint4 length, uint4 indexlast, uint4 *counts, unsigned int order);


// comment the following #defines if you dont want them
#define SZ_SRT_O4
//#define SZ_UNSRT_O4
#define SZ_SRT_BW

// alternate sorter for order 4 (different method, same result)
#if defined SZ_SRT_O4
void sz_srt_o4(unsigned char *inout, uint4 length, uint4 *indexlast);
#endif // SZ_SRT_O4


// alternate unsorter for order 4 (different method (hash), same result)
#if defined SZ_UNSRT_O4
void sz_unsrt_o4(unsigned char *in, unsigned char *out, uint4 length, uint4 indexlast, uint4 *counts);
#endif // SZ_UNSRT_O4


#if defined SZ_SRT_BW
// unsorter for unlimited context sort
void sz_unsrt_BW(unsigned char *in, unsigned char *out, uint4 length, uint4 indexfirst, uint4 *counts);
#endif // SZ_SRT_BW
#endif // SZ_SRT_H

#ifdef __cplusplus
}
#endif // __cplusplus
// =================================================================================================
// bitmodel.c
// -------------------------------------------------------------------------------------------------

// #include "bitmodel.h"
// #include <stdio.h>     /* NULL */
// #include <stdlib.h>    /* malloc, free */


/* constructs the b_i_t structire */
static Inline void build_cf(bitmodel *m)
{   int i;
    uint2 *cf;
    cf = m->cf;
    m->totalfreq = 0;
    for (i=1; i<=m->n; i<<=1)
    {   int j;
        for (j=i; j<=m->n; j+= i<<1)
        {   int k;
#ifdef EXCLUDEONUPDATE
            if (m->f[j-1] & 0x8000)
                cf[j] = 0;
            else
#endif // EXCLUDEONUPDATE
                m->totalfreq += cf[j] = m->f[j-1];
            for (k=i>>1; k; k>>=1)
                cf[j] += cf[j-k];
        } /* end for j */
    } /* end for i */
}


/* scales the culmulative frequency tables by 0.5 and keeps nonzero values */
static void scalefreq(bitmodel *m)
{   uint2 *f, *endf;
    for (f=m->f, endf = f+m->n; f<endf; f++)
#ifdef EXCLUDEONUPDATE
        *f = ((1+(*f & 0x7fff))>>1) | (*f & 0x8000);
#else
        *f = (1 + *f)>>1;
#endif // EXCLUDEONUPDATE
    build_cf(m);
}


/* initialisation of bitmodel                          */
/* m   bitmodel to be initialized                      */
/* n   number of symbols in that model                 */
/* max_totf  maximum allowed total frequency count     */
/* rescale  desired rescaling interval, must be <max_totf/2 */
/* init  array of int's to be used for initialisation (NULL ok) */
void initbitmodel( bitmodel *m, int n, int max_totf, int rescale,
    int *init )
{   m->n = n;
    if (max_totf < n<<1) max_totf = n<<1;
    m->max_totf = max_totf;
    m->incr = max_totf/2/rescale;
    if (m->incr < 1) m->incr = 1;
    m->f = (uint2*) malloc(n*sizeof(uint2));
    m->cf = (uint2*) malloc((n+1)*sizeof(uint2));
    m->mask = 1;
    while (n>>=1)
        m->mask <<=1;
    resetbitmodel(m,init);
}



/* reinitialisation of bitmodel                        */
/* m   bitmodel to be initialized                      */
/* init  array of int's to be used for initialisation (NULL ok) */
void resetbitmodel( bitmodel *m, int *init)
{   int i;
    if (init == NULL)
    {   for(i=0; i<m->n; i++)
            m->f[i] = 1;
        m->totalfreq = m->n;
    } else
    {   m->totalfreq = 0;
        for(i=0; i<m->n; i++)
        {   m->f[i] = init[i];
            m->totalfreq += init[i];
        }
    }
    while (m->totalfreq > m->max_totf)
        scalefreq(m);
    build_cf(m);
}


/* deletion of bitmodel m                              */
void deletebitmodel( bitmodel *m )
{   free(m->f);
    free(m->cf);
    m->n = 0;
}


/* retrieval of estimated frequencies for a symbol     */
/* m   bitmodel to be questioned                       */
/* sym  symbol for which data is desired; must be <n   */
/* sy_f frequency of that symbol                       */
/* lt_f frequency of all smaller symbols together      */
/* the total frequency can be obtained with bit_totf   */
void bitgetfreq( bitmodel *m, int sym, int *sy_f, int *lt_f)
{   int cul;
    uint2 *cf;
    *sy_f = m->f[sym];
    sym++;
    cf = m->cf;
    cul = cf[sym];
    while (sym &= sym-1)
        cul += cf[sym];
    *lt_f = cul - *sy_f;
}


/* find out symbol for a given cumulative frequency    */
/* m   bitmodel to be questioned                       */
/* lt_f  cumulative frequency                          */
int bitgetsym( bitmodel *m, int lt_f )
{   int sym, mask, n;
    uint2 *cf;
    mask = m->mask;
    n = m->n;
    cf = m->cf;
    sym = 0;
    do
    {   int x;
        if ((x=sym|mask) <= n && lt_f >= cf[x])
        {   lt_f -= cf[x];
            sym = x;
        }
    } while (mask >>= 1);
    return sym;
}


/* update the cumulative frequency data by delta */
static Inline void bit_cfupd( bitmodel *m, int sym, int delta )
{   m->totalfreq += delta;
    if (m->totalfreq > m->max_totf)
        scalefreq(m);
    else
    {   uint2 *cf;
        sym++;
        cf = m->cf;
        while (sym<= m->n)
        {   cf[sym] += delta;
            sym = (sym | (sym-1)) + 1;
        }
    }
}


/* update model                                        */
/* m   bitmodel to be updated                          */
/* sym  symbol that occurred (must be <n from init)    */
void bitupdate( bitmodel *m, int sym )
{   m->f[sym] += m->incr;
    bit_cfupd(m, sym, m->incr);
}


#ifdef EXCLUDEONUPDATE
/* update model and exclude symbol                     */
/* m   bitmodel to be updated                          */
/* sym  symbol that occurred (must be <n from init)    */
void bitupdate_ex( bitmodel *m, int sym )
{   int delta;
    delta = -m->f[sym];
    m->f[sym] = (m->f[sym] + m->incr) | 0x8000;
    bit_cfupd(m, sym, delta);
}


/* deactivate symbol                                   */
/* m   bitmodel to be updated                          */
/* sym  symbol to be reactivated                       */
void bitdeactivate( bitmodel *m, int sym )
{   bit_cfupd(m, sym, -m->f[sym]);
    m->f[sym] |= 0x8000;
}


/* reactivate symbol                                   */
/* m   bitmodel to be updated                          */
/* sym  symbol to be reactivated                       */
void bitreactivate( bitmodel *m, int sym )
{   m->f[sym] &= 0x7fff;
    bit_cfupd(m, sym, m->f[sym]);
}
#endif // EXCLUDEONUPDATE
// =================================================================================================
// qsmodel.c
// -------------------------------------------------------------------------------------------------

// #include "qsmodel.h"
// #include <stdio.h>
// #include <stdlib.h>

/* default tablesize 1<<TBLSHIFT */
#define TBLSHIFT 7

/* rescale frequency counts */
static void dorescale( qsmodel *m)
{   int i, cf, missing;
    if (m->nextleft)  /* we have some more before actual rescaling */
    {   m->incr++;
        m->left = m->nextleft;
        m->nextleft = 0;
        return;
    }
    if (m->rescale < m->targetrescale)  /* double rescale interval if needed */
    {   m->rescale <<= 1;
        if (m->rescale > m->targetrescale)
            m->rescale = m->targetrescale;
    }
    cf = missing = m->cf[m->n];  /* do actual rescaling */
    for(i=m->n-1; i; i--)
    {   int tmp = m->newf[i];
        cf -= tmp;
        m->cf[i] = cf;
        tmp = tmp>>1 | 1;
        missing -= tmp;
        m->newf[i] = tmp;
    }
    if (cf!=m->newf[0])
    {   fprintf(stderr,"BUG: rescaling left %d total frequency\n",cf);
        deleteqsmodel(m);
        exit(1);
    }
    m->newf[0] = m->newf[0]>>1 | 1;
    missing -= m->newf[0];
    m->incr = missing / m->rescale;
    m->nextleft = missing % m->rescale;
    m->left = m->rescale - m->nextleft;
    if (m->search != NULL)
    {   i=m->n;
        while (i)
        {   int start, end;
            end = (m->cf[i]-1) >> m->searchshift;
            i--;
            start = m->cf[i] >> m->searchshift;
            while (start<=end)
            {   m->search[start] = i;
                start++;
            }
        }
    }
}


/* initialisation of qsmodel                           */
/* m   qsmodel to be initialized                       */
/* n   number of symbols in that model                 */
/* lg_totf  base2 log of total frequency count         */
/* rescale  desired rescaling interval, should be < 1<<(lg_totf+1) */
/* init  array of int's to be used for initialisation (NULL ok) */
/* compress  set to 1 on compression, 0 on decompression */
void initqsmodel( qsmodel *m, int n, int lg_totf, int rescale, int *init, int compress )
{   m->n = n;
    m->targetrescale = rescale;
    m->searchshift = lg_totf - TBLSHIFT;
    if (m->searchshift < 0)
        m->searchshift = 0;
    m->cf = (uint2*) malloc((n+1)*sizeof(uint2));
    m->newf = (uint2*) malloc((n+1)*sizeof(uint2));
    m->cf[n] = 1<<lg_totf;
    m->cf[0] = 0;
    if (compress)
        m->search = NULL;
    else
    {   m->search = (uint2*) malloc(((1<<TBLSHIFT)+1)*sizeof(uint2));
        m->search[1<<TBLSHIFT] = n-1;
    }
    resetqsmodel(m, init);
}


/* reinitialisation of qsmodel                         */
/* m   qsmodel to be initialized                       */
/* init  array of int's to be used for initialisation (NULL ok) */
void resetqsmodel( qsmodel *m, int *init)
{   int i, end, initval;
    m->rescale = m->n>>4 | 2;
    m->nextleft = 0;
    if (init == NULL)
    {   initval = m->cf[m->n] / m->n;
        end = m->cf[m->n] % m->n;
        for (i=0; i<end; i++)
            m->newf[i] = initval+1;
        for (; i<m->n; i++)
            m->newf[i] = initval;
    } else
        for(i=0; i<m->n; i++)
            m->newf[i] = init[i];
    dorescale(m);
}


/* deletion of qsmodel m                               */
void deleteqsmodel( qsmodel *m )
{   free(m->cf);
    free(m->newf);
    if (m->search != NULL)
        free(m->search);
}


/* retrieval of estimated frequencies for a symbol     */
/* m   qsmodel to be questioned                        */
/* sym  symbol for which data is desired; must be <n   */
/* sy_f frequency of that symbol                       */
/* lt_f frequency of all smaller symbols together      */
/* the total frequency is 1<<lg_totf                   */
void qsgetfreq( qsmodel *m, int sym, int *sy_f, int *lt_f )
{   *sy_f = m->cf[sym+1] - (*lt_f = m->cf[sym]);
}	


/* find out symbol for a given cumulative frequency    */
/* m   qsmodel to be questioned                        */
/* lt_f  cumulative frequency                          */
int qsgetsym( qsmodel *m, int lt_f )
{   int lo, hi;
    uint2 *tmp;
    tmp = m->search+(lt_f>>m->searchshift);
    lo = *tmp;
    hi = *(tmp+1) + 1;
    while (lo+1 < hi )
    {   int mid = (lo+hi)>>1;
        if (lt_f < m->cf[mid])
            hi = mid;
        else
            lo = mid;
    }
    return lo;
}


/* update model                                        */
/* m   qsmodel to be updated                           */
/* sym  symbol that occurred (must be <n from init)    */
void qsupdate( qsmodel *m, int sym )
{   if (m->left <= 0)
        dorescale(m);
    m->left--;
    m->newf[sym] += m->incr;
}
// =================================================================================================
// qsort_u4.c
// -------------------------------------------------------------------------------------------------
// #include "port.h"
// #include <stdlib.h>

#define szip_swap(x,y) {uint4 tmp = *(x); *(x) = *(y); *(y) = tmp;}

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* prototypes for local routines */
static void shortsort ( uint4 *lo, uint4 *hi, unsigned char *data, uint4 minmatch );

static Inline int qscmp(uint4 a, uint4 b, unsigned char *data, uint4 *ml)
{	unsigned char *a1,*b1;
    a1 = data + a;
	b1 = data + b;
	while (a1 > data && *a1 == *b1)
	{	a1--; b1--;
	}
	*ml = data+a-a1;
	if (*a1 <= *b1)
			return -1;
	return 1;
}

static int qscompare(uint4 a, uint4 b, unsigned char *data, uint4 *ml)
{	if (a<b)
		return qscmp(a,b,data,ml);
	else 
		return -qscmp(b,a,data,ml);
}


static void checksort(uint4 *lo, uint4 *hi, unsigned char *data)
{   uint4 ml;
    while(lo<hi)
    {   if (qscompare(*lo,*(lo+1),data,&ml)!= -1)
            ml=1;
        lo++;
    }
}        


/* this parameter defines the cutoff between using quick sort and
   insertion sort for arrays; arrays with lengths shorter or equal to the
   below value use insertion sort */

#define CUTOFF 8            /* testing shows that this is good value */


/***
*qsort(base, num, wid, comp) - quicksort function for sorting arrays
*
*Purpose:
*       quicksort the array of elements
*       side effects:  sorts in place
*
*Entry:
*       char *base = pointer to base of array
*       unsigned num  = number of elements in the array
*       unsigned width = width in bytes of each array element
*       int (*comp)() = pointer to function returning analog of strcmp for
*               strings, but supplied by user for comparing the array elements.
*               it accepts 2 pointers to elements and returns neg if 1<2, 0 if
*               1=2, pos if 1>2.
*
*Exit:
*       returns void
*
*Exceptions:
*
*******************************************************************************/

/* sort the array between lo and hi (inclusive) */

static void qsort_u4 ( uint4 *base, uint4 num, unsigned char *data, uint4 minmatch )
{
    uint4 *lo, *hi;             /* ends of sub-array currently sorting */
    uint4 *mid;                 /* points to middle of subarray */
    uint4 *loguy, *higuy;       /* traveling pointers for partition step */
    uint4 size;                 /* size of the sub-array */
    uint4 *lostk[30], *histk[30], mm[30];
	uint4 lomm, himm;			/* minmatch for low/high */
    int stkptr;                 /* stack for saving sub-array to be processed */

    /* Note: the number of stack entries required is no more than
       1 + log2(size), so 30 is sufficient for any array */

    if (num < 2)
        return;                 /* nothing to do */

    stkptr = 0;                 /* initialize stack */

    lo = base;
    hi = base + (num-1);        /* initialize limits */

    /* this entry point is for pseudo-recursion calling: setting
       lo and hi and jumping to here is like recursion, but stkptr is
       prserved, locals aren't, so we preserve stuff on the stack */
recurse:

    size = (hi - lo) + 1;        /* number of el's to sort */

    /* below a certain size, it is faster to use a O(n^2) sorting method */
    if (size <= CUTOFF) {
         shortsort(lo, hi, data, minmatch);
    }
    else {
		uint4 ml;
        /* First we pick a partititioning element.  The efficiency of the
           algorithm demands that we find one that is approximately the
           median of the values, but also that we select one fast.  Using
           the first one produces bad performace if the array is already
           sorted, so we use the middle one, which would require a very
           wierdly arranged array for worst case performance.  Testing shows
           that a median-of-three algorithm does not, in general, increase
           performance. */

        mid = lo + rand()%size;     /* find middle element */
        szip_swap(mid, lo)               /* swap it to beginning of array */

        /* We now wish to partition the array into three pieces, one
           consisiting of elements <= partition element, one of elements
           equal to the parition element, and one of element >= to it.  This
           is done below; comments indicate conditions established at every
           step. */

        loguy = lo;
        higuy = hi + 1;
		lomm = num-minmatch;
		himm = num-minmatch;
		ml = num-minmatch;

        /* Note that higuy decreases and loguy increases on every iteration,
           so loop must terminate. */
        for (;;) {
            /* lo <= loguy < hi, lo < higuy <= hi + 1,
               A[i] <= A[lo] for lo <= i <= loguy,
               A[i] >= A[lo] for higuy <= i <= hi */

            do  {
				if (ml<lomm) lomm = ml;
                loguy ++;
            } while (loguy <= hi && qscompare(*loguy-minmatch,*lo-minmatch,data,&ml) <= 0);

            /* lo < loguy <= hi+1, A[i] <= A[lo] for lo <= i < loguy,
               either loguy > hi or A[loguy] > A[lo] */

            do  {
				if (ml<himm) himm = ml;
                higuy --;
            } while (higuy > lo && qscompare(*higuy-minmatch,*lo-minmatch,data,&ml ) >= 0);

            /* lo-1 <= higuy <= hi, A[i] >= A[lo] for higuy < i <= hi,
               either higuy <= lo or A[higuy] < A[lo] */

            if (higuy < loguy)
                break;

            /* if loguy > hi or higuy <= lo, then we would have exited, so
               A[loguy] > A[lo], A[higuy] < A[lo],
               loguy < hi, highy > lo */

            szip_swap(loguy, higuy)

            /* A[loguy] < A[lo], A[higuy] > A[lo]; so condition at top
               of loop is re-established */
        }
		if (ml<lomm) lomm = ml;

		
        /*     A[i] >= A[lo] for higuy < i <= hi,
               A[i] <= A[lo] for lo <= i < loguy,
               higuy < loguy, lo <= higuy <= hi
           implying:
               A[i] >= A[lo] for loguy <= i <= hi,
               A[i] <= A[lo] for lo <= i <= higuy,
               A[i] = A[lo] for higuy < i < loguy */

        szip_swap(lo, higuy)     /* put partition element in place */

        /* OK, now we have the following:
              A[i] >= A[higuy] for loguy <= i <= hi,
              A[i] <= A[higuy] for lo <= i < higuy
              A[i] = A[lo] for higuy <= i < loguy    */

        /* We've finished the partition, now we want to sort the subarrays
           [lo, higuy-1] and [loguy, hi].
           We do the smaller one first to minimize stack usage.
           We only sort arrays of length 2 or more.*/

        if ( higuy - 1 - lo >= hi - loguy ) {
            if (lo + 1 < higuy) {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy - 1;
				mm[stkptr] = lomm+minmatch;
                ++stkptr;
            }                           /* save big recursion for later */

            if (loguy < hi) {
                lo = loguy;
				minmatch += himm;
                goto recurse;           /* do small recursion */
            }
        }
        else {
            if (loguy < hi) {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
				mm[stkptr] = himm+minmatch;
                ++stkptr;               /* save big recursion for later */
            }

            if (lo + 1 < higuy) {
                hi = higuy - 1;
				minmatch += lomm;
                goto recurse;           /* do small recursion */
            }
        }
    }

    /* We have sorted the array, except for any pending sorts on the stack.
       Check if there are any, and do them. */

    --stkptr;
    if (stkptr >= 0) {
        lo = lostk[stkptr];
        hi = histk[stkptr];
		minmatch = mm[stkptr];
        goto recurse;           /* pop subarray from stack */
    }
    else
        return;                 /* all subarrays done */
}


/***
*shortsort(hi, lo, width, comp) - insertion sort for sorting short arrays
*
*Purpose:
*       sorts the sub-array of elements between lo and hi (inclusive)
*       side effects:  sorts in place
*       assumes that lo < hi
*
*Entry:
*       uint4 *lo = pointer to low element to sort
*       uint4 *hi = pointer to high element to sort
*       unsigned width = width in bytes of each array element
*       int (*comp)() = pointer to function returning analog of strcmp for
*               strings, but supplied by user for comparing the array elements.
*               it accepts 2 pointers to elements and returns neg if 1<2, 0 if
*               1=2, pos if 1>2.
*
*Exit:
*       returns void
*
*Exceptions:
*
*******************************************************************************/

static void shortsort ( uint4 *lo, uint4 *hi, unsigned char *data, uint4 minmatch )
{
    uint4 *p, *max, ml;

    /* Note: in assertions below, i and j are alway inside original bound of
       array to sort. */

    while (hi > lo) {
        /* A[i] <= A[j] for i <= j, j > hi */
        max = lo;
        for (p = lo+1; p <= hi; p++) {
            /* A[i] <= A[max] for lo <= i < p */
            if (qscompare(*p-minmatch, *max-minmatch, data, &ml) > 0) {
                max = p;
            }
            /* A[i] <= A[max] for lo <= i <= p */
        }

        /* A[i] <= A[max] for lo <= i <= hi */

        szip_swap(max, hi)

        /* A[i] <= A[hi] for i <= hi, so A[i] <= A[j] for i <= j, j >= hi */

        hi--;

        /* A[i] <= A[j] for i <= j, j > hi, loop top condition established */
    }
    /* A[i] <= A[j] for i <= j, j > lo, which implies A[i] <= A[j] for i < j,
       so array is sorted */
}

#ifdef __cplusplus
}
#endif // __cplusplus
// =================================================================================================
// rangecod.c
// -------------------------------------------------------------------------------------------------

/*
  define NOWARN if you do not expect more than 2^32 outstanding bytes 
  since I recommend restarting the coder in intervals of less than    
  2^23 symbols for error tolerance this is not expected
*/
#define NOWARN

/*
  define EXTRAFAST for increased speed; you loose compression and
  compatibility in exchange.
*/
#define EXTRAFAST

// #include <stdio.h>		/* fprintf(), get_byte(), putchar(), NULL */
// #include "port.h"
// #include "rangecod.h"
// #include "sz_stream.h"

/* SIZE OF RANGE ENCODING CODE VALUES. */

#define CODE_BITS 32
#define Top_value ((code_value)1 << (CODE_BITS-1))
#define SHIFT_BITS (CODE_BITS - 9)
#define EXTRA_BITS ((CODE_BITS-2) % 8 + 1)
#define Bottom_value (Top_value >> 8)

char coderversion[]="rangecode 1.1c NOWARN (c) 1997-1999 Michael Schindler";

#define EOF (-1)

/* Function to get the next byte from the source buffer */
static inline int get_byte(szip_stream *stream) {
    return (stream->sourcePos < stream->sourceSize)
           ? stream->sourceBuffer[stream->sourcePos++]
           : EOF;
}

/* Function to read a 3-byte unsigned integer from the source buffer */
static inline uint32_t read_uint3(szip_stream *stream) {
    uint32_t x = get_byte(stream);
    x = (x << 8) | get_byte(stream);
    x = (x << 8) | get_byte(stream);
    return x;
}

/* Start the decoder                                         */
/* rc is the range coder to be used                          */
/* returns the char from start_encoding or EOF               */
int start_decoding(rangecoder *rc, szip_stream *stream) {
    int c = get_byte(stream);
    if (c == EOF)
        return EOF;
    rc->buffer = get_byte(stream);
    rc->low = rc->buffer >> (8 - EXTRA_BITS);
    rc->range = ((code_value)1) << EXTRA_BITS;
    return c;
}

/* Normalize decoder state */
static inline void dec_normalize(rangecoder *rc, szip_stream *stream) {
    while (rc->range <= Bottom_value) {
        rc->low = (rc->low << 8) | ((rc->buffer << EXTRA_BITS) & 0xff);
        rc->buffer = get_byte(stream);
        rc->low |= rc->buffer >> (8 - EXTRA_BITS);
        rc->range <<= 8;
    }
}

/* Calculate cumulative frequency for next symbol. Does NO update! */
/* rc is the range coder to be used                                */
/* tot_f is the total frequency                                    */
/* or: totf is (code_value)1<<shift                                */
/* returns the cumulative frequency                                */
freq decode_culfreq(rangecoder *rc, freq tot_f, szip_stream *stream) {
    dec_normalize(rc, stream);
    rc->help = rc->range / tot_f;
    return rc->low / rc->help;
}

/* Calculate cumulative frequency with a shift optimization */
freq decode_culshift(rangecoder *rc, freq shift, szip_stream *stream) {
    dec_normalize(rc, stream);
    rc->help = rc->range >> shift;
    return rc->low / rc->help;
}

/* Update decoding state                                     */
/* rc is the range coder to be used                          */
/* sy_f is the interval length (frequency of the symbol)     */
/* lt_f is the lower end (frequency sum of < symbols)        */
/* tot_f is the total interval length (total frequency sum)  */
void inline decode_update(rangecoder *rc, freq sy_f, freq lt_f, freq tot_f) {
    code_value tmp = rc->help * lt_f;
    rc->low -= tmp;
    rc->range = rc->help * sy_f;
}

/* Decode a byte/short without modelling                     */
/* rc is the range coder to be used                          */
unsigned char decode_byte(rangecoder *rc, szip_stream *stream) {
    unsigned char tmp = decode_culshift(rc, 8, stream);
    decode_update(rc, 1, tmp, (freq)1 << 8);
    return tmp;
}

unsigned short decode_short(rangecoder *rc, szip_stream *stream) {
    unsigned short tmp = decode_culshift(rc, 16, stream);
    decode_update(rc, 1, tmp, (freq)1 << 16);
    return tmp;
}


/* Finish decoding                                           */
/* rc is the range coder to be used                          */
void done_decoding(rangecoder *rc, szip_stream *stream) {
    dec_normalize(rc, stream);  /* normalize to use up all bytes */
}

// =================================================================================================
// reorder.c
// -------------------------------------------------------------------------------------------------
// #include "port.h"

void reorder(unsigned char *in, unsigned char *out, uint4 length, uint recordsize)
{	uint4 i,j;
	for (i=0; i<recordsize; i++)
		for(j=i; j<length; j+=recordsize)
			*(out++) = in[j];
}

void unreorder(unsigned char *in, unsigned char *out, uint4 length, uint recordsize)
{	uint4 i,j;
	for (i=0; i<recordsize; i++)
		for(j=i; j<length; j+=recordsize)
			out[j] = *(in++);
}
// =================================================================================================
// sz_mod4.c
// -------------------------------------------------------------------------------------------------

// #include <assert.h>
// #include <stdio.h>
// #include <stdlib.h>   /* exit() */
// #include "sz_mod4.h"

#define RLSHIFT 10
#define MTFSHIFT 10

#define FULLFLAG (m->cache - 1)
#define MTFFLAG (m->cache - 2)

/* add a new symbol to MTF list */
static void addtomtf(sz_model *m, uint sym)
{   
    uint i = (m->mtffirst + 1) & (MTFHISTSIZE - 1);
    
    if (m->mtfhist[i].next == 0xffff) {     /* an empty place */
        m->mtfsize++;
        m->mtfsizeact++;
    } 
    else if (m->mtfsizeact == m->mtfsize) { /* occupied by active symbol */
        m->lastseen[m->mtfhist[i].sym] = m->cache - 1; /* FULLFLAG */
        bitreactivate(&(m->full), m->mtfhist[i].sym);
    } 
    else {                                  /* occupied by inactive symbol */
        m->mtfsizeact++;
    }

    m->mtfhist[i].next = m->mtffirst;
    m->mtfhist[i].sym = sym;
    m->mtffirst = i;
    m->lastseen[sym] = m->cache - 2; /* MTFFLAG */
}

/* finish updating the model */
static void finishupdate(sz_model *m, uint symbol)
{   
    cacheptr tmp;
    tmp = m->newest; /* make tmp point to new element */
    m->lastseen[symbol] = tmp;
    tmp->symbol = symbol;
    m->cachetotf += tmp->weight;
    
    tmp = tmp->next; /* make tmp point to the element to be cleared */
    m->whatmod[tmp->what]--;
    
    if (!tmp->sy_f) {
        (m->lastseen[tmp->symbol])->sy_f--;
    } 
    else { /* last instance, move to MTF */
        addtomtf(m, tmp->symbol);
    }

    tmp = m->lastnew; /* make tmp point to adjustment place */
    m->whatmod[tmp->what] -= 5;
    m->cachetotf -= tmp->weight;
    m->lastseen[tmp->symbol]->sy_f -= tmp->weight - 1;
    tmp->weight = 1;
    m->lastnew = tmp->next;
}

static unsigned char readrun(qsmodel *rlmod, sz_model *m, uint4 *n, szip_stream *stream) {   
    int sy_f, lt_f, rl;
    rl = qsgetsym(rlmod, decode_culshift(&(m->ac), RLSHIFT, stream));
    qsgetfreq(rlmod, rl, &sy_f, &lt_f);
    decode_update_shift(&(m->ac), sy_f, lt_f, RLSHIFT);
    qsupdate(rlmod, rl);
    
    if (rl <= 3) {   /* no extra bits */
        rl++;
        *n = rl;
        return (1 + (rl >> 1));
    }

    if (rl == 4) {  /* two extra bits */
        rl = decode_culshift(&(m->ac), 2, stream);
        decode_update_shift(&(m->ac), 1, rl, 2);
        *n = rl + 5;
        return 3;
    }

    if (rl == 5) {  /* three extra bits */
        rl = decode_culshift(&(m->ac), 3, stream);
        decode_update_shift(&(m->ac), 1, rl, 3);
        *n = rl + 9;
        return 4;
    }

    /* five extra bits */
    rl = decode_culshift(&(m->ac), 5, stream);
    decode_update_shift(&(m->ac), 1, rl, 5);

    if (rl > 16)
        *n = rl;
    else {   
        uint4 bits;
        rl += 5;
        bits = decode_culshift(&(m->ac), rl, stream);
        decode_update_shift(&(m->ac), 1, bits, rl);
        *n = bits + ((uint4)1 << rl);
    }

    return 4;
}

static int activatenext(sz_model *m, uint *next) {
    while (m->mtfsize > m->mtfsizeact) {
        mtfentry *tmp = m->mtfhist + *next;
        if (m->lastseen[tmp->sym] == FULLFLAG) {
            bitdeactivate(&(m->full), tmp->sym);
            m->lastseen[tmp->sym] = MTFFLAG;
            m->mtfsizeact++;
            return 1;
        }
        *next = tmp->next;
        tmp->next = 0xffff;
        m->mtfsize--;
    }
    return 0;
}

void sz_decode(sz_model *m, uint *symbol, uint4 *runlength, szip_stream *stream) {
    uint sym;

    /* First decode which model was used in encoding */
    sym = decode_culshift(&(m->ac), 6, stream);

    if (sym < m->whatmod[0]) {  /* Cache */
        uint lt_f, tot_f;
        cacheptr tmp;
        decode_update_shift(&(m->ac), m->whatmod[0], 0, 6);
        m->whatmod[0] += 6;

        tmp = m->newest;
        tot_f = m->cachetotf - tmp->sy_f;
        sym = decode_culfreq(&(m->ac), tot_f, stream);
        tmp = tmp->prev;
        lt_f = tmp->sy_f;

        while (lt_f <= sym) {
            tmp = tmp->prev;
            lt_f += tmp->sy_f;
        }

        decode_update(&(m->ac), tmp->sy_f, lt_f - tmp->sy_f, tot_f);

        cacheptr free = m->newest->next;
        m->newest = free;
        free->what = 0;
        free->weight = readrun(&(m->rlemod[tmp->weight]), m, runlength, stream);
        free->sy_f = free->weight + tmp->sy_f;

        tmp->sy_f = 0;
        *symbol = tmp->symbol;
    }
    else if (sym < m->whatmod[0] + m->whatmod[1]) {  /* MTF */
        mtfentry *pred;
        int sy_f, lt_f;

        decode_update_shift(&(m->ac), m->whatmod[1], m->whatmod[0], 6);
        m->whatmod[1] += 6;

        sym = qsgetsym(&(m->mtfmod), decode_culshift(&(m->ac), MTFSHIFT, stream));
        qsgetfreq(&(m->mtfmod), sym, &sy_f, &lt_f);
        decode_update_shift(&(m->ac), sy_f, lt_f, MTFSHIFT);
        qsupdate(&(m->mtfmod), sym);

        if (m->mtfsizeact == 0) {
            activatenext(m, &(m->mtffirst));
        }

        pred = m->mtfhist + m->mtffirst;
        if (sym == 0) {  /* First entry */
            if (m->mtfsizeact == 0)
            activatenext(m, &(m->mtffirst));
            pred = m->mtfhist + m->mtffirst;
            sym = pred->sym;
            m->mtffirst = pred->next;
            pred->next = 0xffff;
        }
        else {
            uint n;
            mtfentry *target;
            if (sym < m->mtfsizeact) { /* Active list is large enough */
                for (n = sym - 1; n; n--)
                    pred = m->mtfhist + pred->next;
            }
            else {
                for (n = m->mtfsizeact - 1; n; n--)
                    pred = m->mtfhist + pred->next;
                while (m->mtfsizeact < sym) {
                    activatenext(m, &(pred->next));
                    pred = m->mtfhist + pred->next;
                }
                activatenext(m, &(pred->next));
            }

            target = m->mtfhist + pred->next;
            sym = target->sym;
            pred->next = target->next;
            target->next = 0xffff;
        }

        m->mtfsizeact--;
        m->mtfsize--;

        cacheptr free = m->newest->next;
        m->newest = free;
        free->what = 1;
        free->weight = readrun(m->rlemod, m, runlength, stream);
        free->sy_f = free->weight;

        *symbol = sym;
    }
    else {  /* Full model */
        int sy_f, lt_f;

        decode_update_shift(&(m->ac), m->whatmod[2], m->whatmod[0] + m->whatmod[1], 6);
        m->whatmod[2] += 6;

        /* Adjust the size of the MTF */
        if (m->mtfsizeact > MTFSIZE) {  /* Active MTF too big */
            uint n, i;
            i = m->mtffirst;
            for (n = 0; n < MTFSIZE; n++)
                i = m->mtfhist[i].next;

            while (n < m->mtfsizeact) {
                bitreactivate(&(m->full), m->mtfhist[i].sym);
                m->lastseen[m->mtfhist[i].sym] = FULLFLAG;
                i = m->mtfhist[i].next;
                n++;
            }
            m->mtfsizeact = MTFSIZE;
        }
        else if (m->mtfsizeact < MTFSIZE) {  /* Active MTF too small */
            uint n;
            mtfentry *pred;

            if (m->mtfsizeact == 0) {
                activatenext(m, &(m->mtffirst));
                pred = m->mtfhist + m->mtffirst;
            }
            else {
                pred = m->mtfhist + m->mtffirst;
                for (n = m->mtfsizeact - 1; n; n--)
                    pred = m->mtfhist + pred->next;
            }

            while (m->mtfsizeact < MTFSIZE && activatenext(m, &(pred->next)))
                pred = m->mtfhist + pred->next;
        }

        sym = bitgetsym(&(m->full), decode_culfreq(&(m->ac), bittotf(&(m->full)), stream));
        bitgetfreq(&(m->full), sym, &sy_f, &lt_f);
        decode_update(&(m->ac), sy_f, lt_f, bittotf(&(m->full)));
        bitupdate_ex(&(m->full), sym);

        cacheptr free = m->newest->next;
        m->newest = free;
        free->what = 2;
        free->weight = readrun(m->rlemod, m, runlength, stream);
        free->sy_f = free->weight;

        *symbol = sym;
    }

    finishupdate(m, *symbol);
}



/* initialization of the model */
/* headersize -1 means decompression */
/* first is the first byte written by the arithcoder */
void initmodel(sz_model *m, int headersize, unsigned char *first, szip_stream *stream) {   
    int i;

    /* init the arithcoder using the external stream */
    *first = start_decoding(&(m->ac), stream);

    /* init the full model */
    initbitmodel(&(m->full), ALPHABETSIZE, 40 * ALPHABETSIZE, 10 * ALPHABETSIZE, NULL);
    for (i = 0; i < ALPHABETSIZE; i++)
        m->lastseen[i] = FULLFLAG;

    /* init the cache with symbols CACHESIZE-1 to 0 */
    {   
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
    }
    
    m->newest = m->cache + (CACHESIZE - 2);
    m->lastnew = m->cache + (CACHESIZE - 7);
    m->cachetotf = CACHESIZE; // for startup only, decremented by 1 later

    /* initialize the whatmodel */
    m->whatmod[0] = 41; // 1 + 22*1 + 3*6
    m->whatmod[1] = 8;  // 1 + 1*1 + 1*6
    m->whatmod[2] = 15; // 1 + 2*1 + 2*6

    /* make 2 old and 2 new full hits for what */
    for (i = 0; i < 2; i++) {   
        m->cache[i].what = 2;
        m->lastnew[i].what = 2;
    }

    /* make 1 old and 1 new hit for MTF */
    m->cache[2].what = 1;
    m->lastnew[2].what = 1;

    /* init the mtf models with symbols CACHESIZE .. (CACHESIZE+MTFSIZE<<1) */
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

    /* init the runlength models */
    for (i = 0; i < 5; i++)
        initqsmodel(m->rlemod + i, 7, RLSHIFT, 150, NULL, m->compress);
}

/* call fixafterfirst after encoding/decoding the first run */
void fixafterfirst(sz_model *m) {   
    m->cachetotf--;
}


/* deletion of the model */
void deletemodel(sz_model *m, szip_stream *stream) {   
    int i;

    done_decoding(&(m->ac), stream);

    // fprintf(stderr,"%d %d %d ", m->ac.bytecount, MAXCACHESIZE, MTFSIZE);
    // for(i = 0; i < MTFSIZE; i++) fprintf(stderr,"%d ", modelused[i]);

    /* delete the full model */
    deletebitmodel(&(m->full));

    /* delete the mtf model */
    deleteqsmodel(&(m->mtfmod));

    /* delete the runlength models */
    for (i = 0; i < 5; i++)
        deleteqsmodel(m->rlemod + i);
}

// =================================================================================================
// sz_srt.c
// -------------------------------------------------------------------------------------------------
//#define CHECKINDIRECT

// #include <string.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include "port.h"
// #include "sz_err.h"
// #include "sz_srt.h"

// #if defined SZ_UNSRT_O4
// #include "sz_hash2.h"		// only used in sz_unsrt_o4
// #endif // SZ_UNSRT_O4

// the sorting is a little slow due to attempts to reuse memory as soon as possible.
// since the n-1 order sorted block is read sequentially a block can be freed (inserted
// in a freelist) as soon as it is processed. Since the new n-order sorted pointers
// grow as 256 different lists there is no need to have all memory available at once;
// new memory is needed at the same speed old is freed.

#define BITSSAMEBLOCK 10
#define BLOCKSIZE (1<<BITSSAMEBLOCK)
#define BLOCKMASK (BLOCKSIZE-1)

typedef struct p_block ptrblock;

struct p_block{
	uint2 msbytes[BLOCKSIZE];
	unsigned char lsbyte[BLOCKSIZE];
	ptrblock *nextfree;
};


typedef struct {
	ptrblock **index;		// index to blocks used in current sort
	ptrblock **oldindex;	// spare mem for alternate index
	ptrblock *freelist;
	ptrblock *block;
	ptrblock *spare[18];
	uint4 nrblocks;
} ptrstruct;

static void allocptrs(uint4 length, ptrstruct *p) {
    uint4 i;
    p->nrblocks = (length + BLOCKSIZE - 1) / BLOCKSIZE;

    p->index = (ptrblock**) malloc(sizeof(ptrblock*) * p->nrblocks);
    if (!p->index) sz_error(SZ_NOMEM_SORT);

    p->oldindex = (ptrblock**) malloc(sizeof(ptrblock*) * p->nrblocks);
    if (!p->oldindex) sz_error(SZ_NOMEM_SORT);

    p->block = (ptrblock*) malloc(sizeof(ptrblock) * p->nrblocks);
    if (!p->block) sz_error(SZ_NOMEM_SORT);

    p->freelist = NULL;
    for (i = 0; i < 18; i++)
        p->spare[i] = NULL;
    for (i = 0; i < p->nrblocks; i++)
        p->index[i] = p->block + i;
}

static void extraspare(ptrstruct *p, int blocks)
{	int i;
	for (i=0; p->spare[i]!= NULL; i++)
		/* void */;
	p->spare[i] = (ptrblock*) malloc(sizeof(ptrblock)*blocks);
	if (p->spare[i] == NULL)
		sz_error(SZ_NOMEM_SORT);
	p->spare[i]->nextfree = p->freelist;
	p->freelist = p->spare[i];
	for(i=1; i<blocks; i++)
		p->freelist[i-1].nextfree = p->freelist + i;
	p->freelist[blocks-1].nextfree = NULL;
}

static void allocspareptrs(uint4 length, ptrstruct *p)
{	length = (length>>BITSSAMEBLOCK) + 1;
	if (length>256) length = 256;
	extraspare(p,length);
}

static void freeptrs(ptrstruct *p)
{	int i;
	free(p->index);
	free(p->oldindex);
	free(p->block);
	for (i=0; p->spare[i] != NULL; i++)
		free(p->spare[i]);
}

static Inline void setptr(ptrstruct *p, uint4 i, uint4 ptr)
{	ptrblock *tmp;
	tmp = p->index[i>>BITSSAMEBLOCK];
	if (tmp==NULL)
	{	if (p->freelist == NULL)
			extraspare(p,16);
		tmp = p->index[i>>BITSSAMEBLOCK] = p->freelist;
		p->freelist = p->freelist->nextfree;
	}
	i &= BLOCKMASK;
	tmp->msbytes[i] = ptr>>8;
	tmp->lsbyte[i] = ptr & 0xff;
}

static void sortorder2(ptrstruct *p, unsigned char *in, uint4 length,
					   uint4 *counts, unsigned int offset, uint4 *indexlast)
{	uint4 i, *o2counts, sum;
	unsigned int context;
	memset(counts, 0, 256*sizeof(uint4));
	o2counts = (uint4*) calloc(0x10000, sizeof(uint4));
	if (o2counts == NULL)
		sz_error(SZ_NOMEM_SORT);
	context = (unsigned)in[length-1]<<8;
	for(i=0; i<length; i++)
	{	context = context>>8 | (unsigned)(in[i])<<8;
		counts[in[i]]++;
		o2counts[context]++;
	}
	sum = length;
	for (i=0x10000; i--; )
	{	sum -= o2counts[i];
		o2counts[i] = sum;
	}
	sum = length;
	for (i=0x100; i--; )
	{	sum -= counts[i];
		counts[i] = sum;
	}
	context = (unsigned)in[length-offset]<<8 | in[length-offset-1];
	if (context == 0xffff)
		*indexlast = length-1;
	else
		*indexlast = o2counts[context+1]-1;
	offset--;
	for(i=0; i<offset; i++)
	{	in[i+length] = in[i];
		context = context>>8 | (unsigned int)(in[i+length-offset])<<8;
		setptr(p,o2counts[context],i+length);
		o2counts[context]++;
	}
	for(i=offset; i<length; i++)
	{	context = context>>8 | (unsigned int)(in[i-offset])<<8;
		setptr(p,o2counts[context],i);
		o2counts[context]++;
	}
	free(o2counts);
}

static void incsortorder(ptrstruct *p, unsigned char *in, uint4 length,
						 uint4 *counts, int offset, uint4 *indexlast)
{	uint4 i, block, ct[256];
	ptrblock *curblock;
	unsigned char ch=0;
	{ptrblock **swap_ptr=p->index; p->index = p->oldindex; p->oldindex = swap_ptr;}
	memset(p->index,0,p->nrblocks*sizeof(ptrblock*));
	memcpy(ct,counts,256*sizeof(uint4));
	block = 0;
	curblock = p->oldindex[block];
	for (i=0; i<=*indexlast; i++)
	{	unsigned index = i & BLOCKMASK;
		uint4 tmp = (uint4)(curblock->msbytes[index])<<8 | curblock->lsbyte[index];
		ch = in[tmp-offset];
		setptr(p,ct[ch],tmp);
		ct[ch]++;
		if (index==BLOCKMASK && block!=p->nrblocks-1)		//last ptr in block
		{	curblock->nextfree = p->freelist;
			p->freelist = curblock;
			block++;
			curblock = p->oldindex[block];
		}
	}
	*indexlast = ct[ch]-1;
	for ( ; i<length; i++)
	{	unsigned index = i & BLOCKMASK;
		uint4 tmp = (uint4)(curblock->msbytes[index])<<8 | curblock->lsbyte[index];
		ch = in[tmp-offset];
		setptr(p,ct[ch],tmp);
		ct[ch]++;
		if (index==BLOCKMASK && block<p->nrblocks-1)		//last ptr in block
		{	curblock->nextfree = p->freelist;
			p->freelist = curblock;
			block++;
			curblock = p->oldindex[block];
		}
	}
	curblock->nextfree = p->freelist;
	p->freelist = curblock;
}


static void finishsort(ptrstruct *p, unsigned char *in, uint4 length,
						 uint4 *counts, uint4 *indexlast)
{	uint4 i, block, ct[256];
	ptrblock *curblock;
	unsigned char ch=0;
	{ptrblock **swap_ptr=p->index; p->index = p->oldindex; p->oldindex = swap_ptr;}
	memset(p->index,0,p->nrblocks*sizeof(ptrblock*));
	memcpy(ct,counts,256*sizeof(uint4));
	block = 0;
	curblock = p->oldindex[block];
	for (i=0; i<=*indexlast; i++)
	{	unsigned index = i & BLOCKMASK;
		uint4 tmp = (uint4)(curblock->msbytes[index])<<8 | curblock->lsbyte[index];
		ch = in[tmp-1];
		setptr(p,ct[ch],in[tmp]);
		ct[ch]++;
		if (index==BLOCKMASK && block!=p->nrblocks-1)		//last ptr in block
		{	curblock->nextfree = p->freelist;
			p->freelist = curblock;
			block++;
			curblock = p->oldindex[block];
		}
	}
	*indexlast = ct[ch]-1;
	for ( ; i<length; i++)
	{	unsigned index = i & BLOCKMASK;
		uint4 tmp = (uint4)(curblock->msbytes[index])<<8 | curblock->lsbyte[index];
		ch = in[tmp-1];
		setptr(p,ct[ch],in[tmp]);
		ct[ch]++;
		if (index==BLOCKMASK && block!=p->nrblocks-1)		//last ptr in block
		{	curblock->nextfree = p->freelist;
			p->freelist = curblock;
			block++;
			curblock = p->oldindex[block];
		}
	}
	curblock->nextfree = p->freelist;
	p->freelist = curblock;
	for (i=0; i<p->nrblocks-1; i++)
		memcpy(in+i*BLOCKSIZE, p->index[i]->lsbyte, BLOCKSIZE);
	i= p->nrblocks - 1;
	memcpy(in+BLOCKSIZE*i, p->index[i]->lsbyte, length-i*BLOCKSIZE);
}


// inout: bytes to be sorted; sorted bytes on return. must be length+order bytes long
// length: number of bytes in inout
// *indexlast: returns position of last context (needed for unsort)
// order: order of context used in sorting (must be >=3)
// the code assumes length>=order
// and inout is length+order bytes long (only the first length need to be filled)
void sz_srt(unsigned char *inout, uint4 length, uint4 *indexlast, unsigned int order)
{	uint4 i;
	ptrstruct p;
	uint4 counts[256];
	allocptrs(length, &p);
	sortorder2(&p, inout, length, counts, order, indexlast);
	allocspareptrs(length, &p);
	for (i=order-2; i>1; i--)
		incsortorder(&p, inout, length, counts, i, indexlast);
	finishsort(&p, inout, length, counts, indexlast);
	freeptrs(&p);
}


#define INDIRECT 0x800000

static inline void setbit(unsigned char *flags, uint32_t bit) {
    flags[bit >> 3] |= (1 << (bit & 7));
}
static inline int getbit(const unsigned char *flags, uint32_t bit) {
    return (flags[bit >> 3] >> (bit & 7)) & 1;
}

static void makeorder2(unsigned char *flags, unsigned char *in, uint4 *counts, uint4 length) {
    uint32_t i, j, ct[256];
    memcpy(ct, counts, 256 * sizeof(uint32_t));
    for (i = 0; i < 256; i++) {
        setbit(flags, ct[i]);
    }
    j = 0;
    for (i = 0; i < 255; i++) {
        uint32_t k;
        for (k = counts[i + 1]; j < k; j++) {
            ct[in[j]]++;
        }
        for (k = 0; k < 256; k++) {
            setbit(flags, ct[k]);
        }
    }
}

static void increaseorder(unsigned char *inflags, unsigned char *outflags, unsigned char *in, uint4 *counts, uint4 length) {
    uint32_t *lastseen = (uint32_t *)malloc(256 * sizeof(uint32_t));
    uint32_t *ct       = (uint32_t *)malloc(256 * sizeof(uint32_t));
    if (!lastseen || !ct) {
        free(lastseen);
        free(ct);
        sz_error(SZ_NOMEM_SORT);
    }

    // Copy counts so we can increment while scanning
    memcpy(ct, counts, 256 * sizeof(uint32_t));

    // We'll track "contextstart" each time we see an inflags bit set
    uint32_t contextstart = 0;
    memset(lastseen, 0xFF, 256 * sizeof(uint32_t));  // 0xFFFFFFFF => not yet seen

    for (uint32_t i = 0; i < length; i++) {
        if (getbit(inflags, i)) {
            contextstart = i;  // new context boundary
        }
        uint32_t ch = in[i];

        // If ch not “seen” in this context, set the bit for ct[ch]
        if (lastseen[ch] != contextstart) {
            lastseen[ch] = contextstart;
            setbit(outflags, ct[ch]);
        }
        ct[ch]++;
    }

    free(ct);
    free(lastseen);
}

// Constructs the permutation table used for unsorting
static void maketable(unsigned char *inflags, uint4 *table, unsigned char *in, uint4 *counts, uint4 length) {
    uint32_t *ct = (uint32_t *)malloc(256 * sizeof(uint32_t));
    uint32_t *firstseen = (uint32_t *)malloc(256 * sizeof(uint32_t));
    if (!ct || !firstseen) {
        free(ct);
        free(firstseen);
        sz_error(SZ_NOMEM_SORT);
    }

    // Copy the counts
    memcpy(ct, counts, 256 * sizeof(uint32_t));
    memset(firstseen, 0, 256 * sizeof(uint32_t));

    uint32_t contextstart = 0;
    for (uint32_t i = 0; i < length; i++) {
        if (getbit(inflags, i)) {
            contextstart = i;  // new context boundary
        }
        uint32_t ch = in[i];

        // If not seen in this context, store table[i] = ct[ch]. 
        // Else table[i] = link to the older occurrence with “INDIRECT” bit set.
        if (firstseen[ch] <= contextstart) {
            // “first time we see ch” in this context
            table[i] = ct[ch];
            firstseen[ch] = i + 1;  // store “1 + i” so we know i was the last
        } else {
            table[i] = (firstseen[ch] - 1) | 0x80000000; // set high bit as “INDIRECT”
        }
        ct[ch]++;
    }

    free(firstseen);
    free(ct);
}

/*
 * in: bytes to be unsorted
 * out: unsorted bytes; if out==NULL output is written to stdout
 * length: number of bytes in in (and out)
 * indexlast: position of last context (as returned by sorttrans)
 * counts: number of occurrences of each byte in in (if NULL it will be calculated)
 * order: order of context used in sorting (must be >=3)
 * the code assumes length>=order
 */
void sz_unsrt(unsigned char *in, unsigned char *out, uint4 length, uint4 indexlast, uint4 *counts, unsigned int order) {

    // If counts == NULL, we must build them ourselves
    unsigned char nocounts = (counts == NULL);
    if (nocounts) {
        counts = (uint32_t *)calloc(256, sizeof(uint32_t));
        if (!counts) {
            sz_error(SZ_NOMEM_SORT);
        }
        for (uint32_t i = 0; i < length; i++) {
            counts[in[i]]++;
        }
    }

    // Convert counts[] so that counts[i] = number of symbols < i
    {
        uint32_t sum = length;
        for (int i = 255; i >= 0; i--) {
            sum -= counts[i];
            counts[i] = sum;  // prefix sums
        }
    }

    // Allocate local flags and table arrays
    unsigned char *flags1 = (unsigned char *)calloc((length + 8) >> 3, 1);
    unsigned char *flags2 = (unsigned char *)calloc((length + 8) >> 3, 1);
    uint32_t      *table  = (uint32_t *)malloc((length + 1) * sizeof(uint32_t));
    if (!flags1 || !flags2 || !table) {
        free(flags1); free(flags2); free(table);
        if (nocounts) free(counts);
        sz_error(SZ_NOMEM_SORT);
    }

    // Build order-2 flags
    makeorder2(flags1, in, counts, length);

    // If we need higher orders, repeatedly refine from flags1 -> flags2 or vice versa
    for (unsigned int level = 2; level < order - 1; level++) {
        // produce flags2 from flags1
        memset(flags2, 0, (length + 8) >> 3);
        increaseorder(flags1, flags2, in, counts, length);

        // swap them
        unsigned char *tmp = flags1;
        flags1 = flags2;
        flags2 = tmp;
    }

    // Build the forward “table” from the final flags array
    maketable(flags1, table, in, counts, length);
    table[length] = 0x80000000;  // sentinel with “INDIRECT” bit set

    // Now do the actual unsorting by walking the permutation table
    {
        uint32_t j = indexlast;
        for (uint32_t i = 0; i < length; i++) {
            // next = table[j]
            uint32_t next = table[j];
            if (next & 0x80000000) {
                // if INDRECT is set, link to old occurrence
                uint32_t realPos = (next & ~0x80000000);
                j = table[realPos]++;
            } else {
                // direct pointer
                table[j]++;
                j = next;
            }
            out[i] = in[j];
        }
        if (j != indexlast) {
            sz_error(SZ_NOTCYCLIC);
        }
    }

    // Clean up
    free(table);
    free(flags2);
    free(flags1);

    if (nocounts) {
        free(counts);
    }
}


#if defined SZ_SRT_O4
// a fast alternate sort, only for order 4. inout only length bytes is OK here.
void sz_srt_o4(unsigned char *inout, uint4 length, uint4 *indexlast)
{
    // Allocate local buffers.
    uint4 *counters = (uint4*)calloc(0x10000, sizeof(uint4));
    if (counters == NULL)
        sz_error(SZ_NOMEM_SORT);

    uint2 *context = (uint2*)malloc(length * sizeof(uint2));
    if (context == NULL) {
        free(counters);
        sz_error(SZ_NOMEM_SORT);
    }

    unsigned char *symbols = (unsigned char*)malloc(length * sizeof(unsigned char));
    if (symbols == NULL) {
        free(counters);
        free(context);
        sz_error(SZ_NOMEM_SORT);
    }

    register uint4 i;

    // Count contexts.
    memset(counters, 0, 0x10000 * sizeof(uint4));
    i = ((uint4)inout[length - 1]) << 8;
    {
        register unsigned char *tmp;
        for (tmp = inout; tmp < inout + length; tmp++) {
            i = (i >> 8) | (((uint4)(*tmp)) << 8);
            counters[i]++;
        }
    }

    // Add context counts.
    {
        register uint4 sum = length;
        for (i = 0x10000; i--; ) {
            sum -= counters[i];
            counters[i] = sum;
        }
    }

    // First sort pass.
    {
        register unsigned char *tmp;
        register uint4 ctx = (((uint4)inout[length - 4]) << 8) | inout[length - 5];
        if (ctx == 0xffff)
            *indexlast = length - 1;
        else
            *indexlast = counters[ctx + 1] - 1;
        ctx = ((((uint4)inout[length - 1] << 8) | inout[length - 2]) << 8 | inout[length - 3]) << 8 | inout[length - 4];
        for (tmp = inout; tmp < inout + length; tmp++) {
            register uint4 x = counters[ctx & 0xffff]++;
            context[x] = ctx >> 16;
            ctx = (ctx >> 8) | (((uint4)(symbols[x] = *tmp)) << 24);
        }
    }

    /* Second sort pass */
    {
        uint4 lastpos = *indexlast;
        for (i = length; i > lastpos; ) { // lastpos is the last processed in this loop
            i -= 1;
            inout[--counters[context[i]]] = symbols[i];
        }
    }
    *indexlast = counters[context[i]];
    while (i--)
        inout[--counters[context[i]]] = symbols[i];

    // Free the allocated buffers.
    free(counters);
    free(context);
    free(symbols);
}
#endif // SZ_SRT_O4


#ifdef SZ_UNSRT_O4
// an alternate backtransform for order 4 using hash tables
void sz_unsrt_o4(unsigned char *in, unsigned char *out, uint4 length, uint4 indexlast,
			   uint4 *counts)
{	uint4 i, *contexts2, *contexts4, initcontext;
	uint2 *lastseen;
	unsigned char *loop, *endloop, nocounts;
	h2table htable;

	// get counts if not supplied
	nocounts = counts==NULL;
	if (nocounts)
	{	counts = (uint4*) calloc(256, sizeof(uint4));
		for (i=0; i<length; i++)
			counts[in[i]]++;
	}
	
	// allocate tables (could do on stack, but will cause problems with some compilers
	contexts2 = (uint4*)calloc(0x10000, sizeof(uint4));
	if (contexts2 == NULL)
		sz_error(SZ_NOMEM_SORT);

	// count contexts
	loop = in;
	for (i=0; i<0x100; i++)
		for (endloop = loop+counts[i]; loop<endloop; loop++)
			contexts2[(unsigned int)(*loop)<<8 | i]++;
		
	// put sum of order2 contexts in order 4 contexts
	contexts4 = (uint4*)malloc(0x10000*sizeof(uint4));
	if (contexts4 == NULL)
		sz_error(SZ_NOMEM_SORT);
  {	uint4 sum = length;
	for (i=0x10000; i--;)
	{	if (sum > indexlast)
			initcontext = i;
		sum -= contexts2[i];
		contexts4[i] = sum;
	}
	initcontext <<= 16;
	// sum counts
	sum = length;
	for (i=0x100; i--;)
	{	sum -= counts[i];
		counts[i] = sum;
	}
  }

	initHash2(htable);

	// in hardware: make lastseen a bitfield and zero them all when you hit a new
	// order 2 context. set them whenever you see a new order 4 context.
	// much like in the loop for the 0x????0000 contexts.

	// loop over all order 2 contexts
	// count contexts 0x????0000 first
	loop = in;
	lastseen = (uint2*)calloc(0x10000,sizeof(uint2));
	if (lastseen == NULL)
		sz_error(SZ_NOMEM_SORT);
	for (endloop = loop+contexts2[0]; loop<endloop; loop++)
	{	uint4 j, tmp = (uint4)(*loop);
		j = counts[tmp]++;
		tmp |= (uint4)(in[j])<<8;
		if (!lastseen[tmp])
		{	lastseen[tmp] = 1;
			h2_insert(htable, tmp<<16, contexts4[tmp]);
		}
		contexts4[tmp]++;
	}

	
	// and now all others (couldnt do it in one because of lastseens limited size)
	memset(lastseen, 0, 0x10000*sizeof(uint2));		// zero lastseen again
	for (i=1; i<0x10000; i++)
	{	if (indexlast >= contexts4[initcontext>>16])
			initcontext = (initcontext & 0xffff0000) | i;
		for (endloop = loop+contexts2[i]; loop<endloop; loop++)
		{	uint4 j, tmp = (uint4)(*loop);
			j = counts[tmp]++;
			tmp |= (uint4)(in[j])<<8;
			if (lastseen[tmp] != i)
			{	lastseen[tmp] = i;
				h2_insert(htable, tmp<<16 | i, contexts4[tmp]);
			}
			contexts4[tmp]++;
		}
	}

	free(contexts2);
	free(contexts4);
	free(lastseen);
	if (nocounts)
		free(counts);

	// do the actual unsorting
	initcontext = initcontext>>8 | (uint4)in[indexlast]<<24;
  {	uint4 context = initcontext;
	if (out == NULL)
		for ( i=0; i<length; i++ )
		{	unsigned char outchar;
			outchar = in[h2_get_inc(htable, context)];
			context = (context>>8) | ((uint4)outchar<<24);
			putc(outchar, stdout);
		}
	else
		for ( i=0; i<length; i++ )
		{	unsigned char outchar;
			out[i] = outchar = in[h2_get_inc(htable, context)];
			context = (context>>8) | ((uint4)outchar<<24);
		}

	if (context != initcontext)
		sz_error(SZ_NOTCYCLIC);
  }
	freeHash2(htable);
}
#endif // SZ_UNSRT_O4


#ifdef SZ_SRT_BW

// #include "qsort_u4.c"

void sz_srt_BW(unsigned char *inout, uint4 length, uint4 *indexfirst)
{	uint4 i, counts[256], counts1[256], *contextp, start;

	for (i=0; i<256; i++)
		counts[i] = 0;
	for (i=0; i<length; i++)
		counts[inout[i]]++;
	counts1[0] = 0;
	for (i=0; i<255; i++) 
		counts1[i+1] = counts1[i] + counts[i];
	
	contextp = (uint4*) calloc(length, sizeof(uint4));
	if (contextp == NULL)
		sz_error(SZ_NOMEM_SORT);

	for (i=0; i<length; i++)
		contextp[counts1[inout[i]]++] = i;

	start = 0;
	for (i=0; i<256; i++)
    {   
		// if (verbosity&1) fputc((char)('0'+i%10),stderr);
        if (counts[i])
        {	qsort_u4(contextp+start, counts[i], inout, i==inout[0]?0:1);
			if (i==inout[length-1]) // search for indexfirst
			{	uint4 j=start;
                while(contextp[j]!=(length-1))
                    j++;
				*indexfirst = j;
			}
            start += counts[i];
		}
    }

	contextp[*indexfirst] = 0;
	for(i=0; i<length; i++)
		contextp[i] = inout[contextp[i]+1];
	contextp[*indexfirst] = inout[0];
	for(i=0; i<length; i++)
		inout[i] = contextp[i];

	free(contextp);
}


void sz_unsrt_BW(unsigned char *in, unsigned char *out, uint4 length, uint4 indexfirst, uint4 *counts) {
    if (!out) {
        // or debug_log("ERROR: sz_unsrt_BW() called with NULL output buffer!\n");
        sz_error(SZ_NOMEM_SORT);
    }

    // If counts not supplied, allocate & fill them ourselves
    unsigned char needFreeCounts = (counts == NULL);
    if (needFreeCounts) {
        counts = (unsigned int *)calloc(256, sizeof(unsigned int));
        if (!counts) {
            sz_error(SZ_NOMEM_SORT);
        }
        for (unsigned int i = 0; i < length; i++) {
            counts[in[i]]++;
        }
    }

    // Convert to prefix sums: counts[i] = total # of symbols < i
    {
        unsigned int sum = length;
        for (int i = 255; i >= 0; i--) {
            sum        -= counts[i];
            counts[i]   = sum;
        }
    }

    // Prepare the transposition vector, size = length
    unsigned int *transvec = (unsigned int *)malloc(length * sizeof(unsigned int));
    if (!transvec) {
        if (needFreeCounts) free(counts);
        sz_error(SZ_NOMEM_SORT);
    }

    // Build the transposition vector
    //  - The block sort indices say "out of row i goes next row transvec[i]"
    //  - We'll fill that in using counts[]
    transvec[indexfirst] = counts[in[indexfirst]]++;
    for (unsigned int i = 0; i < indexfirst; i++) {
        transvec[i] = counts[in[i]]++;
    }
    for (unsigned int i = indexfirst + 1; i < length; i++) {
        transvec[i] = counts[in[i]]++;
    }

    // If we allocated counts, free it now
    if (needFreeCounts) {
        free(counts);
    }

    // Finally, walk the transvec to reconstruct the original data
    {
        unsigned int ic = indexfirst;
        for (unsigned int i = 0; i < length; i++) {
            out[i] = in[ic];
            ic     = transvec[ic];
        }
        if (ic != indexfirst) {
            // Means we didn't cycle back to start => data not fully reversed
            free(transvec);
            sz_error(SZ_NOTCYCLIC);
        }
    }

    free(transvec);
}

#endif // SZ_SRT_BW
// =================================================================================================
// szip.c
// -------------------------------------------------------------------------------------------------
/* szip.c                                                                   *
*                                                                           *
*  written by Michael Schindler michael@compressconsult.com                 *
*  1997,1998                                                                *
*  http://www.compressconsult.com/                                         */

static char vmayor=1, vminor=12;

/* Not needed for ESP32 implementation, included here for documentation purposes only
static void usage()
{   fprintf(stderr,"szip %d.%d (c)1997-2000 Michael Schindler, szip@compressconsult.com\n",
        vmayor, vminor);
    fprintf(stderr,"homepage: http://www.compressconsult.com/szip/\n");
    fprintf(stderr,"usage: szip [options] [inputfile [outputfile]]\n");
    fprintf(stderr,"option           meaning              default   range\n");
    fprintf(stderr,"-d               decompress\n");
    fprintf(stderr,"-b<blocksize>    blocksize in 100kB   -b1      1-41\n"); // default block size to minimum for ESP32-friendly decompression
    fprintf(stderr,"-o<order>        order of context     -o6       0, 3-255\n");
    fprintf(stderr,"-r<recordsize>   recordsize           -r1       1-127\n");
    fprintf(stderr,"-i               incremental          -i\n");
    fprintf(stderr,"-v<level>        verbositylevel       -v0       0-255\n");
    fprintf(stderr,"options may be combined into one, like -r3i\n");
    exit(1);
}
*/

#define COMPRESSION_TYPE_SZIP 'S' // ESP32-specific, not in the original
// uint order = 6;
#define VERBOSITY 0
// unsigned char recordsize = 1;

static void no_szip() {
    debug_log("no_szip: Not a valid SZIP encoding.\n");
    exit(1);
}


/* Read the global header from the input stream */
static void readglobalheader(szip_stream *stream) {
    /* Verify the Agon compression header prefix */
    if (get_byte(stream) != 'C') no_szip();
    if (get_byte(stream) != 'm') no_szip();
    if (get_byte(stream) != 'p') no_szip();
    if (get_byte(stream) != COMPRESSION_TYPE_SZIP) no_szip();
    debug_log("readglobalheader: Agon header ok\n");

    /* Read the original file size (4 bytes, little-endian) */
    uint4 orig_size = 0;
    orig_size |= (uint4)(unsigned char)get_byte(stream);
    orig_size |= (uint4)(unsigned char)get_byte(stream) << 8;
    orig_size |= (uint4)(unsigned char)get_byte(stream) << 16;
    orig_size |= (uint4)(unsigned char)get_byte(stream) << 24;
    debug_log("readglobalheader: Original size: %u\n", orig_size);

    /* Verify the SZIP magic SZ\012\004 magic chars */
    if (get_byte(stream) != 0x53) no_szip();  // 'S'
    if (get_byte(stream) != 0x5a) no_szip();  // 'Z'
    if (get_byte(stream) != 0x0a) no_szip();  // '\n'
    if (get_byte(stream) != 0x04) no_szip();  // version marker
    debug_log("readglobalheader: SZIP header ok\n");

    /* Verify the SZIP version number */
    int vmay = get_byte(stream);
    int vmin = get_byte(stream);
    if (vmay != vmayor || vmin != vminor) no_szip();
    debug_log("readglobalheader: SZIP version %d.%d\n", vmay, vmin);
}

static uint readblockdir(szip_stream *stream, uint4 *buflen) {
    int ch = get_byte(stream);
    if (ch == EOF) {
        *buflen = 0;
        return 0;
    }
    if (ch != 0x42) no_szip();
    if (get_byte(stream) != 0x48) no_szip();
    *buflen = read_uint3(stream);
    if (get_byte(stream) != 0) no_szip();
    debug_log("readblockdir: block size %d\n", *buflen);
    return 6;
}

static void readszipblock(szip_stream *stream, uint dirsize, uint4 buflen, unsigned char *buffer) {
    unsigned char *out_buffer;
    uint4 indexlast, charcount[256], bytesleft;
    sz_model *m = NULL;

    debug_log("readszipblock: Decoding %d bytes\n", buflen);

    // Read the block header info from the compressed stream using the standalone stream:
    indexlast = read_uint3(stream);  // updated: read from stream
    uint order = get_byte(stream);     // updated: read from stream
    debug_log("readszipblock: indexlast=%d order=%d\n", indexlast, order);

    // Initialize charcount to zero
    memset(charcount, 0, sizeof(charcount));

    // Dynamically allocate the sz_model
    m = (sz_model *)malloc(sizeof(sz_model));
    if (!m) {
        debug_log("readszipblock: memory allocation for sz_model failed\n");
        exit(1);
    }

    // Initialize the model for decompression.
    unsigned char recordsize = 1; // used to be global
    initmodel(m, -1, &recordsize, stream);
    debug_log("readszipblock: model initialized\n");

    // === Begin decoding runs into `buffer` ===
    unsigned char *tmp = buffer;
    bytesleft = buflen;

    // Decode the *first* run
    {
        uint4 runlength;
        uint ch;

        sz_decode(m, &ch, &runlength, stream);
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

    fixafterfirst(m);
    debug_log("readszipblock: first run decoded, bytesleft=%d\n", bytesleft);

    // Decode the rest of the runs
    while (bytesleft) {
        uint4 runlength;
        uint ch;

        sz_decode(m, &ch, &runlength, stream);
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
    deletemodel(m, stream);
    debug_log("readszipblock: model deleted\n");

    // Allocate a separate output buffer for "unsorting"
    out_buffer = (unsigned char *)malloc(buflen);
    if (!out_buffer) {
        debug_log("memory allocation failure\n");
        exit(1);
    }

    // Perform unsorting into out_buffer.
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
        // Perform the "unreorder" step
        unreorder(out_buffer, buffer, buflen, recordsize & 0x7F);
        debug_log("readszipblock: unsorted\n");
    }

    // Copy final output back into buffer
    memcpy(buffer, out_buffer, buflen);
    free(out_buffer);
    free(m);

    debug_log("readszipblock: done\n");
}

static void decompressit(szip_stream *stream, unsigned char *inoutbuffer, uint32_t *outSize) {
    uint32_t blocksize = 0;
    
    // Read the global header using the stream.
    readglobalheader(stream); 
    *outSize = 0;  // Reset output size

    while (1) {
        uint32_t blocklen;
        uint32_t dirsize;
        int ch;

        // Read the block directory from the stream.
        dirsize = readblockdir(stream, &blocklen);
        if (dirsize == 0) break;

        if (blocklen > blocksize)
            blocksize = blocklen;  // Track maximum block size

        // Read the block marker from the stream.
        ch = get_byte(stream);
        if (ch == 1) {
            debug_log("decompressit: Reading compressed block, size=%d bytes\n", blocklen);
            readszipblock(stream, dirsize + 1, blocklen, inoutbuffer);
        } else {
            debug_log("decompressit: [ERROR] Expected block marker 0x01, got 0x%02X\n", ch);
            no_szip();
        }
        
        *outSize = blocklen;  // Update the output size
    }
}

#include "buffers.h"

void szip_decompress(uint16_t sourceBufferId, BufferVector &sourceBuffer, uint8_t *buffer, uint32_t orig_size) { 
    if (sourceBuffer.empty() || !buffer) {
        debug_log("szip_decompress: ERROR - Empty source buffer or null output buffer!\n");
        return;
    }

    // Retrieve the compressed input buffer and its size.
    const uint8_t *compressedData = sourceBuffer.front()->getBuffer();
    uint32_t compressedSize = sourceBuffer.front()->size();

    debug_log("szip_decompress: Starting decompression for buffer %u (compressed size: %u bytes, expected output: %u bytes)...\n",
              sourceBufferId, compressedSize, orig_size);

    // // Dump first few bytes of compressed data to verify input.
    // debug_log("Compressed data (first 64 bytes):");
    // for (int i = 0; i < 64 && i < compressedSize; i++) {
    //     debug_log(" %02X", compressedData[i]);
    // }
    // debug_log("\n");

    // Create and initialize a local szip_stream instance.
    szip_stream stream;
    stream.sourceBuffer = compressedData;
    stream.sourceSize   = compressedSize;
    stream.sourcePos    = 0;  // Always start at the beginning for new data.
    uint32_t decompressedSize = 0;
    decompressit(&stream, buffer, &decompressedSize);

    if (decompressedSize == 0) {
        debug_log("szip_decompress: ERROR - Decompression failed: No data output.\n");
        return;
    }

    if (decompressedSize != orig_size) {
        debug_log("szip_decompress: WARNING - Output size mismatch! Decompressed %u bytes, expected %u bytes.\n",
                  decompressedSize, orig_size);
    } else {
        debug_log("szip_decompress: Success! Decompressed %u bytes.\n", decompressedSize);
    }
}
