/*
  qsmodel.c     headerfile for quasistatic probability model

  (c) Michael Schindler
  1997, 1998
  http://www.compressconsult.com/ or http://eiunix.tuwien.ac.at/~michael
  michael@compressconsult.com        michael@eiunix.tuwien.ac.at

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.  It may be that this
  program violates local patents in your country, however it is
  belived (NO WARRANTY!) to be patent-free here in Austria.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston,
  MA 02111-1307, USA.

  Qsmodel is a quasistatic probability model that periodically
  (at chooseable intervals) updates probabilities of symbols;
  it also allows to initialize probabilities. Updating is done more
  frequent in the beginning, so it adapts very fast even without
  initialisation.

  it provides function for creation, deletion, query for probabilities
  and symbols and model updating.

  for usage see example.c
*/

#include "qsmodel.h"
#include <stdio.h>
#include <stdlib.h>

/* Default table size 1 << TBLSHIFT */
#define TBLSHIFT 7

/* Rescale frequency counts */
static void qsmodel_rescale(QSModel *m) {
    int i, cf, missing;
    if (m->nextleft) { /* We have some more before actual rescaling */
        m->incr++;
        m->left = m->nextleft;
        m->nextleft = 0;
        return;
    }
    if (m->rescale < m->targetrescale) { /* Double rescale interval if needed */
        m->rescale <<= 1;
        if (m->rescale > m->targetrescale)
            m->rescale = m->targetrescale;
    }
    cf = missing = m->cf[m->n]; /* Do actual rescaling */
    for (i = m->n - 1; i; i--) {
        int tmp = m->newf[i];
        cf -= tmp;
        m->cf[i] = cf;
        tmp = tmp >> 1 | 1;
        missing -= tmp;
        m->newf[i] = tmp;
    }
    if (cf != m->newf[0]) {
        fprintf(stderr, "BUG: rescaling left %d total frequency\n", cf);
        delete_qsmodel(m);
        exit(1);
    }
    m->newf[0] = m->newf[0] >> 1 | 1;
    missing -= m->newf[0];
    m->incr = missing / m->rescale;
    m->nextleft = missing % m->rescale;
    m->left = m->rescale - m->nextleft;
    if (m->search != NULL) {
        i = m->n;
        while (i) {
            int start, end;
            end = (m->cf[i] - 1) >> m->searchshift;
            i--;
            start = m->cf[i] >> m->searchshift;
            while (start <= end) {
                m->search[start] = i;
                start++;
            }
        }
    }
}

/* Initialization of QSModel */
void init_qsmodel(QSModel *m, int n, int lg_totf, int rescale, int *init, int compress) {
    m->n = n;
    m->targetrescale = rescale;
    m->searchshift = lg_totf - TBLSHIFT;
    if (m->searchshift < 0)
        m->searchshift = 0;
    
    m->cf = (uint16_t*) heap_caps_malloc((n + 1) * sizeof(uint16_t), MALLOC_CAP_8BIT);
    m->newf = (uint16_t*) heap_caps_malloc((n + 1) * sizeof(uint16_t), MALLOC_CAP_8BIT);
    m->cf[n] = 1 << lg_totf;
    m->cf[0] = 0;
    
    if (compress)
        m->search = NULL;
    else {
        m->search = (uint16_t*) heap_caps_malloc(((1 << TBLSHIFT) + 1) * sizeof(uint16_t), MALLOC_CAP_8BIT);
        m->search[1 << TBLSHIFT] = n - 1;
    }
    reset_qsmodel(m, init);
}

/* Reinitialization of QSModel */
void reset_qsmodel(QSModel *m, int *init) {
    int i, end, initval;
    m->rescale = m->n >> 4 | 2;
    m->nextleft = 0;
    if (init == NULL) {
        initval = m->cf[m->n] / m->n;
        end = m->cf[m->n] % m->n;
        for (i = 0; i < end; i++)
            m->newf[i] = initval + 1;
        for (; i < m->n; i++)
            m->newf[i] = initval;
    } else {
        for (i = 0; i < m->n; i++)
            m->newf[i] = init[i];
    }
    qsmodel_rescale(m);
}

/* Deletion of QSModel */
void delete_qsmodel(QSModel *m) {
    if (m->cf)
        heap_caps_free(m->cf);
    if (m->newf)
        heap_caps_free(m->newf);
    if (m->search)
        heap_caps_free(m->search);
}

/* Retrieval of estimated frequencies for a symbol */
void qsmodel_get_freq(QSModel *m, int sym, int *sy_f, int *lt_f) {
    *sy_f = m->cf[sym + 1] - (*lt_f = m->cf[sym]);
}

/* Find symbol for a given cumulative frequency */
int qsmodel_get_symbol(QSModel *m, int lt_f) {
    int lo, hi;
    uint16_t *tmp = m->search + (lt_f >> m->searchshift);
    lo = *tmp;
    hi = *(tmp + 1) + 1;
    while (lo + 1 < hi) {
        int mid = (lo + hi) >> 1;
        if (lt_f < m->cf[mid])
            hi = mid;
        else
            lo = mid;
    }
    return lo;
}

/* Update QSModel */
void qsmodel_update(QSModel *m, int sym) {
    if (m->left <= 0)
        qsmodel_rescale(m);
    m->left--;
    m->newf[sym] += m->incr;
}
