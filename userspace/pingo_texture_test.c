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

    uint8_t fixed_rgba2222_pixels[6] = {
        0x01, 0x02, 0x03,
        0x04, 0x05, 0x06,
    };
    Texture fixed_rgba2222;
    assert(texture_init_format(
        &fixed_rgba2222,
        (Vec2i){3, 2},
        fixed_rgba2222_pixels,
        TEXTURE_FORMAT_RGBA2222) == 0);

    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, INT32_MIN, INT32_MIN),
        0x01);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, 65535, 65535),
        0x01);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, 1 << 16, 0),
        0x02);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, (2 << 16) - 1, 0),
        0x02);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, 2 << 16, 0),
        0x03);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, 0, 1 << 16),
        0x04);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, INT32_MAX, INT32_MAX),
        0x06);

    // Fixed V is already a top-down memory row; normalized UV V grows up.
    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, 0, 0),
        texture_readFInline(
            &fixed_rgba2222, (Vec2f){0.0f, 1.0f}).c);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &fixed_rgba2222, 0, 1 << 16),
        texture_readFInline(
            &fixed_rgba2222, (Vec2f){0.0f, 0.0f}).c);

    assert_pixel(
        texture_read_fixed16_16_inline(
            &rgba8888, INT32_MIN, INT32_MIN),
        pack(0, 1, 2, 3));
    assert_pixel(
        texture_read_fixed16_16_inline(
            &rgba8888, 1 << 16, 0),
        pack(0, 1, 2, 3));
    assert_pixel(
        texture_read_fixed16_16_inline(
            &rgba8888, 0, 1 << 16),
        pack(1, 2, 3, 3));
    assert_pixel(
        texture_read_fixed16_16_inline(
            &rgba8888, INT32_MAX, INT32_MAX),
        pack(3, 3, 2, 1));

    uint8_t one_pixel_storage = 0xA5;
    Texture one_pixel;
    assert(texture_init_format(
        &one_pixel,
        (Vec2i){1, 1},
        &one_pixel_storage,
        TEXTURE_FORMAT_RGBA2222) == 0);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &one_pixel, INT32_MIN, INT32_MIN),
        0xA5);
    assert_pixel(
        texture_read_fixed16_16_inline(
            &one_pixel, INT32_MAX, INT32_MAX),
        0xA5);

    Pixel target_storage[4] = {{0}, {0}, {0}, {0}};
    Texture target;
    assert(texture_init(&target, (Vec2i){2, 2}, target_storage) == 0);
    assert(target.format == TEXTURE_FORMAT_RGBA2222);
    texture_draw(&target, (Vec2i){1, 1}, (Pixel){0xE4});
    assert(target_storage[0].c == 0);
    assert(target_storage[3].c == 0xE4);

    puts("Pingo one-byte pixel and dual-format texture sampling passed");
    return 0;
}
