# Pingo rasterizer experiment — 2026-07-29

## Status

The retained renderer is code commit `b87c95e` on
`experiment/hecker-rasterizer`. It passed exact emulator color/depth
comparison, two complete physical-hardware timing runs, and the author's
hardware visual review. It is a successful experimental milestone, not yet a
release or a claim that the Hecker investigation is complete.

The connected hardware now contains the ordinary candidate whose SHA-256 is
recorded below. A deliberately repeated pre-Hecker capture first proved that
the benchmark is stable enough to distinguish real changes: three baseline
runs differed by only 0.003 ms in their all-frame means.

## Baseline and correctness oracle

The stepping-off point is `cb91c12`, tagged `working-pre-hecker`. The fixed
suite contains 15 programs and 1,447 frames:

1. bitmap 1257: 580 contiguous records, sequences `0..579`;
2. bitmap 1410: 867 contiguous records, sequences `0..866`; and
3. eight stationary models, four near-plane paths, and three multi-object
   camera/orbit scenes.

Commit `dac14df` added an emulator-only FNV-1a target hash and a strict stream
comparator. Commit `d9830b4` extended the gate to the complete 32-bit z-buffer.
Two fresh baseline processes produced exactly the same 1,447 color and depth
records. The compact state oracle has SHA-256:

```text
abd2d61bb08d333923f7f79d17335267ba11e9754f0ece64feb567e252fdc11d
```

Hashing happens after the measured render interval and is compiled out of
ordinary emulator and embedded builds. See `userspace/README.md`.

## Retained exact-output changes

Every retained step was built separately and compared against all 1,447
baseline color and depth states before the next step was attempted.

| Commit | Change |
|---|---|
| `32cbc61` | Normalize the constant light once per object, not per triangle. |
| `ccc2139` | Compose view/projection directly from renderer fields, avoiding two 64-byte copies. |
| `8f32ece` | Cache the backend z-buffer pointer once per object. |
| `0dedd85` | Fuse depth comparison and write, converting depth only once. |
| `60ab518` | Inline the fused depth operation. |
| `1beb287` | Replace four libc `fminf`/`fmaxf` sampler calls with equivalent NaN-safe clamps. |
| `0fce076` | Inline default/custom pixel-backend dispatch. |
| `6c8c62b` | Inline native RGBA2222 shading. |
| `f0a9ce4` | Reuse the linear fragment index and write default-backend pixels directly. |
| `e185772` | Replace three per-pixel floating shade operations with a four-level table prepared once per triangle. |
| `db1b949` | Inline the texture sampler while retaining RGBA2222 and RGBA8888 input support. |
| `e68cc76` | Add a clean-room, exhaustively tested integer edge-span primitive. |
| `6aa02bb` | Bound each raster row by that exact span while retaining the old coverage predicate as a guard. |
| `b87c95e` | Remove the now-proven redundant per-pixel coverage predicate. |

The span work borrows the general edge-walking idea discussed by Chris Hecker,
not historical source code. The old branches use incompatible depth,
projection, UV, clipping, fill, and backend semantics; one imported historical
implementation also lacks a clear permissive license. No historical Hecker
code was copied.

The retained span convention intentionally matches this renderer rather than a
new top-left rule:

1. samples lie at integer `(x,y)`;
2. all zero-valued edges are included;
3. shared edges may therefore be drawn by both triangles;
4. both accepted area signs are supported; and
5. the existing half-open bounding-box maximum is retained.

The native span test exhaustively compares both windings over clipped small
triangles, adds 200,000 deterministic affine rows, covers integer-limit cases,
and passes with undefined-behavior sanitization.

## Deferred experiments

1. `1.0f / (float)area` removes a costly software-double path, but changed the
   final color target in one near-plane frame (`bmid=1257`, `seq=362`). It was
   reverted under the original exact-output policy. That policy was
   intentionally conservative: this candidate now deserves a hardware A/B
   test because its single changed frame may be an acceptable rounding trade.
   Its exact reusable diff is
   `docs/experiments/float-area-reciprocal.patch`.
2. Sharing one perspective reciprocal between U and V changed the color target
   in 9 of 1,447 frames. All z-buffer hashes remained identical, identifying
   texel-boundary rounding rather than geometry or occlusion changes. It was
   reverted, but eliminating one software floating division per shaded
   textured fragment could justify those few boundary differences. Its exact
   reusable diff is
   `docs/experiments/shared-perspective-reciprocal.patch`.
3. Copying immutable texture metadata into an object-local sampler snapshot
   was exact, but showed no persuasive timing signal in one noisy emulator run.
   It was reverted rather than retained without evidence.
4. A clean-room rational edge walker was derived and exhaustively verified
   outside the repository. It replaces per-row quotients with at most six
   setup quotients per triangle. Xtensa inspection shows the present helper is
   already fully inlined and uses native `quou`, not a software division
   helper. The rational walker is therefore deferred until hardware data says
   row quotient cost matters.
5. Incremental depth, reciprocal-W, U/W, and V/W remain deferred. Repeated
   floating addition can alter depth and texel boundaries and must be
   introduced one attribute at a time.

## Automatic qualification

At `b87c95e`:

1. all 1,447 final color and z-buffer states match `working-pre-hecker`
   exactly;
2. ordinary native ABI, texture, math, span, and dual-target smoke tests pass;
3. detailed native renderer diagnostics pass;
4. ordinary and diagnostic PlatformIO builds pass;
5. the TurboVega command-surface scope check passes;
6. embedded RAM remains 42,520 bytes;
7. ordinary flash usage is 1,065,489 bytes; and
8. diagnostic flash usage is 1,067,645 bytes.

The ordinary candidate firmware has SHA-256:

```text
6f68c4da62ce4e1df98eb41036fb7f25b1dd0763e58e9d99ac5639d194603c70
```

## Emulator performance screening

These figures compare one ordinary `working-pre-hecker` run with three fresh
ordinary candidate processes. They are useful for deciding what deserves a
hardware flash; they are not hardware performance claims.

| Fixture | Baseline µs | Candidate µs | Change |
|---|---:|---:|---:|
| Cube | 581.8 | 236.8 | -59.30% |
| HeavyTank | 174.6 | 64.3 | -63.18% |
| EarthUV | 467.7 | 194.2 | -58.47% |
| Earth party ellipse | 673.9 | 472.1 | -29.94% |
| Cube near-plane | 788.7 | 293.1 | -62.84% |
| EarthUV near-plane | 715.6 | 305.2 | -57.35% |
| Jet near-plane | 393.9 | 201.2 | -48.91% |
| Airliner near-plane | 887.7 | 498.6 | -43.83% |
| EarthIco | 279.7 | 130.5 | -53.32% |
| Lara | 213.2 | 78.6 | -63.12% |
| Crash | 113.3 | 78.4 | -30.87% |
| Jet | 175.5 | 109.9 | -37.39% |
| Airliner | 307.0 | 272.8 | -11.14% |
| Earth party | 684.9 | 472.3 | -31.04% |
| Earth party dolly | 585.4 | 436.4 | -25.46% |

The pooled mean fell from 586.38 to 370.27 microseconds, a 36.86% emulator
reduction. Candidate process means were 371.4, 384.9, and 354.5 microseconds,
which also demonstrates why hardware repeats and per-fixture distributions
remain mandatory.

## Hardware qualification

The author flashed the verified ordinary candidate and judged the complete
physical display sequence visually correct. Two fresh complete captures each
contained:

1. 1,447 records;
2. bitmap 1257 sequences `0..579`;
3. bitmap 1410 sequences `0..866`; and
4. no malformed record, gap, reset, panic, or timeout.

The comparison uses three newly repeated `working-pre-hecker` runs and the two
candidate runs. Equivalent FPS is `1,000,000 / mean render_us`; it measures the
instrumented renderer interval, not application logic, display transfer, or
buffer presentation.

| Fixture | Baseline FPS | Exact-span FPS | FPS gain | Render-time change |
|---|---:|---:|---:|---:|
| Cube | 4.92 | 9.25 | +88.17% | -46.86% |
| HeavyTank | 13.73 | 20.64 | +50.32% | -33.48% |
| EarthUV | 6.94 | 10.62 | +52.90% | -34.60% |
| Earth party ellipse | 5.72 | 7.66 | +33.94% | -25.34% |
| Cube near-plane | 3.80 | 7.54 | +98.61% | -49.65% |
| EarthUV near-plane | 4.43 | 7.92 | +78.90% | -44.10% |
| Jet near-plane | 9.49 | 15.07 | +58.77% | -37.01% |
| Airliner near-plane | 5.19 | 8.28 | +59.56% | -37.33% |
| EarthIco | 10.46 | 17.14 | +63.88% | -38.98% |
| Lara | 20.42 | 23.89 | +16.99% | -14.52% |
| Crash | 21.96 | 24.04 | +9.51% | -8.68% |
| Jet | 21.84 | 24.58 | +12.55% | -11.15% |
| Airliner | 11.90 | 13.63 | +14.59% | -12.73% |
| Earth party | 5.88 | 7.72 | +31.29% | -23.83% |
| Earth party dolly | 7.36 | 8.84 | +20.03% | -16.69% |
| **All frames, weighted** | **6.49** | **9.12** | **+40.59%** | **-28.87%** |

The result validates the edge-span direction: benefits are largest for Cube
and the near-plane workloads, where bounding boxes contain many pixels outside
the triangle. Smaller but repeatable gains on every fixture also show that the
supporting invariant and hot-path work did not merely optimize one synthetic
case.

Raw captures, the two-version comparison utility, and its standalone HTML/SVG
report live in `~/Agon/mystuff/pingoasm/benchmarks/render-spin` and
`~/Agon/mystuff/pingoasm/build/scripts`.

## Implementation-ready next experiments

Each experiment begins at `b87c95e`; do not stack candidates until their
individual hardware effects are known. Preserve the current ordinary firmware
as the rollback image.

### 1. Single-precision triangle-area reciprocal

Apply:

```bash
git apply docs/experiments/float-area-reciprocal.patch
```

This changes one reciprocal prepared per rasterized triangle from an
unsuffixed double expression to an explicit float expression. Inspect the
Xtensa image to confirm the double helper disappeared. Run native and embedded
tests, then the exact emulator comparison. The expected exception is the
already identified color hash at bitmap 1257 sequence 362; depth and every
other state must remain unchanged. Capture two ordinary hardware runs and
retain the candidate only if its measured gain and human image review justify
that single-frame rounding difference.

### 2. One perspective division per textured fragment

Apply, independently:

```bash
git apply docs/experiments/shared-perspective-reciprocal.patch
```

The current shader computes:

```c
u = uNumerator * areaInverse / oneOverW;
v = vNumerator * areaInverse / oneOverW;
```

The patch computes `perspectiveScale = areaInverse / oneOverW` once and
multiplies both numerators by it. This removes one software `__divsf3` call
from every depth-passing textured fragment. The known emulator result changes
only nine color targets and no z-buffer targets. Produce pixel-difference
images for those nine frames, inspect whether differences are isolated
one-texel boundary choices, then hardware-test it twice. Test the combined
area-plus-perspective patch only after both independent results are recorded.

### 3. Division-free row advancement

`triangleRowSpanFind()` currently performs up to three native Xtensa `quou`
instructions for each nonempty scanline. A clean-room rational edge walker was
derived and stress-tested outside the repository but has not been integrated.
For each winding-normalized affine edge `e + dx*x >= 0`:

1. `dx > 0` gives the inclusive lower bound
   `x >= ceil(-e / dx)`;
2. `dx < 0` gives the exclusive upper bound
   `x < floor(e / -dx) + 1`;
3. `dx == 0` admits the whole row when `e >= 0`, otherwise no pixels;
4. store floor quotient, nonnegative remainder, divisor, quotient step, and
   remainder step for each nonflat edge;
5. advance one row with quotient/remainder addition and one carry, without a
   row division; and
6. intersect all raw 64-bit bounds with `[0,rowWidth)` before converting to
   `uint32_t`.

Use at most six setup quotients per triangle—two per nonflat edge—and preserve
integer sampling, inclusive zero edges, double-owned shared edges, both
windings, clipping, and half-open bbox maxima. Extend
`pingo_triangle_span_test` with walker-versus-current-span equivalence before
renderer integration. Hardware data, especially short-triangle workloads,
must decide whether setup overhead is worthwhile.

### 4. Incremental fragment attributes

Only after the simpler reciprocal candidates are measured, advance depth,
`1/W`, `U/W`, and `V/W` across spans using precomputed X/Y gradients. Introduce
one attribute family per commit. Repeated floating addition can drift from the
present barycentric recomputation, so retain color and 32-bit z-buffer hashes,
identify the first divergent frame, and never mix a fill-rule change into the
same experiment. A fixed-point form is a later candidate, not the initial
reference.

### 5. Indexed transformed-vertex cache

EarthUV repeatedly transforms indexed source vertices once per triangle
corner. A cache could transform each used position once per object render, but
this is more invasive:

1. establish a reliable vertex count; the present `Mesh` does not expose
   `positions_count`, so either extend its construction contract or derive and
   validate `max(pos_indices)+1`;
2. allocate/cache transformed positions with explicit internal-RAM/PSRAM
   accounting;
3. invalidate on object, camera, projection, mesh, or scene transform changes;
4. keep model-space positions available for the existing lighting convention;
   and
5. measure memory and speed separately on EarthUV and the multi-object scenes.

Do not begin this until fragment-loop experiments have a clear stopping point.

## Remaining qualification limits

1. The unattended chain is RGBA2222-only. Exercise an RGBA8888 textured
   fixture before claiming full legacy texture compatibility.
2. Add deliberate single-triangle viewport-edge, shared-edge crack, and
   depth-tie fixtures before changing coverage ownership.
3. Hardware visual review remains mandatory for every arithmetic candidate
   whose hashes differ, even when the difference count is small.
4. Update the bespoke emulator only after hardware acceptance and obtain the
   author's explicit emulator validation before committing emulator changes.
