#ifndef QSMODEL_H
#define QSMODEL_H

/*
  qsmodel.h     headerfile for quasistatic probability model

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

#include "port.h"
#include <stdint.h>
#include <esp_heap_caps.h>

typedef struct {
    int n;             /* Number of symbols */
    int left;          /* Symbols to next rescale */
    int nextleft;      /* Symbols with other increment */
    int rescale;       /* Intervals between rescales */
    int targetrescale; /* Should be interval between rescales */
    int incr;          /* Increment per update */
    int searchshift;   /* Shift for lt_freq before using as index */
    uint16_t *cf;      /* Array of cumulative frequencies */
    uint16_t *newf;    /* Array for collecting statistics */
    uint16_t *search;  /* Structure for searching on decompression */
} QSModel;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization of QSModel */
void init_qsmodel(QSModel *m, int n, int lg_totf, int rescale, int *init, int compress);

/* Reinitialization of QSModel */
void reset_qsmodel(QSModel *m, int *init);

/* Deletion of QSModel */
void delete_qsmodel(QSModel *m);

/* Retrieval of estimated frequencies for a symbol */
void qsmodel_get_freq(QSModel *m, int sym, int *sy_f, int *lt_f);

/* Find symbol for a given cumulative frequency */
int qsmodel_get_symbol(QSModel *m, int lt_f);

/* Update model */
void qsmodel_update(QSModel *m, int sym);

#ifdef __cplusplus
}
#endif

#endif /* QSMODEL_H */
