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

The smoke target first runs seven direct native tests:

1. `pingo_texture_test`, which verifies the one-byte Pingo working pixel, all
   256 RGBA2222 packed values, RGBA8888-to-RGBA2222 quantization and stride,
   and the qualified UV row direction;
2. `pingo_math_test`, which covers guarded vector normalization, identity,
   translation-only and general matrix inversion, and sequential versus
   composed view/projection transforms;
3. `pingo_triangle_span_test`, which exhaustively compares the row-span
   primitive with the renderer's current inclusive edge equations across both
   windings, clipped viewports, randomized rows, and integer-limit cases;
4. `pingo_perspective_span_test`, which checks the perspective span's
   fixed-block interpolation and numerically stable remainder path;
5. `pingo_depth_test`, which checks endpoint and representative `[0,1]`
   float-to-`uint32_t` depth mappings, adjacent representable values,
   rejection of out-of-range and nonfinite values, and agreement between the
   fused and split depth APIs;
6. `pingo_mesh_validation_test`, which checks out-of-order mesh assembly,
   triplet and index bounds, nonfinite positions and UVs, geometry validity,
   mesh- versus object-owned texture coordinates, and texture-index/count
   cross-validation; and
7. `pingo_clip_test`, which checks exact pass-through, one- and two-vertex
   near-plane crossings, exact and adjacent near boundaries, all six
   homogeneous clip planes, bounded output, interpolated UVs, and safe
   rejection of nonfinite input.

It then runs two full-module harnesses. `pingo_smoke` loads the module with
immediate symbol resolution, starts the native VDP, creates 64×64 RGBA2222 and
RGBA8888 target bitmaps, renders an empty scene to both, and verifies that Fab
exposes a live framebuffer.

`pingo_bridge_robustness_test` drives the real VDU byte stream and covers:

1. object and scene single-axis Z-scale subcommands;
2. invalid dimensions and deterministic failures at every initialization
   allocation;
3. texture pinning across bitmap clear and same-ID replacement, failed and
   successful explicit rebinding, and subsequent rendering;
4. explicit, generic single-buffer, global, repeated, and populated control
   teardown with exact owned-allocation accounting;
5. rejection of copied or aliased control bytes, generic call/jump execution,
   control-backed bitmaps/render targets, and unsafe generic mutation;
6. complete-payload draining after allocation rejection; and
7. bounded recovery from truncated uploads, including the maximum legal
   16-bit element count, followed by an immediately valid command.

The bridge harness has a 30-second outer timeout. General-poll and
render-completion packets provide command barriers; the test does not infer
success from sleeps alone.

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
macro. The current candidate's closed diagnostic schema is version 4. The
counter test covers exact mesh-AABB caching and invalidation; conservative
eight-corner object rejection at every common clip plane; exact and
just-outside object boundaries; crossing bounds; rotation, scene hierarchy,
nonuniform negative scale, nonfinite/unordered bounds and disabled-culling
fail-open behavior, and byte-identical output between object rejection and the
triangle fallback. It also retains the direct triangle tests for empty and
ordinary frames, projected-Z, whole-frustum, and backface rejection; strict
clip boundaries and crossing triangles; all-nonpositive W; integer-screen
degeneracy; bounding-box clamp; overdraw; depth-range and depth-buffer
rejection; frame-to-frame reset; unsigned clock wrap; and the published
object, source-triangle, generated-primitive, and fragment invariants.

The version-4 diagnostic cases additionally exercise the production
projection matrix's near-side rejection semantics, near crossings with either
one or two vertices outside, a huge laterally crossing triangle, nonfinite
projection input, and generated fan counts without unsafe projection. These
are deterministic native correctness checks. The combined candidate
subsequently passed focused emulator visual review and Olimex hardware
qualification on 2026-07-29.
See `docs/pingo-render-diagnostics.md` for the record schema and hardware
workflow.

This is an ABI and command-path smoke test, not visual qualification. It does
not inspect the Pingo target bitmap. After hardware passes, qualify a copied
module in a fresh Fab process with the strict `cube` and `heavytank` fixtures
from `~/Agon/mystuff/pingoasm/apps/turbovega`, using the isolated profile
documented in `~/Agon/mystuff/pingoasm/README.md`. That separate repository is
the canonical home of all Pingo assembly test fixtures; this VDP repository
contains firmware and native-module qualification code only.

The persistent comparison emulator must snapshot the resulting shared object.
It must not symlink directly to this build output, because later
`pingo-codex` builds will replace that file.

## Bridge robustness sanitizer

Run the full VDU module and bridge harness under ASan and UBSan with:

```bash
make -C userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator \
  bridge-sanitizer-test
```

This uses the isolated ignored directory
`video/build/userspace-bridge-sanitizer`, instruments both the shared module
and harness, and retains the 30-second outer timeout. Two narrowly scoped host
adapter exceptions are required by the current Fab checkout:

1. `userspace/bridge_ubsan.supp` suppresses only the pre-existing invalid-bool
   load in `dispdrivers/vgabasecontroller.cpp`; and
2. `alloc_dealloc_mismatch=0` tolerates Fab's pre-existing userspace PSRAM
   `malloc`/`delete[]` mismatch.

Neither exception suppresses Pingo ownership checks. The harness separately
counts every Pingo-owned allocation and requires return to the exact baseline
after failure, teardown, isolation, and truncation cases.

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
