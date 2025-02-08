/*
  bitmodel.c     bit indexed trees probability model

  (c) Michael Schindler
  1997, 1998
  http://www.compressconsult.com or http://eiunix.tuwien.ac.at/~michael
  michael@compressconsult.com       michael@eiunix.tuwien.ac.at

  based on: Peter Fenwick: A New Data Structure for Cumulative Probability Tables
  Technical Report 88, Dep. of Computer Science, University of Auckland, NZ

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.  It may be that this
  program violates local patents in your country, however it is
  belived (NO WARRANTY!) to be patent-free here in Austria and I am
  not aware of a violation elsewhere.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston,
  MA 02111-1307, USA.

  Bitmodel implements bit indexed trees for frequency storage described
  by Peter Fenwick: A New Data Structure for Cumulative Probability Tables
  Technical Report 88, Dep. of Computer Science, University of Auckland, NZ.
  It features a fast method for cumulative frequency storage and updating.
  The difference to the fenwick paper is the way the table is recalculated
  after rescaling; the method here is faster.

  There is a compiletime switch; if EXCLUDEONUPDATE is defined symbols
  are excluded on update; to be able to use them again you have to call
  the include function for that symbol.

  The module provides functions for creation, reset, deletion, query for
  probabilities, queries for symbols, reenabling symbols and model updating.
*/

#include "bitmodel.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

static inline void build_cf(BitModel *m) {
    int i;
    uint16_t *cf = m->cf;
    m->totalfreq = 0;
    for (i = 1; i <= m->n; i <<= 1) {
        for (int j = i; j <= m->n; j += i << 1) {
            int k;
#ifdef EXCLUDEONUPDATE
            if (m->f[j - 1] & 0x8000)
                cf[j] = 0;
            else
#endif
                m->totalfreq += (cf[j] = m->f[j - 1]);
            for (k = i >> 1; k; k >>= 1)
                cf[j] += cf[j - k];
        }
    }
}

static void scale_freq(BitModel *m) {
    for (uint16_t *f = m->f, *endf = f + m->n; f < endf; f++)
#ifdef EXCLUDEONUPDATE
        *f = ((1 + (*f & 0x7FFF)) >> 1) | (*f & 0x8000);
#else
        *f = (1 + *f) >> 1;
#endif
    build_cf(m);
}

void init_bitmodel(BitModel *m, int n, int max_totf, int rescale, int *init) {
    m->n = n;
    if (max_totf < n << 1) max_totf = n << 1;
    m->max_totf = max_totf;
    m->incr = max_totf / 2 / rescale;
    if (m->incr < 1) m->incr = 1;
    m->f = (uint16_t *)heap_caps_malloc(n * sizeof(uint16_t), MALLOC_CAP_8BIT);
    m->cf = (uint16_t *)heap_caps_malloc((n + 1) * sizeof(uint16_t), MALLOC_CAP_8BIT);
    m->mask = 1;
    while (n >>= 1)
        m->mask <<= 1;
    reset_bitmodel(m, init);
}

void reset_bitmodel(BitModel *m, int *init) {
    if (init == NULL) {
        for (int i = 0; i < m->n; i++)
            m->f[i] = 1;
        m->totalfreq = m->n;
    } else {
        m->totalfreq = 0;
        for (int i = 0; i < m->n; i++) {
            m->f[i] = init[i];
            m->totalfreq += init[i];
        }
    }
    while (m->totalfreq > m->max_totf)
        scale_freq(m);
    build_cf(m);
}

void delete_bitmodel(BitModel *m) {
    free(m->f);
    free(m->cf);
    m->n = 0;
}

void bitmodel_get_freq(BitModel *m, int sym, int *sy_f, int *lt_f) {
    int cul;
    uint16_t *cf = m->cf;
    *sy_f = m->f[sym];
    sym++;
    cul = cf[sym];
    while (sym &= sym - 1)
        cul += cf[sym];
    *lt_f = cul - *sy_f;
}

int bitmodel_get_symbol(BitModel *m, int lt_f) {
    int sym = 0, mask = m->mask, n = m->n;
    uint16_t *cf = m->cf;
    do {
        int x;
        if ((x = sym | mask) <= n && lt_f >= cf[x]) {
            lt_f -= cf[x];
            sym = x;
        }
    } while (mask >>= 1);
    return sym;
}

void bitmodel_update(BitModel *m, int sym) {
    m->f[sym] += m->incr;
    m->totalfreq += m->incr;
    if (m->totalfreq > m->max_totf)
        scale_freq(m);
    else {
        uint16_t *cf = m->cf;
        sym++;
        while (sym <= m->n) {
            cf[sym] += m->incr;
            sym = (sym | (sym - 1)) + 1;
        }
    }
}

#ifdef EXCLUDEONUPDATE
void bitmodel_update_ex(BitModel *m, int sym) {
    int delta = -m->f[sym];
    m->f[sym] = (m->f[sym] + m->incr) | 0x8000;
    m->totalfreq += delta;
    if (m->totalfreq > m->max_totf)
        scale_freq(m);
}

void bitmodel_deactivate(BitModel *m, int sym) {
    m->totalfreq -= m->f[sym];
    m->f[sym] |= 0x8000;
}

void bitmodel_reactivate(BitModel *m, int sym) {
    m->f[sym] &= 0x7FFF;
    m->totalfreq += m->f[sym];
}
#endif
