# Pingo native VDP

This directory builds the `pingo-codex` VDP as a native shared object for Fab
Agon Emulator. It is build plumbing only: the embedded and native targets
compile the same Pingo renderer, math library, Agon bridge, and TurboVega
command surface.

`FAB_ROOT` must name the owned Fab checkout with initialized
`userspace-vdp-gl` submodules:

```bash
make -C userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator
```

The ignored output is:

```text
video/build/userspace/vdp_pingo.so
```

Run the ABI and empty-scene smoke test with:

```bash
make -C userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator \
  smoke
```

The smoke target first runs:

1. `pingo_texture_test`, which verifies the one-byte Pingo working pixel, all
   256 RGBA2222 packed values, RGBA8888-to-RGBA2222 quantization and stride,
   and the qualified UV row direction; and
2. `pingo_math_test`, which covers guarded vector normalization, identity,
   translation-only and general matrix inversion, and sequential versus
   composed view/projection transforms.

It then loads the module with immediate symbol resolution, starts the native
VDP, creates 64×64 RGBA2222 and RGBA8888 target bitmaps, renders an empty scene
to both, and verifies that Fab exposes a live framebuffer.

Build the compile-time diagnostic variant and run its deterministic renderer
counter tests with:

```bash
make -C userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator \
  DIAGNOSTICS=1 \
  diagnostics-smoke
```

Diagnostic and ordinary objects use separate ignored build directories, so
switching variants cannot silently reuse objects compiled with the other
macro. The counter test covers empty and ordinary frames, projected-Z,
whole-frustum, and backface rejection; strict clip boundaries and crossing
triangles; all-nonpositive W; enabled/disabled output equivalence; integer-screen
degeneracy; bounding-box clamp; overdraw; depth-range and depth-buffer
rejection; frame-to-frame reset; unsigned clock wrap; and the published
counter partitions.
See `docs/pingo-render-diagnostics.md` for the record schema and hardware
workflow.

This is an ABI and command-path smoke test, not visual qualification. It does
not inspect the Pingo target bitmap. After hardware passes, qualify a copied
module in a fresh Fab process with the strict `cube` and `heavytank` fixtures
from `pingoasm/apps/turbovega`, using the isolated profile documented in
`pingoasm/README.md`.

The persistent comparison emulator must snapshot the resulting shared object.
It must not symlink directly to this build output, because later
`pingo-codex` builds will replace that file.

## Exact render-target comparison

Optimization experiments can add an emulator-only hash after each completed
Pingo render:

```bash
make -C userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator \
  BUILD_DIR="$PWD/video/build/userspace-target-hash" \
  CPPFLAGS=-DPINGO_RENDER_TARGET_HASH=1
```

`PINGO_TARGET` records contain the render sequence, bitmap ID, byte counts, and
64-bit FNV-1a hashes of both the final target bitmap and z-buffer. Hashing
occurs after the timed renderer interval. It is compiled out unless both
`USERSPACE` and `PINGO_RENDER_TARGET_HASH=1` are defined, so embedded and
ordinary emulator builds pay no cost.

Validate one log or require a candidate to match a baseline exactly:

```bash
scripts/compare_pingo_target_hashes.py \
  --expected-stream 1257:580 \
  --expected-stream 1410:867 \
  baseline.log candidate.log
```

Use `--extract oracle.txt` to retain only the canonical `PINGO_TARGET` records.
The comparator also rejects missing, duplicate, reordered, or non-contiguous
records. A passing comparison establishes exact color and depth-buffer
equivalence; it does not replace final visual and hardware qualification.
