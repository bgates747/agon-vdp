# Wolf3D column renderer

`wolf3d_draw.h`'s `Wolf3dRenderer` implements the real per-column raycast
and billboard projection: `CalcProjection`/`SetupView`/`CalcHeight`/
`TransformActor`/`CalcRotate` (view math), `WallRefresh` (grid-DDA against
the tilemap buffer referenced by `Wolf3dWorldState::tilemapBufferId`,
including door open/closed blocking), and `DrawScaleds` (projects every
active actor/static via the shared `TransformPoint` helper, culls off-screen/
behind-camera sprites, depth-sorts far-to-near) are all implemented and
validated against `agonport/assets/generated/dda_smoke_test_map.bin`.
`WallRefresh`/`DrawScaleds` fill in per-column/per-sprite output arrays;
see "Blit stage" below for how those get turned into pixels.

Original id Software equivalent: `WL_DRAW.C`'s raycast column loop, and the
`WL_SCALE.C`/`CONTIGSC.C`/`OLDSCALE.C` family of compiled column scalers
(three CPU-tier-specific implementations of the same job in the original —
Agon only needs one modern implementation). Given a screen column, a
texture ID + source column, and a top/bottom height, this blits a scaled
textured vertical strip into the render target.

The texture *asset layout* question is resolved (see `wolf3d_world.h`'s
`Wolf3dWallBufferId()`/`Wolf3dSpriteBufferId()`): no shared atlas buffer,
one small VDP bitmap buffer per texture, selected by a fixed buffer-id
offset from the wall texture id / sprite shapenum already on the wire.
`WallRefresh`'s output (`WallHeights()`/`WallTiles()`/`WallTexU()`/
`WallSides()`) and `DrawScaleds`' output (`VisSprites()`/
`VisSpriteCount()`) are what a blit stage should consume. Also still
missing: `DrawScaleds` doesn't call `CalcRotate` yet -- that needs a
per-actor-class `numRotations`/`dirangle` table that isn't modeled in
`Wolf3dWorldState` yet.

## Blit stage (implemented)

`WallRefresh`/`DrawScaleds` only fill in per-column/per-sprite scratch
output. The actual pixel blit is a two-part split:

- `Wolf3dRenderer::SampleWallColumn()`/`SampleSprite()` (in this file) do
  the nearest-neighbor scaling math (mirroring `WL_SCALE.C`'s
  `ScaleShape`/`SimpleScaleShape` job) into a caller-owned byte buffer.
  Pure pixel math, no VDP calls.
- `Wolf3dControl::RenderWalls()`/`RenderSprites()` (`video/wolf3d.h`) do
  the actual VDP-side orchestration: write into a scratch VDU buffer
  (`bufferCreate`/`bufferClear`), convert it to a `Bitmap`
  (`createBitmapFromBuffer()`, format 1 = RGBA2222), and draw it with
  `Canvas::drawBitmap()`. They live in `Wolf3dControl` rather than
  `Wolf3dRenderer` because that's where the `VDUStreamProcessor` friend
  access to those buffer/bitmap calls already exists.

This is the approved method: buffer -> bitmap -> bitmap-plot, not
per-pixel `Canvas::setPixel()` (ruled out as non-performant during Pingo's
own development) and not any per-column/per-sprite wire command (rendering
is entirely VDP-internal). The pipeline is RGBA2222 end to end -- source
texture bitmaps, the scratch buffers, and the constructed column/sprite
bitmaps are all format 1.

FabGL queues only a raw pointer when `Canvas::drawBitmap()` is called.
Consequently, each scratch blit is drained with
`waitPlotCompletion(false)` while its `Bitmap` and backing VDU buffer still
exist, before the next iteration clears and recreates that scratch ID.
`render_frame` performs a final drain before reporting completion, so its
wire-level behavior remains synchronous.

`RenderSprites()` also reproduces the original `ScaleShape` wall-occlusion
rule without giving up the single whole-bitmap blit. After scaling, it maps
each scratch column back to its clipped viewport X coordinate and preserves
that column only when `wallHeight[viewX] < spriteHeight`; equality belongs to
the wall. Occluded columns are zeroed to transparent RGBA2222 before bitmap
creation. `userspace/wolf3d_renderer_test.cpp` covers the equality boundary,
nearer and farther walls, clipped X offsets, and preservation of existing
transparent sprite pixels.

## First correct rendering milestone

The `first-correct-render` tag marks the first emulator- and hardware-verified
pass of the real shareware level: correct quarter-pixel projection scaling,
VDP-local floor/ceiling clear, synchronous scratch-bitmap lifetime, 30 Hz
callback-gated double buffering, and correct half-cell door faces with
perpendicular jamb geometry. The full development narrative and qualification
hashes live in the sibling `Wolf3dOrig` repository at
`agonport/doc/first_correct_render_success_story.md`.

## Moving-door texture regression

Door-plane geometry and the adjacent perpendicular jamb were already correct
at the first rendering milestone. A partially open door still needs its source
column to move with the slab, however: for an actual door hit, texture U is the
non-stepped-axis fraction minus the door's 0.16 position. Jamb and ordinary
wall hits deliberately keep their unshifted fraction.

`userspace/wolf3d_renderer_test.cpp` exercises the production DDA directly
without the FabGL runtime. It covers horizontal and vertical moving slabs, the
exact `fraction == position` pass boundary, both jamb orientations, and the
parallel neighboring wall faces. Run it with:

```bash
make -C userspace renderer-test
```
