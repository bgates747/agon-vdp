#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../video/pingo/render/texture.h"

static void assert_pixel(Pixel pixel, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    assert(pixel.r == r);
    assert(pixel.g == g);
    assert(pixel.b == b);
    assert(pixel.a == a);
}

int main(void)
{
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
        assert_pixel(
            pixel,
            (uint8_t)((value & 0x03) * 85),
            (uint8_t)(((value >> 2) & 0x03) * 85),
            (uint8_t)(((value >> 4) & 0x03) * 85),
            (uint8_t)(((value >> 6) & 0x03) * 85));
    }

    Pixel rgba8888_pixels[4] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
    };
    Texture rgba8888;
    assert(texture_init(&rgba8888, (Vec2i){2, 2}, rgba8888_pixels) == 0);
    assert_pixel(texture_read(&rgba8888, (Vec2i){1, 0}), 5, 6, 7, 8);

    // UV V grows upward while the first source row is the image top.
    assert_pixel(texture_readF(&rgba8888, (Vec2f){0.0f, 1.0f}), 1, 2, 3, 4);
    assert_pixel(texture_readF(&rgba8888, (Vec2f){1.0f, 1.0f}), 5, 6, 7, 8);
    assert_pixel(texture_readF(&rgba8888, (Vec2f){0.0f, 0.0f}), 9, 10, 11, 12);
    assert_pixel(texture_readF(&rgba8888, (Vec2f){1.0f, 0.0f}), 13, 14, 15, 16);

    puts("Pingo RGBA2222/RGBA8888 texture sampling passed");
    return 0;
}
