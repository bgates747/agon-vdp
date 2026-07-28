#include "pixel.h"

#ifdef RGBA2222P

static uint8_t pixelChannelFromUInt8(uint8_t value)
{
    return value >> 6;
}

static uint8_t pixelChannelToUInt8(uint8_t value)
{
    return value * 85;
}

extern Pixel pixelRandom()
{
    return (Pixel){(uint8_t)rand() | 0xC0};
}

extern Pixel pixelFromUInt8(uint8_t g)
{
    uint8_t g2 = pixelChannelFromUInt8(g);
    return (Pixel){(uint8_t)(0xC0 | (g2 << 4) | (g2 << 2) | g2)};
}

extern uint8_t pixelToUInt8(Pixel *p)
{
    uint16_t r = pixelChannelToUInt8(p->c & 0x03);
    uint16_t g = pixelChannelToUInt8((p->c >> 2) & 0x03);
    uint16_t b = pixelChannelToUInt8((p->c >> 4) & 0x03);
    return (r + g + b) / 3;
}

extern Pixel pixelFromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (Pixel){
        (uint8_t)(
            (pixelChannelFromUInt8(a) << 6) |
            (pixelChannelFromUInt8(b) << 4) |
            (pixelChannelFromUInt8(g) << 2) |
            pixelChannelFromUInt8(r))
    };
}

extern Pixel pixelFromRGBA8888(uint32_t rgba8888)
{
    return pixelFromRGBA(
        rgba8888 & 0xFF,
        (rgba8888 >> 8) & 0xFF,
        (rgba8888 >> 16) & 0xFF,
        (rgba8888 >> 24) & 0xFF);
}

extern uint32_t pixelToRGBA8888(Pixel pixel)
{
    uint32_t r = pixelChannelToUInt8(pixel.c & 0x03);
    uint32_t g = pixelChannelToUInt8((pixel.c >> 2) & 0x03);
    uint32_t b = pixelChannelToUInt8((pixel.c >> 4) & 0x03);
    uint32_t a = pixelChannelToUInt8((pixel.c >> 6) & 0x03);
    return r | (g << 8) | (b << 16) | (a << 24);
}

extern Pixel pixelMul(Pixel p, float f)
{
    uint8_t r = (uint8_t)(pixelChannelToUInt8(p.c & 0x03) * f);
    uint8_t g = (uint8_t)(pixelChannelToUInt8((p.c >> 2) & 0x03) * f);
    uint8_t b = (uint8_t)(pixelChannelToUInt8((p.c >> 4) & 0x03) * f);
    return (Pixel){
        (uint8_t)(
            (p.c & 0xC0) |
            (pixelChannelFromUInt8(b) << 4) |
            (pixelChannelFromUInt8(g) << 2) |
            pixelChannelFromUInt8(r))
    };
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
