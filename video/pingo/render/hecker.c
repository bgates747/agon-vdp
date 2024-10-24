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

/******** Perspective texture mapper *********/

#include <math.h>
#include <assert.h>
#include "hecker.h"

/******** TextureMapTriangle **********/

void TextureMapTriangle_suba_fx_fl(const dib_info* Dest, const POINT3D* pVertices, const dib_info* Texture) {
    int Top, Middle, Bottom, MiddleForCompare, BottomForCompare;
    fixed28_4 Y0 = pVertices[0].fxfl.Y, Y1 = pVertices[1].fxfl.Y, Y2 = pVertices[2].fxfl.Y;

    // Sort vertices in y
    if (Y0 < Y1) {
        if (Y2 < Y0) {
            Top = 2; Middle = 0; Bottom = 1;
            MiddleForCompare = 0; BottomForCompare = 1;
        } else {
            Top = 0;
            if (Y1 < Y2) {
                Middle = 1; Bottom = 2;
                MiddleForCompare = 1; BottomForCompare = 2;
            } else {
                Middle = 2; Bottom = 1;
                MiddleForCompare = 2; BottomForCompare = 1;
            }
        }
    } else {
        if (Y2 < Y1) {
            Top = 2; Middle = 1; Bottom = 0;
            MiddleForCompare = 1; BottomForCompare = 0;
        } else {
            Top = 1;
            if (Y0 < Y2) {
                Middle = 0; Bottom = 2;
                MiddleForCompare = 3; BottomForCompare = 2;
            } else {
                Middle = 2; Bottom = 0;
                MiddleForCompare = 2; BottomForCompare = 3;
            }
        }
    }

    gradients_fx_fl_a Gradients;
    InitGradients_fx_fl_a(&Gradients, pVertices);
    edge_fx_fl_a TopToBottom, TopToMiddle, MiddleToBottom;

    InitEdge_fx_fl_a(&TopToBottom, &Gradients, pVertices, Top, Bottom);
    InitEdge_fx_fl_a(&TopToMiddle, &Gradients, pVertices, Top, Middle);
    InitEdge_fx_fl_a(&MiddleToBottom, &Gradients, pVertices, Middle, Bottom);

    edge_fx_fl_a* pLeft;
    edge_fx_fl_a* pRight;
    int MiddleIsLeft;

    // If bottom > middle, middle is right
    if (BottomForCompare > MiddleForCompare) {
        MiddleIsLeft = 0;
        pLeft = &TopToBottom; pRight = &TopToMiddle;
    } else {
        MiddleIsLeft = 1;
        pLeft = &TopToMiddle; pRight = &TopToBottom;
    }

    int Height = TopToMiddle.Height;
    while (Height--) {
        DrawScanLine_suba(Dest, &Gradients, pLeft, pRight, Texture);
        StepEdge_fx_fl_a(&TopToMiddle);
        StepEdge_fx_fl_a(&TopToBottom);
    }

    Height = MiddleToBottom.Height;
    if (MiddleIsLeft) {
        pLeft = &MiddleToBottom; pRight = &TopToBottom;
    } else {
        pLeft = &TopToBottom; pRight = &MiddleToBottom;
    }

    while (Height--) {
        DrawScanLine_suba(Dest, &Gradients, pLeft, pRight, Texture);
        StepEdge_fx_fl_a(&MiddleToBottom);
        StepEdge_fx_fl_a(&TopToBottom);
    }
}

/********** gradients_fx_fl_a initializer **********/

void InitGradients_fx_fl_a(gradients_fx_fl_a* Gradients, const POINT3D* pVertices) {
    int Counter;

    fixed28_4 X1Y0 = Fixed28_4Mul(pVertices[1].fxfl.X - pVertices[2].fxfl.X, pVertices[0].fxfl.Y - pVertices[2].fxfl.Y);
    fixed28_4 X0Y1 = Fixed28_4Mul(pVertices[0].fxfl.X - pVertices[2].fxfl.X, pVertices[1].fxfl.Y - pVertices[2].fxfl.Y);
    float OneOverdX = 1.0f / Fixed28_4ToFloat(X1Y0 - X0Y1);
    float OneOverdY = -OneOverdX;

    for (Counter = 0; Counter < 3; Counter++) {
        float OneOverZ = 1 / pVertices[Counter].fxfl.Z;
        Gradients->aOneOverZ[Counter] = OneOverZ;
        Gradients->aUOverZ[Counter] = pVertices[Counter].fxfl.U * OneOverZ;
        Gradients->aVOverZ[Counter] = pVertices[Counter].fxfl.V * OneOverZ;
    }

    Gradients->dOneOverZdX = OneOverdX * (((Gradients->aOneOverZ[1] - Gradients->aOneOverZ[2]) * Fixed28_4ToFloat(pVertices[0].fxfl.Y - pVertices[2].fxfl.Y)) -
                                          ((Gradients->aOneOverZ[0] - Gradients->aOneOverZ[2]) * Fixed28_4ToFloat(pVertices[1].fxfl.Y - pVertices[2].fxfl.Y)));
    Gradients->dOneOverZdY = OneOverdY * (((Gradients->aOneOverZ[1] - Gradients->aOneOverZ[2]) * Fixed28_4ToFloat(pVertices[0].fxfl.X - pVertices[2].fxfl.X)) -
                                          ((Gradients->aOneOverZ[0] - Gradients->aOneOverZ[2]) * Fixed28_4ToFloat(pVertices[1].fxfl.X - pVertices[2].fxfl.X)));

    Gradients->dUOverZdX = OneOverdX * (((Gradients->aUOverZ[1] - Gradients->aUOverZ[2]) * Fixed28_4ToFloat(pVertices[0].fxfl.Y - pVertices[2].fxfl.Y)) -
                                        ((Gradients->aUOverZ[0] - Gradients->aUOverZ[2]) * Fixed28_4ToFloat(pVertices[1].fxfl.Y - pVertices[2].fxfl.Y)));
    Gradients->dUOverZdY = OneOverdY * (((Gradients->aUOverZ[1] - Gradients->aUOverZ[2]) * Fixed28_4ToFloat(pVertices[0].fxfl.X - pVertices[2].fxfl.X)) -
                                        ((Gradients->aUOverZ[0] - Gradients->aUOverZ[2]) * Fixed28_4ToFloat(pVertices[1].fxfl.X - pVertices[2].fxfl.X)));

    Gradients->dVOverZdX = OneOverdX * (((Gradients->aVOverZ[1] - Gradients->aVOverZ[2]) * Fixed28_4ToFloat(pVertices[0].fxfl.Y - pVertices[2].fxfl.Y)) -
                                        ((Gradients->aVOverZ[0] - Gradients->aVOverZ[2]) * Fixed28_4ToFloat(pVertices[1].fxfl.Y - pVertices[2].fxfl.Y)));
    Gradients->dVOverZdY = OneOverdY * (((Gradients->aVOverZ[1] - Gradients->aVOverZ[2]) * Fixed28_4ToFloat(pVertices[0].fxfl.X - pVertices[2].fxfl.X)) -
                                        ((Gradients->aVOverZ[0] - Gradients->aVOverZ[2]) * Fixed28_4ToFloat(pVertices[1].fxfl.X - pVertices[2].fxfl.X)));

    // Set up rounding modifiers
    // Originally platform-specific modifiers
    // fixed16_16 Half = 0x8000;          // Windows-specific
    // fixed16_16 PosModifier = Half;     // Windows-specific
    // fixed16_16 NegModifier = Half - 1; // Windows-specific

    fixed16_16 Half = (fixed16_16)(1 << 15); // Generic
    fixed16_16 PosModifier = Half;            // Generic
    fixed16_16 NegModifier = Half - 1;        // Generic

    float dUdXIndicator = Gradients->dUOverZdX * Gradients->aOneOverZ[0] - Gradients->aUOverZ[0] * Gradients->dOneOverZdX;

    if (dUdXIndicator > 0) {
        Gradients->dUdXModifier = PosModifier;
    } else if (dUdXIndicator < 0) {
        Gradients->dUdXModifier = NegModifier;
    } else {
        float dUdYIndicator = Gradients->dUOverZdY * Gradients->aOneOverZ[0] - Gradients->aUOverZ[0] * Gradients->dOneOverZdY;

        if (dUdYIndicator >= 0) {
            Gradients->dUdXModifier = PosModifier;
        } else {
            Gradients->dUdXModifier = NegModifier;
        }
    }

    float dVdXIndicator = Gradients->dVOverZdX * Gradients->aOneOverZ[0] - Gradients->aVOverZ[0] * Gradients->dOneOverZdX;

    if (dVdXIndicator > 0) {
        Gradients->dVdXModifier = PosModifier;
    } else if (dVdXIndicator < 0) {
        Gradients->dVdXModifier = NegModifier;
    } else {
        float dVdYIndicator = Gradients->dVOverZdY * Gradients->aOneOverZ[0] - Gradients->aVOverZ[0] * Gradients->dOneOverZdY;

        if (dVdYIndicator >= 0) {
            Gradients->dVdXModifier = PosModifier;
        } else {
            Gradients->dVdXModifier = NegModifier;
        }
    }
}

/********** edge_fx_fl_a initializer **********/

void InitEdge_fx_fl_a(edge_fx_fl_a* Edge, const gradients_fx_fl_a* Gradients, const POINT3D* pVertices, int Top, int Bottom) {
    Edge->Y = Ceil28_4(pVertices[Top].fxfl.Y);
    int YEnd = Ceil28_4(pVertices[Bottom].fxfl.Y);
    Edge->Height = YEnd - Edge->Y;

    if (Edge->Height) {
        int dN = pVertices[Bottom].fxfl.Y - pVertices[Top].fxfl.Y;
        int dM = pVertices[Bottom].fxfl.X - pVertices[Top].fxfl.X;

        int InitialNumerator = dM * 16 * Edge->Y - dM * pVertices[Top].fxfl.Y + dN * pVertices[Top].fxfl.X - 1 + dN * 16;
        FloorDivMod(InitialNumerator, dN * 16, &Edge->X, &Edge->ErrorTerm);  // Pass addresses
        FloorDivMod(dM * 16, dN * 16, &Edge->XStep, &Edge->Numerator);       // Pass addresses
        Edge->Denominator = dN * 16;

        float YPrestep = Fixed28_4ToFloat(Edge->Y * 16 - pVertices[Top].fxfl.Y);
        float XPrestep = Fixed28_4ToFloat(Edge->X * 16 - pVertices[Top].fxfl.X);

        Edge->OneOverZ = Gradients->aOneOverZ[Top] + YPrestep * Gradients->dOneOverZdY + XPrestep * Gradients->dOneOverZdX;
        Edge->OneOverZStep = Edge->XStep * Gradients->dOneOverZdX + Gradients->dOneOverZdY;
        Edge->OneOverZStepExtra = Gradients->dOneOverZdX;

        Edge->UOverZ = Gradients->aUOverZ[Top] + YPrestep * Gradients->dUOverZdY + XPrestep * Gradients->dUOverZdX;
        Edge->UOverZStep = Edge->XStep * Gradients->dUOverZdX + Gradients->dUOverZdY;
        Edge->UOverZStepExtra = Gradients->dUOverZdX;

        Edge->VOverZ = Gradients->aVOverZ[Top] + YPrestep * Gradients->dVOverZdY + XPrestep * Gradients->dVOverZdX;
        Edge->VOverZStep = Edge->XStep * Gradients->dVOverZdX + Gradients->dVOverZdY;
        Edge->VOverZStepExtra = Gradients->dVOverZdX;
    }
}

/********** Edge step function **********/

int StepEdge_fx_fl_a(edge_fx_fl_a* Edge) {
    Edge->X += Edge->XStep;
    Edge->Y++;
    Edge->Height--;

    Edge->UOverZ += Edge->UOverZStep;
    Edge->VOverZ += Edge->VOverZStep;
    Edge->OneOverZ += Edge->OneOverZStep;

    Edge->ErrorTerm += Edge->Numerator;
    if (Edge->ErrorTerm >= Edge->Denominator) {
        Edge->X++;
        Edge->ErrorTerm -= Edge->Denominator;
        Edge->OneOverZ += Edge->OneOverZStepExtra;
        Edge->UOverZ += Edge->UOverZStepExtra;
        Edge->VOverZ += Edge->VOverZStepExtra;
    }
    return Edge->Height;
}

/********** DrawScanLine ************/

void DrawScanLine_suba(const dib_info* Dest, const gradients_fx_fl_a* Gradients, edge_fx_fl_a* pLeft, edge_fx_fl_a* pRight, const dib_info* Texture) {
    int XStart = pLeft->X;
    int Width = pRight->X - XStart;

    unsigned char* pDestBits = Dest->pBits;
    unsigned char const* pTextureBits = Texture->pBits;
    pDestBits += pLeft->Y * Dest->DeltaScan + XStart;
    int TextureDeltaScan = Texture->DeltaScan;

    int const AffineLength = 8;

    float OneOverZLeft = pLeft->OneOverZ;
    float UOverZLeft = pLeft->UOverZ;
    float VOverZLeft = pLeft->VOverZ;

    float dOneOverZdXAff = Gradients->dOneOverZdX * AffineLength;
    float dUOverZdXAff = Gradients->dUOverZdX * AffineLength;
    float dVOverZdXAff = Gradients->dVOverZdX * AffineLength;

    float OneOverZRight = OneOverZLeft + dOneOverZdXAff;
    float UOverZRight = UOverZLeft + dUOverZdXAff;
    float VOverZRight = VOverZLeft + dVOverZdXAff;

    float ZLeft = 1 / OneOverZLeft;
    float ULeft = ZLeft * UOverZLeft;
    float VLeft = ZLeft * VOverZLeft;

    float ZRight, URight, VRight;
    fixed16_16 U, V, DeltaU, DeltaV;

    if (Width > 0) {
        int Subdivisions = Width / AffineLength;
        int WidthModLength = Width % AffineLength;

        if (!WidthModLength) {
            Subdivisions--;
            WidthModLength = AffineLength;
        }

        while (Subdivisions-- > 0) {
            ZRight = 1 / OneOverZRight;
            URight = ZRight * UOverZRight;
            VRight = ZRight * VOverZRight;

            U = FloatToFixed16_16(ULeft) + Gradients->dUdXModifier;
            V = FloatToFixed16_16(VLeft) + Gradients->dVdXModifier;
            DeltaU = FloatToFixed16_16(URight - ULeft) / AffineLength;
            DeltaV = FloatToFixed16_16(VRight - VLeft) / AffineLength;

            for (int Counter = 0; Counter < AffineLength; Counter++) {
                int UInt = U >> 16;
                int VInt = V >> 16;

                *(pDestBits++) = *(pTextureBits + UInt + (VInt * TextureDeltaScan));

                U += DeltaU;
                V += DeltaV;
            }

            ZLeft = ZRight;
            ULeft = URight;
            VLeft = VRight;

            OneOverZRight += dOneOverZdXAff;
            UOverZRight += dUOverZdXAff;
            VOverZRight += dVOverZdXAff;
        }

        if (WidthModLength) {
            ZRight = 1 / (pRight->OneOverZ - Gradients->dOneOverZdX);
            URight = ZRight * (pRight->UOverZ - Gradients->dUOverZdX);
            VRight = ZRight * (pRight->VOverZ - Gradients->dVOverZdX);

            U = FloatToFixed16_16(ULeft) + Gradients->dUdXModifier;
            V = FloatToFixed16_16(VLeft) + Gradients->dVdXModifier;

            if (--WidthModLength) {
                DeltaU = FloatToFixed16_16(URight - ULeft) / WidthModLength;
                DeltaV = FloatToFixed16_16(VRight - VLeft) / WidthModLength;
            }

            for (int Counter = 0; Counter <= WidthModLength; Counter++) {
                int UInt = U >> 16;
                int VInt = V >> 16;

                *(pDestBits++) = *(pTextureBits + UInt + (VInt * TextureDeltaScan));

                U += DeltaU;
                V += DeltaV;
            }
        }
    }
}

/* ----------------------------------------------------------------------------
Additional Agon/Pingo-specific code 
*/

dib_info texture_to_dib_info(Texture *texture) {
    dib_info info;
    info.pBits = (unsigned char *)texture->pixels;
    info.Width = texture->size.x;
    info.Height = texture->size.y;
    info.DeltaScan = texture->size.x;
    return info;
}
