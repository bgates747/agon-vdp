/*----------------------------------------------------------------------------

Texture Test Program - a cheesy test harness for texture mapping

by Chris Hecker for my Game Developer Magazine articles.  See my homepage
for more information.

NOTE: This is a hacked test program, not a nice example of Windows programming.
The texture mappers are the only part of this you should look at.

This material is Copyright 1997 Chris Hecker, All Rights Reserved.
It's for you to read and learn from, not to put in your own articles
or books or on your website, etc.  Thank you.

Chris Hecker
checker@d6.com
http://www.d6.com/users/checker

*/
/*----------------------------------------------------------------------------

mappers header - this file contains the various platform-independent
	structures for the texture mappers

Chris Hecker (checker@netcom.com)

*/


/*----------------------------------------------------------------------------

types

*/
#pragma once

#include <stdint.h>

/*----------------------------------------------------------------------------

Type definitions

*/

typedef int32_t fixed28_4;
typedef int32_t fixed16_16;

/*----------------------------------------------------------------------------

Struct definitions

*/

typedef struct {
    float X, Y;
    float Z, U, V;
} P3D_fl_fl;

typedef struct {
    int32_t X, Y;
    float Z, U, V;
} P3D_i_fl;

typedef struct {
    fixed28_4 X, Y;
    float Z, U, V;
} P3D_fx_fl;

typedef struct {
    fixed28_4 X, Y;
    fixed16_16 Z, U, V;
} P3D_fx_fx;

typedef struct {
    union {
        P3D_fl_fl flfl;
        P3D_i_fl ifl;
        P3D_fx_fl fxfl;
        P3D_fx_fx fxfx;
    };
} POINT3D;

typedef struct {
    unsigned char *pBits;
    int32_t Width;
    int32_t Height;
    int32_t DeltaScan;
} dib_info;

typedef struct {
    float aOneOverZ[3];          // 1/z for each vertex
    float aUOverZ[3];            // u/z for each vertex
    float aVOverZ[3];            // v/z for each vertex
    float dOneOverZdX, dOneOverZdY;  // d(1/z)/dX, d(1/z)/dY
    float dUOverZdX, dUOverZdY;      // d(u/z)/dX, d(u/z)/dY
    float dVOverZdX, dVOverZdY;      // d(v/z)/dX, d(v/z)/dY
    fixed16_16 dUdXModifier;
    fixed16_16 dVdXModifier;
} gradients_fx_fl_a;

typedef struct {
    int X, XStep, Numerator, Denominator;     // DDA info for x
    int ErrorTerm;
    int Y, Height;                             // current y and vertical count
    float OneOverZ, OneOverZStep, OneOverZStepExtra;  // 1/z and step
    float UOverZ, UOverZStep, UOverZStepExtra;        // u/z and step
    float VOverZ, VOverZStep, VOverZStepExtra;        // v/z and step
} edge_fx_fl_a;

/********** Function Prototypes **********/

void InitGradients_fx_fl_a(gradients_fx_fl_a* Gradients, const POINT3D* pVertices);
void InitEdge_fx_fl_a(edge_fx_fl_a* Edge, const gradients_fx_fl_a* Gradients, const POINT3D* pVertices, int Top, int Bottom);
int StepEdge_fx_fl_a(edge_fx_fl_a* Edge);

void DrawScanLine_suba(const dib_info* Dest, const gradients_fx_fl_a* Gradients, edge_fx_fl_a* pLeft, edge_fx_fl_a* pRight, const dib_info* Texture);

void TextureMapTriangle_suba_fx_fl(const dib_info* Dest, const POINT3D* pVertices, const dib_info* Texture);

/*----------------------------------------------------------------------------

Inline functions

*/

static inline fixed28_4 FloatToFixed28_4(float Value) {
    return (fixed28_4)(Value * 16);
}

static inline float Fixed28_4ToFloat(fixed28_4 Value) {
    return Value / 16.0f;
}

static inline fixed16_16 FloatToFixed16_16(float Value) {
    return (fixed16_16)(Value * 65536);
}

static inline float Fixed16_16ToFloat(fixed16_16 Value) {
    return Value / 65536.0f;
}

static inline fixed28_4 Fixed28_4Mul(fixed28_4 A, fixed28_4 B) {
    return (A * B) / 16;  // 28.4 * 28.4 = 24.8 / 16 = 28.4
}

static inline int32_t Ceil28_4(fixed28_4 Value) {
    int32_t ReturnValue;
    int32_t Numerator = Value - 1 + 16;
    if (Numerator >= 0) {
        ReturnValue = Numerator / 16;
    } else {
        // Deal with negative numerators correctly
        ReturnValue = -((-Numerator) / 16);
        ReturnValue -= ((-Numerator) % 16) ? 1 : 0;
    }
    return ReturnValue;
}

inline void FloorDivMod(int Numerator, int Denominator, int* Floor, int* Mod) {
    assert(Denominator > 0);  // we assume it's positive
    if (Numerator >= 0) {
        // Positive case, C is okay
        *Floor = Numerator / Denominator;
        *Mod = Numerator % Denominator;
    } else {
        // Numerator is negative, handle it properly
        *Floor = -((-Numerator) / Denominator);
        *Mod = (-Numerator) % Denominator;
        if (*Mod) {
            // There is a remainder
            (*Floor)--;
            *Mod = Denominator - *Mod;
        }
    }
}

/* ----------------------------------------------------------------------------
Additional Agon/Pingo-specific code 
*/

#include "texture.h"
#include "pixel.h"

dib_info texture_to_dib_info(Texture *texture);