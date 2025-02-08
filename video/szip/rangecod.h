#ifndef rangecod_h
#define rangecod_h

/*
  rangecod.h     headerfile for range encoding

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

  The input and output is done by the INBYTE and OUTBYTE macros
  defined in the .c file; change them as needed; the first parameter
  passed to them is a pointer to the rangecoder structure; extend that
  structure as needed (and don't forget to initialize the values in
  start_encoding resp. start_decoding). This distribution writes to
  stdout and reads from stdin.

  There are no global or static var's, so if the IO is thread save the
  whole rangecoder is - unless GLOBALRANGECODER is defined.

  For error recovery the last 3 bytes written contain the total number
  of bytes written since starting the encoder. This can be used to
  locate the beginning of a block if you have only the end.

  For some application using a global coder variable may provide a better
  performance. This will allow you to use only one coder at a time and
  will destroy thread savety. To enabble this feature uncomment the
  #define GLOBALRANGECODER line below.
*/

#include "port.h"
#include <stdint.h>
#include <esp_heap_caps.h>

#define GLOBALRANGECODER

typedef uint32_t code_value; /* Type of a range code value (must accommodate 32 bits) */

/* Recommended total frequency count limits */
typedef uint32_t freq;

typedef struct {
    uint32_t low;       /* Low end of interval */
    uint32_t range;     /* Length of interval */
    uint32_t help;      /* Bytes_to_follow resp. intermediate value */
    uint8_t buffer;     /* Buffer for input/output */
    uint32_t bytecount; /* Counter for output bytes */
} RangeCoder;

#ifdef __cplusplus
extern "C" {
#endif

/* Function prototypes */
void start_encoding(RangeCoder *rc, char c, int initlength);
void encode_freq(RangeCoder *rc, freq sy_f, freq lt_f, freq tot_f);
void encode_shift(RangeCoder *rc, freq sy_f, freq lt_f, freq shift);
uint32_t done_encoding(RangeCoder *rc);

int start_decoding(RangeCoder *rc);
freq decode_culfreq(RangeCoder *rc, freq tot_f);
freq decode_culshift(RangeCoder *rc, freq shift);
void decode_update(RangeCoder *rc, freq sy_f, freq lt_f, freq tot_f);
void done_decoding(RangeCoder *rc);

unsigned char decode_byte(RangeCoder *rc);
unsigned short decode_short(RangeCoder *rc);

#ifdef __cplusplus
}
#endif

/* Macros */
#define encode_byte(rc, b)  encode_shift(rc, (freq)1, (freq)(b), (freq)8)
#define encode_short(rc, s) encode_shift(rc, (freq)1, (freq)(s), (freq)16)
#define decode_update_shift(rc, f1, f2, f3) decode_update((rc), (f1), (f2), (freq)1 << (f3))

#endif /* rangecod_h */
