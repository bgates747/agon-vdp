#ifndef BITMODEL_H
#define BITMODEL_H

/*
  bitmodel.h     headerfile for bit indexed trees probability model

  (c) Michael Schindler
  1997, 1998
  http://www.compressconsult.com or http://eiunix.tuwien.ac.at/~michael
  michael@compressconsult.com        michael@eiunix.tuwien.ac.at

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

  bitmodel implements bit indexed trees for frequency storage described
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

#include "port.h"
#include <stdint.h>
#include <esp_heap_caps.h>

#define EXCLUDEONUPDATE

typedef struct {
    int n;             /* Number of symbols */
    int totalfreq;     /* Total frequency count (without excluded symbols) */
    int max_totf;      /* Maximum allowed total frequency count */
    int incr;          /* Increment per update */
    int mask;          /* Initial bitmask used for search */
    uint16_t *f;       /* Frequency for the symbol; first bit set if excluded */
    uint16_t *cf;      /* Array of cumulative frequencies */
} bitmodel;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization of bitmodel */
void initbitmodel(bitmodel *m, int n, int max_totf, int rescale, int *init);

/* Reinitialization of bitmodel */
void resetbitmodel(bitmodel *m, int *init);

/* Deletion of bitmodel */
void deletebitmodel(bitmodel *m);

/* Retrieval of estimated frequencies for a symbol */
void bitgetfreq(bitmodel *m, int sym, int *sy_f, int *lt_f);

/* Find total frequency */
#define bitmodel_total_freq(m) ((m)->totalfreq)

/* Find symbol for a given cumulative frequency */
int bitgetsym(bitmodel *m, int lt_f);

/* Update model */
void bitupdate(bitmodel *m, int sym);

#ifdef EXCLUDEONUPDATE
/* Update model and exclude symbol */
void bitupdate_ex(bitmodel *m, int sym);

/* Deactivate symbol */
void bitdeactivate(bitmodel *m, int sym);

/* Reactivate symbol */
void bitreactivate(bitmodel *m, int sym);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BITMODEL_H */
