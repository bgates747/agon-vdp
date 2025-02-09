/*
  rangecod.c     range encoding

  (c) Michael Schindler
  1997, 1998, 1999
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
  belived (NO WARRANTY!) to be patent-free here in Austria. Glen
  Langdon also confirmed my poinion that IBM UK did not protect that
  method.


  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston,
  MA 02111-1307, USA.

  Range encoding is based on an article by G.N.N. Martin, submitted
  March 1979 and presented on the Video & Data Recording Conference,
  Southampton, July 24-27, 1979. If anyone can name the original
  copyright holder of that article or locate G.N.N. Martin please
  contact me; this might allow me to make that article available on
  the net for general public.

  Range coding is closely related to arithmetic coding, except that
  it does renormalisation in larger units than bits and is thus
  faster. An earlier version of this code was distributed as byte
  oriented arithmetic coding, but then I had no knowledge of Martin's
  paper from seventy-nine.

  The input and output is done by the inbyte and outbyte macros
  defined in the .c file; change them as needed; the first parameter
  passed to them is a pointer to the rangecoder structure; extend that
  structure as needed (and don't forget to initialize the values in
  start_encoding resp. start_decoding). This distribution writes to
  stdout and reads from stdin.

  There are no global or static var's, so if the IO is thread save the
  whole rangecoder is.

  For error recovery the last 3 bytes written contain the total number
  of bytes written since starting the encoder. This can be used to
  locate the beginning of a block if you have only the end.

  There is a supplementary file called renorm95.c available at the
  website (www.compressconsult.com/rangecoder/) that changes the range
  coder to an arithmetic coder for speed comparisons.

  define RENORM95 if you want the old renormalisation. Requires renorm95.c
  Note that the old version does not write out the bytes since init.
  you should not define GLOBALRANGECODER then. This Flag is provided
  only for spped comparisons between both renormalizations, see my
  data compression conference article 1998 for details.
*/
/* #define RENORM95 */

#include "rangecod.h"
#include <stdio.h>
#include <stdlib.h>

#define CODE_BITS 32
#define Top_value ((code_value)1 << (CODE_BITS - 1))

/* Macros for I/O */
#define outbyte(cod, x) putchar(x)
#define inbyte(cod) getchar()

#define SHIFT_BITS (CODE_BITS - 9)
#define EXTRA_BITS ((CODE_BITS - 2) % 8 + 1)
#define Bottom_value (Top_value >> 8)

static inline void enc_normalize(rangecoder *rc) {
    while (rc->range <= Bottom_value) {
        if (rc->low < (code_value)0xff << SHIFT_BITS) {
            outbyte(rc, rc->buffer);
            for (; rc->help; rc->help--) outbyte(rc, 0xff);
            rc->buffer = (uint8_t)(rc->low >> SHIFT_BITS);
        } else if (rc->low & Top_value) {
            outbyte(rc, rc->buffer + 1);
            for (; rc->help; rc->help--) outbyte(rc, 0);
            rc->buffer = (uint8_t)(rc->low >> SHIFT_BITS);
        } else {
            rc->help++;
        }
        rc->range <<= 8;
        rc->low = (rc->low << 8) & (Top_value - 1);
        rc->bytecount++;
    }
}

void start_encoding(rangecoder *rc, char c, int initlength) {
    rc->low = 0;
    rc->range = Top_value;
    rc->buffer = c;
    rc->help = 0;
    rc->bytecount = initlength;
}

void encode_freq(rangecoder *rc, freq sy_f, freq lt_f, freq tot_f) {
    code_value r, tmp;
    enc_normalize(rc);
    r = rc->range / tot_f;
    tmp = r * lt_f;
    rc->low += tmp;
    rc->range = r * sy_f;
}

void encode_shift(rangecoder *rc, freq sy_f, freq lt_f, freq shift) {
    code_value r, tmp;
    enc_normalize(rc);
    r = rc->range >> shift;
    tmp = r * lt_f;
    rc->low += tmp;
    rc->range = r * sy_f;
}

uint32_t done_encoding(rangecoder *rc) {
    uint32_t tmp;
    enc_normalize(rc);
    rc->bytecount += 5;
    if ((rc->low & (Bottom_value - 1)) < (rc->bytecount >> 1))
        tmp = rc->low >> SHIFT_BITS;
    else
        tmp = (rc->low >> SHIFT_BITS) + 1;

    if (tmp > 0xff) {
        outbyte(rc, rc->buffer + 1);
        for (; rc->help; rc->help--) outbyte(rc, 0);
    } else {
        outbyte(rc, rc->buffer);
        for (; rc->help; rc->help--) outbyte(rc, 0xff);
    }

    outbyte(rc, tmp & 0xff);
    outbyte(rc, (rc->bytecount >> 16) & 0xff);
    outbyte(rc, (rc->bytecount >> 8) & 0xff);
    outbyte(rc, rc->bytecount & 0xff);
    return rc->bytecount;
}

int start_decoding(rangecoder *rc) {
    int c = inbyte(rc);
    if (c == EOF) return EOF;
    rc->buffer = inbyte(rc);
    rc->low = rc->buffer >> (8 - EXTRA_BITS);
    rc->range = (code_value)1 << EXTRA_BITS;
    return c;
}

static inline void dec_normalize(rangecoder *rc) {
    while (rc->range <= Bottom_value) {
        rc->low = (rc->low << 8) | ((rc->buffer << EXTRA_BITS) & 0xff);
        rc->buffer = inbyte(rc);
        rc->low |= rc->buffer >> (8 - EXTRA_BITS);
        rc->range <<= 8;
    }
}

freq decode_culfreq(rangecoder *rc, freq tot_f) {
    freq tmp;
    dec_normalize(rc);
    rc->help = rc->range / tot_f;
    tmp = rc->low / rc->help;
    return tmp;
}

freq decode_culshift(rangecoder *rc, freq shift) {
    freq tmp;
    dec_normalize(rc);
    rc->help = rc->range >> shift;
    tmp = rc->low / rc->help;
    return tmp;
}

void decode_update(rangecoder *rc, freq sy_f, freq lt_f, freq tot_f) {
    code_value tmp;
    tmp = rc->help * lt_f;
    rc->low -= tmp;
    rc->range = rc->help * sy_f;
}

unsigned char decode_byte(rangecoder *rc) {
    unsigned char tmp = decode_culshift(rc, 8);
    decode_update(rc, 1, tmp, (freq)1 << 8);
    return tmp;
}

unsigned short decode_short(rangecoder *rc) {
    unsigned short tmp = decode_culshift(rc, 16);
    decode_update(rc, 1, tmp, (freq)1 << 16);
    return tmp;
}

void done_decoding(rangecoder *rc) {
    dec_normalize(rc);
}
