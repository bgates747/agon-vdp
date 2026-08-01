# Wolf3D presentation effects

This directory contains presentation-only mechanics which do not belong in
the eZ80 simulation state. `wolf3d_fizzle.h` is the pure, deterministic
iterator for the ordinary player-death fizzle from
`orig/src/WOLFSRC/ID_VH.C::FizzleFade` in the sibling `Wolf3dOrig` checkout.

The iterator decodes the current 17-bit LFSR state before advancing it, uses
the original `0x12000` feedback polynomial, and accepts only `x < width` and
`y < height`. The strict comparison intentionally corrects the original
routine's one-row/one-column overrun while preserving its pixel order. For
the current centered 256x160 viewport the maximal 131071-state cycle emits
all 40960 pixels exactly once.

`Wolf3dControl::fizzle_to_red` turns the pure order into sixty cumulative
display batches. The DOS effect used 70 ticks at 70 Hz; sixty VDP swaps at
60 Hz preserve its intended one-second duration. A batch is written directly
to FabGL's hidden raw scanlines, swapped at vertical blank, then replayed from
an iterator snapshot into the newly hidden surface. Thus both mode-8 buffers
remain identical after every presentation step. Only the active renderer
viewport is touched: all four play-surround bands and the complete HUD remain
intact. With the current layout that viewport is `(32,20)` through `(287,179)`
inside the 320x200 play area, followed by the 320x40 HUD at `y=200`.

Raw scanline pixels use FabGL's required `x ^ 2` dword-byte mapping and a raw
`RGB222(2,0,0)` pixel, the Agon64 equivalent of Wolf palette index 4
(`RGB888(170,0,0)`). The eZ80 must not submit another Wolf3D state or render
command while a fizzle token is outstanding.

Run the host regression with:

```sh
make -C userspace fizzle-test
```

It verifies the full LFSR period, the known initial coordinate sequence,
strict unique 256x160 coverage, 60-batch distribution, mirrored-buffer
identity after every batch, and untouched pixels outside the viewport.
