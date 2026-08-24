#include "pixel.h"

#ifdef P2C_PIXEL_RGBA2222

static uint8_t channel_from_u8(uint8_t value) {
    return value >> 6;
}

static uint8_t channel_to_u8(uint8_t value) {
    return (uint8_t)(value * 85u);
}

Pixel pixelRandom(void) {
    return (Pixel){(uint8_t)((uint8_t)rand() | 0xC0u)};
}

Pixel pixelFromUInt8(uint8_t gray) {
    uint8_t packed = channel_from_u8(gray);
    return (Pixel){(uint8_t)(0xC0u | (packed << 4) |
                            (packed << 2) | packed)};
}

uint8_t pixelToUInt8(Pixel *pixel) {
    uint16_t red = channel_to_u8(pixel->c & 0x03u);
    uint16_t green = channel_to_u8((pixel->c >> 2) & 0x03u);
    uint16_t blue = channel_to_u8((pixel->c >> 4) & 0x03u);
    return (uint8_t)((red + green + blue) / 3u);
}

Pixel pixelFromRGBA(uint8_t red, uint8_t green, uint8_t blue,
                    uint8_t alpha) {
    return (Pixel){(uint8_t)(
        (channel_from_u8(alpha) << 6) |
        (channel_from_u8(blue) << 4) |
        (channel_from_u8(green) << 2) |
        channel_from_u8(red)
    )};
}

Pixel pixelMul(Pixel pixel, float factor) {
    uint8_t red = (uint8_t)(channel_to_u8(pixel.c & 0x03u) * factor);
    uint8_t green = (uint8_t)(
        channel_to_u8((pixel.c >> 2) & 0x03u) * factor
    );
    uint8_t blue = (uint8_t)(
        channel_to_u8((pixel.c >> 4) & 0x03u) * factor
    );
    return (Pixel){(uint8_t)(
        (pixel.c & 0xC0u) |
        (channel_from_u8(blue) << 4) |
        (channel_from_u8(green) << 2) |
        channel_from_u8(red)
    )};
}

#else

Pixel pixelRandom(void) {
    return (Pixel){
        (uint8_t)rand(), (uint8_t)rand(), (uint8_t)rand(), 255
    };
}

Pixel pixelFromUInt8(uint8_t gray) {
    return (Pixel){gray, gray, gray, 255};
}

uint8_t pixelToUInt8(Pixel *pixel) {
    return (uint8_t)((pixel->r + pixel->g + pixel->b) / 3);
}

Pixel pixelFromRGBA(uint8_t red, uint8_t green, uint8_t blue,
                    uint8_t alpha) {
    return (Pixel){blue, green, red, alpha};
}

Pixel pixelMul(Pixel pixel, float factor) {
    return (Pixel){
        (uint8_t)(pixel.b * factor),
        (uint8_t)(pixel.g * factor),
        (uint8_t)(pixel.r * factor),
        pixel.a,
    };
}

#endif
