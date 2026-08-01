# Pingo lighting and per-triangle color implementation record

Status: firmware, native/sanitizer validation, companion `pingoasm` tooling
and fixture, embedded builds, disposable headless inspection, bespoke-emulator
visual acceptance, and physical-hardware visual acceptance are complete in the
working trees. Nothing in this tranche has yet been committed or pushed.

## 1. Implemented scope

1. [x] Set the qualified default directional light to normalized
   `(0, +1, -1)`.
2. [x] Add a scene-wide runtime command for changing light direction.
3. [x] Add a scene-wide runtime command for changing directional intensity,
   including deliberate overdrive with saturating color channels.
4. [x] Add a scene-wide ambient-light floor.
5. [x] Add a runtime command that disables illumination calculations and emits
   texture colors at their native RGBA2222 values.
6. [x] Add a mesh-selectable flat-palette mode: one sampled color per source
   triangle, with no texture interpolation across its fragments.
7. [x] Preserve Pingo's established geometry, UV, bitmap, object, clipping,
   depth, scanline, and output machinery.
8. [x] Preserve textured, illuminated rendering as the default mesh and scene
   state for applications that do not select the new modes. The separately
   approved default light direction is the intentional visual change.

## 2. Coordinate and lighting semantics

1. The established world convention is right-handed: `+X` right, `+Y` up,
   and `+Z` toward the viewer. Unrotated forward is `-Z`; the canonical camera
   pose `(0, 0, +25)` sees the origin along `-Z`. Firmware itself initializes
   the camera pose at the origin, so applications establish that separation.
2. The light is a scene-wide normalized direction from a surface toward the
   light, not a position. Point lights and per-object/material lighting remain
   outside this tranche.
3. The qualified default vector is normalized `(0, +1, -1)`: upward and
   toward world `-Z`. It is a direction, not a light position. Earlier draft
   language calling world `-Z` the camera side was incorrect; the canonical
   camera pose is on the scene's `+Z` side.
4. Command input components are signed 16-bit ratios. The bridge normalizes a
   complete nonzero vector once when received. A zero or incomplete vector
   leaves the accepted direction unchanged.
5. For each face, the directional term is:

   ```text
   directional = clamp((1 + dot(face_normal, light_direction)) / 2, 0, 1)
   ```

6. Intensity and ambient are unsigned bytes on a 127-unity scale. The final
   multiplier is:

   ```text
   shade = max(ambient / 127, directional * intensity / 127)
   ```

   Thus 0 is zero, 127 is unity, and 255 is approximately 2.008 times unity.
   Ambient is a floor rather than an additive term. RGB channels saturate when
   overdriven; the RGBA2222 alpha bits are preserved.
7. Disabling illumination retains the configured direction, intensity, and
   ambient values but skips normal, dot-product, and shade-table work. Both
   textured and flat-palette meshes then emit native sampled colors.
8. The `esp32dev-pingo-unlit` diagnostic environment remains a stronger
   compile-time override. Its `PINGO_DISABLE_ILLUMINATION=1` build cannot be
   re-lit by the runtime command.

## 3. Implemented wire API

1. [x] Subcommand 43: set light direction.

   Payload: signed little-endian 16-bit `x; y; z;`. The vector is normalized;
   zero and truncated vectors are rejected without changing state.

2. [x] Subcommand 44: set directional intensity.

   Payload: unsigned byte `intensity`, on the 127-unity scale.

3. [x] Subcommand 45: set ambient-light floor.

   Payload: unsigned byte `ambient`, on the 127-unity scale.

4. [x] Subcommand 46: enable or disable illumination.

   Payload: unsigned byte `enabled`; only 0 and 1 are accepted.

5. [x] Subcommand 47: set mesh shading mode.

   Payload: little-endian 16-bit mesh ID followed by an unsigned mode byte.
   Mode 0 is perspective-textured and mode 1 is flat palette. A valid command
   may establish the mesh before its uploads; an invalid or truncated mode
   must not create or alter a mesh.

6. [x] New controls initialize deterministically: direction
   `(0, +1, -1)` normalized, intensity 127, ambient 0, illumination enabled,
   and mesh mode 0. Control deletion and recreation restore those defaults.
7. [x] Subcommand 42 remains reserved for the historical
   `camera_track_object(oid;)` command. Reusing it would make old payloads
   desynchronize the current stream, so this implementation deliberately
   leaves 42 unassigned.
8. [x] Existing render-completion subcommand 41 is unchanged.
9. [x] Companion assembly emitters for subcommands 43–47 have been added to
   `~/Agon/mystuff/pingoasm/apps/_common/vdu_pingo.inc` and qualified through
   the standalone fixture and master-build integration tests.

## 4. Flat-palette renderer path

1. Shading mode is stored on the mesh. All objects sharing that mesh share its
   rendering method, while an object's existing UV override can still select
   its own per-triangle colors.
2. Mode 0 retains the current perspective-correct texture path.
3. Mode 1 records the source triangle's first UV before clipping. The texture
   lookup is delayed until the first clipped primitive survives projection,
   backface, and degeneracy rejection, then performed once for that source
   triangle. Every generated fan triangle carries the same color.
4. Flat fragments retain the established clipping, coverage, depth,
   illumination, RGBA2222 working-pixel, and optional RGBA8888 expansion paths.
   They skip reciprocal-W texture recovery, perspective UV advancement, and
   per-fragment texture lookup.
5. Flat-palette mode and illumination are independent. A flat mesh is lit by
   default and becomes native-color flat shading when illumination is disabled.
6. Firmware deliberately trusts the uploaded UV data. Build tooling must reject
   a flat source triangle if its three encoded UVs resolve to different palette
   cells; silently accepting a hand-edited multi-color triangle would make the
   first-corner sampling rule conceal malformed assets.

## 5. Validation record and remaining gates

1. [x] The command-surface guard accepts the intentionally extended surface:
   TurboVega commands 1–38 and 40, render notification 41, and new commands
   43–47. Command 42 remains absent.
2. [x] The full byte-stream bridge test covers initialization defaults, signed
   direction decoding and normalization, intensity, ambient, enable/disable,
   mesh mode, invalid values, truncated fixed payloads, next-command recovery,
   deletion/reinitialization, and allocation balance.
3. [x] Direct renderer diagnostics cover a textured triangle that crosses the
   clip volume, a flat source triangle producing two clipped primitives with a
   single color, elimination of reciprocal-W rejects in flat mode, native-color
   illumination bypass, unity ambient, and saturating overdrive.
4. [x] Pixel tests cover unity shade, overdrive saturation, darkness, and alpha
   preservation.
5. [x] The initial command guard, ordinary native `smoke` suite, and direct
   diagnostic renderer test passed during implementation.
6. [x] Rerun the complete ordinary and diagnostic native suites after final
   code cleanup. Both suites pass.
7. [x] Run the direct-renderer and full-module ASan/UBSan suites. Both pass.
8. [x] Complete ordinary, diagnostic, and compile-time-unlit ESP32 builds.
   All three pass with 42,552 bytes of RAM; flash use is 1,074,877,
   1,077,665, and 1,074,165 bytes respectively.
9. [x] Finish and qualify the companion `pingoasm` palette asset, conversion
   tests, fixture, and selected master-build integration. The palette tests
   pass 13 cases, fixture tests pass 9, master-build integration tests pass 12,
   and both explicit regeneration and ordinary real `ez80asm` assembly pass.
10. [x] Qualify Cube on physical hardware: default light placement, each
    axis-facing normal, intensity/ambient limits, native unlit colors, flat
    palette colors, and clipping. The physical result matched the accepted
    emulator fixture panel-for-panel.
11. [ ] Compare a dense model such as EarthUV in textured and flat modes to
    measure the work avoided by constant face color.
12. [x] Validate one flat mesh instantiated by two simultaneous objects. Both
    inherit the mesh-owned mode, while distinct object UV overrides select
    distinct constant face colors.
13. [x] Refresh and visually validate the bespoke current-Pingo emulator. At
    the author's explicit request this occurred before the deferred hardware
    pass. All four panels and both mesh modes were accepted provisionally.
    The changes remain uncommitted and unpushed.
14. [x] Run the new application against the candidate module in a disposable,
    silent emulator process without changing a profile or snapshot. The four
    render commands completed in order. Every 160x120 RGBA2222 target was
    nonblank and had a distinct color hash; all four depth hashes were
    identical, as expected for unchanged geometry. Direct target dumps showed
    both Cubes, constant-color flat faces, side-light response, saturating
    overdrive, and native colors with illumination disabled.

## 6. Existing-mesh conversion tooling

1. [x] A companion script now exists at
   `~/Agon/mystuff/pingoasm/build/scripts/flat_palette.py`.
2. [x] It verifies that `src/blender/colors64.png` is exactly the canonical
   8×8 Agon palette in row-major index order before converting anything.
3. [x] For each source triangle it samples covered source texels, ignores fully
   transparent samples, quantizes visible RGB to the nearest Agon color, and
   selects the modal palette index. Ties select the lowest index; a triangle
   with no visible sample selects index 0. Very small UV triangles use a
   deterministic centroid fallback.
4. [x] It writes a new, provenance-labeled OBJ whose three UV corners for each
   face select a safe interior point of the chosen palette cell. It preserves
   the source geometry and does not modify the authoritative OBJ or texture.
5. [x] Its validation helpers resolve the final encoded 16-bit UV words using
   Pingo's endpoint, row-direction, and truncation rules, then reject any
   source triangle whose three corners resolve to different cells.
6. [x] Add focused automated tests for palette verification, cell-center
   encoding, transparent handling, modal/tie behavior, malformed triangles,
   generated provenance, and assembly-table validation.
7. [x] Generate and qualify the canonical one-byte RGBA2222 palette fixture;
   do not create a competing palette definition.
8. [x] Integrate validation into the selected fixture/profile build and prove
   that malformed hand edits fail the build.

The packed palette is exactly 64 bytes and has SHA-256
`858b708a30584790b8255450819ffdca148a60150a31ba35be3ff8f70f92f130`.
The fixture lives at `~/Agon/mystuff/pingoasm/apps/lighting-shading` and uses
the established camera pose `(0, 0, +3200)`, looking toward the origin along
`-Z`.

A disposable scalability check also converted and round-trip validated the
960-triangle authoritative EarthUV OBJ against its existing PNG texture. It
selected 12 palette indices without modifying either source asset. This proves
the migration path is not Cube-specific; comparative dense-model rendering and
timing remain a later hardware task.

## 7. Acceptance note on light direction

1. The explicitly approved default surface-to-light vector remains
   `(0, +1, -1)`.
2. With the established camera convention, the canonical camera is on the
   scene's `+Z` side. Therefore `(0, +1, -1)` points upward and toward the far
   or forward side of the scene, not toward the camera.
3. Earlier wording asking for a light “behind the camera” is geometrically
   inconsistent with the later approved numeric vector. This implementation
   gives precedence to the explicit vector. The emulator Cube fixture has now
   passed visual review with that sign, and physical hardware subsequently
   reproduced the emulator result exactly to the author's eye.
4. The diffuse term deliberately retains Pingo's established half-Lambert
   response, `(1 + dot) / 2`, rather than changing to standard Lambert. The
   explicit ambient setting is a floor on that retained response.

## 8. Related optimization backlog

1. Backface culling already rejects projected clockwise primitives before
   bounding-box setup and rasterization. There is no unimplemented, distinct
   backface-culling algorithm currently approved.
2. Integer area/sign normalization remains a possible hot-path cleanup, but it
   changes coverage arithmetic rather than deciding which faces are backs.
3. Remaining substantial renderer investigations include division-free row
   advancement, further interpolant work, transformed indexed-vertex caching,
   clear/output cost measurement, and dedicated shared-edge/depth-tie tests.
4. Object-AABB and whole-triangle frustum rejection already exist in the
   current renderer, despite stale unchecked items in the older `pingoasm`
   roadmap. That roadmap should be reconciled separately rather than treated
   as an exact statement of current implementation state.
