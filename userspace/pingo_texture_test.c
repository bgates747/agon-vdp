#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../video/pingo/render/texture.h"

static uint8_t pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (uint8_t)((a << 6) | (b << 4) | (g << 2) | r);
}

static void assert_pixel(Pixel pixel, uint8_t expected)
{
    assert(pixel.c == expected);
}

int main(void)
{
    static const uint8_t boundaries[] = {
        0, 63, 64, 127, 128, 191, 192, 255,
    };
    static const uint8_t quantized[] = {
        0, 0, 1, 1, 2, 2, 3, 3,
    };

    _Static_assert(sizeof(Pixel) == 1, "Pingo working pixels must be one byte");
    assert_pixel(PIXELBLACK, 0xC0);
    assert_pixel(PIXELWHITE, 0xFF);

    for (unsigned int i = 0; i < sizeof(boundaries); ++i) {
        assert_pixel(
            pixelFromRGBA(
                boundaries[i],
                boundaries[i],
                boundaries[i],
                boundaries[i]),
            pack(quantized[i], quantized[i], quantized[i], quantized[i]));
    }

    uint8_t packed_pixels[256];
    for (unsigned int value = 0; value < 256; ++value) {
        packed_pixels[value] = (uint8_t)value;
    }

    Texture rgba2222;
    assert(texture_init_format(
        &rgba2222,
        (Vec2i){16, 16},
        packed_pixels,
        TEXTURE_FORMAT_RGBA2222) == 0);

    for (unsigned int value = 0; value < 256; ++value) {
        Pixel pixel = texture_read(
            &rgba2222,
            (Vec2i){(int)(value % 16), (int)(value / 16)});
        assert_pixel(pixel, (uint8_t)value);
        assert(pixelFromRGBA8888(pixelToRGBA8888(pixel)).c == value);
    }

    uint8_t rgba8888_pixels[16] = {
        0,   64, 128, 192,
        63, 127, 191, 255,
        64, 128, 192, 255,
        255, 192, 128, 64,
    };
    Texture rgba8888;
    assert(texture_init_format(
        &rgba8888,
        (Vec2i){2, 2},
        rgba8888_pixels,
        TEXTURE_FORMAT_RGBA8888) == 0);
    assert_pixel(texture_read(&rgba8888, (Vec2i){0, 0}), pack(0, 1, 2, 3));
    assert_pixel(texture_read(&rgba8888, (Vec2i){1, 0}), pack(0, 1, 2, 3));
    assert_pixel(texture_read(&rgba8888, (Vec2i){0, 1}), pack(1, 2, 3, 3));
    assert_pixel(texture_read(&rgba8888, (Vec2i){1, 1}), pack(3, 3, 2, 1));

    // UV V grows upward while the first source row is the image top.
    assert_pixel(texture_readF(&rgba8888, (Vec2f){0.0f, 1.0f}), pack(0, 1, 2, 3));
    assert_pixel(texture_readF(&rgba8888, (Vec2f){1.0f, 1.0f}), pack(0, 1, 2, 3));
    assert_pixel(texture_readF(&rgba8888, (Vec2f){0.0f, 0.0f}), pack(1, 2, 3, 3));
    assert_pixel(texture_readF(&rgba8888, (Vec2f){1.0f, 0.0f}), pack(3, 3, 2, 1));

    Pixel target_storage[4] = {{0}, {0}, {0}, {0}};
    Texture target;
    assert(texture_init(&target, (Vec2i){2, 2}, target_storage) == 0);
    assert(target.format == TEXTURE_FORMAT_RGBA2222);
    texture_draw(&target, (Vec2i){1, 1}, (Pixel){0xE4});
    assert(target_storage[0].c == 0);
    assert(target_storage[3].c == 0xE4);

    // Runtime illumination may intentionally exceed unity. Packed channels
    // must saturate rather than wrap when overdriven; alpha is unchanged.
    PixelShadeLut unity = pixelShadeLut(1.0f);
    PixelShadeLut overdrive = pixelShadeLut(2.0f);
    PixelShadeLut dark = pixelShadeLut(0.0f);
    Pixel source = (Pixel){pack(1, 2, 3, 3)};
    assert_pixel(pixelMulLut(source, &unity), pack(1, 2, 3, 3));
    assert_pixel(pixelMulLut(source, &overdrive), pack(2, 3, 3, 3));
    assert_pixel(pixelMulLut(source, &dark), pack(0, 0, 0, 3));
    assert_pixel(pixelMul(source, 2.0f), pack(2, 3, 3, 3));

    // Multiplicative overdrive saturates channels that are present; a zero
    // channel remains zero rather than being raised toward white.
    Pixel saturatedRed = (Pixel){pack(3, 0, 0, 3)};
    assert_pixel(pixelMulLut(saturatedRed, &overdrive), saturatedRed.c);

    puts("Pingo one-byte pixel and dual-format texture sampling passed");
    return 0;
}
