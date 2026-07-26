#include "pixel.h"

#ifdef RGBA2222P
// Helpers
Pixel pixelFromRGBA8888(uint32_t rgba8888) {
    // Extract 8-bit values for each channel
    uint8_t r8 = (rgba8888 >> 0) & 0xFF;   // Red channel (8 bits)
    uint8_t g8 = (rgba8888 >> 8) & 0xFF;   // Green channel (8 bits)
    uint8_t b8 = (rgba8888 >> 16) & 0xFF;  // Blue channel (8 bits)
    uint8_t a8 = (rgba8888 >> 24) & 0xFF;  // Alpha channel (8 bits)

    // Convert 8-bit to 2-bit values (0-255 to 0-3 range)
    uint8_t r2 = (r8 > 170) ? 3 : (r8 > 85) ? 2 : (r8 > 0) ? 1 : 0;
    uint8_t g2 = (g8 > 170) ? 3 : (g8 > 85) ? 2 : (g8 > 0) ? 1 : 0;
    uint8_t b2 = (b8 > 170) ? 3 : (b8 > 85) ? 2 : (b8 > 0) ? 1 : 0;
    uint8_t a2 = (a8 > 170) ? 3 : (a8 > 85) ? 2 : (a8 > 0) ? 1 : 0;

    // Combine 2-bit values into a single 8-bit RGBA2222P format
    uint8_t rgba2222p = (a2 << 6) | (b2 << 4) | (g2 << 2) | r2;
    return (Pixel){rgba2222p};
}

// Convert from RGBA2222P (Pixel) to RGBA8888 (uint32_t)
uint32_t pixelToRGBA8888(Pixel pixel) {
    // Extract 2-bit values for each channel
    uint8_t rgba2222p = pixel.c;
    uint8_t a2 = (rgba2222p >> 6) & 0b11;
    uint8_t b2 = (rgba2222p >> 4) & 0b11;
    uint8_t g2 = (rgba2222p >> 2) & 0b11;
    uint8_t r2 = rgba2222p & 0b11;

    // Map 2-bit values to 8-bit color values
    static const uint8_t mapping[4] = {0, 85, 170, 255};
    uint8_t r8 = mapping[r2];
    uint8_t g8 = mapping[g2];
    uint8_t b8 = mapping[b2];
    uint8_t a8 = mapping[a2];

    // Combine 8-bit values into a single 32-bit RGBA8888 format
    return (a8 << 24) | (b8 << 16) | (g8 << 8) | r8;
}

// Randomly generate a Pixel in RGBA2222P format
extern Pixel pixelRandom() {
    uint8_t rgba2222p = (uint8_t)rand() | 0b11000000;
    return (Pixel){rgba2222p};
}

// Create a Pixel in RGBA2222P format from a grayscale value
extern Pixel pixelFromUInt8(uint8_t g) {
    uint8_t g2 = (g > 170) ? 3 : (g > 85) ? 2 : (g > 0) ? 1 : 0;
    uint8_t rgba2222p = (3 << 6) | (g2 << 4) | (g2 << 2) | g2;  // Alpha is fully opaque
    return (Pixel){rgba2222p};
}

// Convert a Pixel in RGBA2222P format to grayscale
extern uint8_t pixelToUInt8(Pixel* p) {
    uint32_t rgba8888 = pixelToRGBA8888(*p);
    uint8_t r = (rgba8888 >> 0) & 0xFF;
    uint8_t g = (rgba8888 >> 8) & 0xFF;
    uint8_t b = (rgba8888 >> 16) & 0xFF;
    return (r + g + b) / 3;  // Average to compute grayscale
}

// Create a Pixel in RGBA2222P format from RGBA components
extern Pixel pixelFromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Convert 8-bit to 2-bit
    uint8_t r2 = (r > 170) ? 3 : (r > 85) ? 2 : (r > 0) ? 1 : 0;
    uint8_t g2 = (g > 170) ? 3 : (g > 85) ? 2 : (g > 0) ? 1 : 0;
    uint8_t b2 = (b > 170) ? 3 : (b > 85) ? 2 : (b > 0) ? 1 : 0;
    uint8_t a2 = (a > 170) ? 3 : (a > 85) ? 2 : (a > 0) ? 1 : 0;
    uint8_t rgba2222p = (a2 << 6) | (b2 << 4) | (g2 << 2) | r2;
    return (Pixel){rgba2222p};
}

// Multiply a Pixel's RGB values in RGBA2222P format by a float factor
extern Pixel pixelMul(Pixel p, float f) {
    uint32_t rgba8888 = pixelToRGBA8888(p);
    uint8_t r = (uint8_t)(((rgba8888 >> 0) & 0xFF) * f);
    uint8_t g = (uint8_t)(((rgba8888 >> 8) & 0xFF) * f);
    uint8_t b = (uint8_t)(((rgba8888 >> 16) & 0xFF) * f);
    uint8_t a = (rgba8888 >> 24) & 0xFF;
    return pixelFromRGBA(r, g, b, a);
}
#endif

#ifdef UINT8

extern Pixel pixelRandom() {
    return (Pixel){(uint8_t)rand()};
}

uint8_t pixelToUInt8(Pixel * p)
{
    return p->g;
}

extern Pixel pixelFromUInt8( uint8_t g){
    return (Pixel){g};
}

extern Pixel pixelMul(Pixel p, float f)
{
    return (Pixel){p.g*f};
}

extern Pixel pixelFromRGBA( uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (Pixel){((r + g + b) / 3)};
}
#endif

#ifdef RGB888
extern Pixel pixelRandom() {
    return (Pixel){(uint8_t)rand(),(uint8_t)rand(),(uint8_t)rand()};
}

uint32_t pixelToRGBA(Pixel * p)
{
    uint8_t g = p->g;
    uint32_t a = p->r | p->g <<8 | p->b<<16| 255<<24;
    return a;
}
#endif

#ifdef RGBA8888
extern Pixel pixelRandom() {
    return (Pixel){(uint8_t)rand(),(uint8_t)rand(),(uint8_t)rand(),255};
}

extern Pixel pixelFromUInt8( uint8_t g){
    return (Pixel){g,g,g, 255};
}
extern uint8_t pixelToUInt8( Pixel * p){
    return (p->r + p->g + p->b) / 3;
}

extern Pixel pixelFromRGBA( uint8_t r, uint8_t g, uint8_t b, uint8_t a){
    return (Pixel){r,g,b,a};
}

extern Pixel pixelMul(Pixel p, float f)
{
    return (Pixel){p.r*f,p.g*f,p.b*f,p.a};
}

#endif


#ifdef BGRA8888
extern Pixel pixelRandom() {
    return (Pixel){(uint8_t)rand(),(uint8_t)rand(),(uint8_t)rand(),255};
}

extern Pixel pixelFromUInt8( uint8_t g){
    return (Pixel){g,g,g, 255};
}

extern uint8_t pixelToUInt8( Pixel * p){
    return (p->r + p->g + p->b) / 3;
}

extern Pixel pixelFromRGBA( uint8_t r, uint8_t g, uint8_t b, uint8_t a){
    return (Pixel){r,g,b,a};
}

extern Pixel pixelMul(Pixel p, float f)
{
    return (Pixel){p.r*f, p.g*f, p.b*f, p.a};
}

#endif
