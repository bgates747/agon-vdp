# Pingo rasterizer experiment — 2026-07-29

## Status

The retained edge-span renderer change is commit `b87c95e` on
`experiment/hecker-rasterizer`; its complete qualified stepping-off state is
commit `641d80d`. It passed exact emulator color/depth comparison, two complete
physical-hardware timing runs, and the author's hardware visual review. It is
a successful experimental milestone, not a release or a claim that the Hecker
investigation is complete.

The branch now integrates the qualified unconditional subdivided-affine
checkpoint described below on top of `641d80d`. The connected hardware
contains its byte-qualified firmware. A deliberately repeated pre-Hecker
capture first proved that the benchmark is stable enough to distinguish real
changes: three baseline runs differed by only 0.003 ms in their all-frame
means.

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

1. `1.0f / (float)area` removes a costly software-double path once per
   rasterized triangle. Exact target dumps corrected the earlier hash-only
   assessment: bitmap 1257 sequence 362 changes one color pixel and 4,582
   stored depth values; sequence 366 preserves color but changes 15,527 depth
   values. The depth changes are only one or two fixed-point units. Two
   hardware runs showed a `+0.972%` weighted FPS gain over the shortened
   543-frame chain, with no observed visual regression. This candidate is
   shelved rather than retained: its gain is modest and its numerical effect
   is broader than the color hashes first suggested. Its reusable diff remains
   `docs/experiments/float-area-reciprocal.patch`.
2. Sharing one perspective reciprocal between U and V removes one software
   float division per depth-passing textured fragment. Exact target dumps show
   nine changed pixels among 111,129,600 examined: exactly one RGBA2222 pixel
   in each of nine frames, with every z-buffer value bit-exact. Two shortened
   hardware-chain runs showed a `+6.195%` weighted FPS gain and no observed
   visual regression. This candidate is selected for retention, subject to the
   final combined/full-suite qualification policy. Its reusable diff is
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

### Short-chain arithmetic-candidate results

During development, a shortened chain preserves the original stream sequence
numbers while running only Cube, HeavyTank, EarthUV, Earth-party camera
ellipse, Cube near-plane, and EarthUV near-plane. It contains 543 frames:
bitmap 1257 sequences `0..253` and bitmap 1410 sequences `0..288`. This makes
its timings directly comparable with the same prefixes of the exact-span
full-suite capture.

Two independent hardware runs of the shared-perspective-reciprocal candidate
produced:

| Fixture | Exact-span FPS | Shared-reciprocal FPS | FPS gain |
|---|---:|---:|---:|
| Cube | 9.252 | 10.275 | +11.051% |
| HeavyTank | 20.636 | 21.833 | +5.800% |
| EarthUV | 10.616 | 11.288 | +6.327% |
| Earth party ellipse | 7.661 | 7.924 | +3.432% |
| Cube near-plane | 7.540 | 8.488 | +12.580% |
| EarthUV near-plane | 7.921 | 8.685 | +9.651% |
| **All 543 frames, weighted** | **8.272** | **8.784** | **+6.195%** |

The two-run weighted means differed by only 1.61 microseconds. The
hardware-tested binary has SHA-256:

```text
4dcac8c6a3ee574cf6a07cf4c2e187959e05aab5cdf6f989665d9ac82e1a0589
```

### Incremental-depth candidate

The first incremental-attribute experiment advances only depth. The first
pixel in every exact row span retains the original barycentric calculation;
later pixels use one floating addition:

```c
const float depthStepX =
    -(A12 * (a.z - c.z) + A20 * (b.z - c.z)) *
    areaInverse;
```

This stable difference form is algebraically equivalent to the direct
three-term expression because `A12 + A20 + A01 == 0`. It avoids subtracting
nearly equal large products while retaining the same one-addition fragment
loop. It was built independently from exact-span commit `641d80d`; it does not
contain the area-reciprocal or shared-perspective-reciprocal candidates.

Full target comparison found widespread low-order depth drift from repeated
floating addition, but representative frames changed only 0–14 final color
pixels. An emulator visual-review sequence appeared to omit slightly less than
the bottom quarter of several frames. The byte-exact exact-span baseline
showed the same stable omission with the identical emulator and SD-card
configuration, while the incremental candidate rendered complete frames on
physical hardware. The lower-strip omission is therefore an emulator
presentation artifact, not evidence of candidate target corruption.

Two physical-hardware runs were visually correct and measured:

| Fixture | Exact-span FPS | Incremental-depth FPS | FPS gain |
|---|---:|---:|---:|
| Cube | 9.252 | 9.481 | +2.470% |
| HeavyTank | 20.636 | 20.949 | +1.517% |
| EarthUV | 10.616 | 10.766 | +1.411% |
| Earth party ellipse | 7.661 | 7.735 | +0.954% |
| Cube near-plane | 7.540 | 7.749 | +2.782% |
| EarthUV near-plane | 7.921 | 8.092 | +2.157% |
| **All 543 frames, weighted** | **8.272** | **8.397** | **+1.516%** |

Every fixture improved, and the two candidate runs were highly repeatable.
The tested image has SHA-256:

```text
40a3ed45be45178ec61f95c150d69ba01a5313541f038e04c486374ad69c8c4e
```

The direct three-term step expression was then tested separately:

```c
const float depthStepX =
    -(A12 * a.z + A20 * b.z + A01 * c.z) *
    areaInverse;
```

It was also visually correct on hardware and improved the weighted chain by
`+1.479%` over exact span. A direct two-run comparison against the difference
form produced:

| Fixture | Difference-form FPS | Direct-sum FPS | Direct-sum change |
|---|---:|---:|---:|
| Cube | 9.481 | 9.474 | -0.07% |
| HeavyTank | 20.949 | 20.929 | -0.10% |
| EarthUV | 10.766 | 10.756 | -0.09% |
| Earth party ellipse | 7.735 | 7.733 | -0.02% |
| Cube near-plane | 7.749 | 7.745 | -0.06% |
| EarthUV near-plane | 8.092 | 8.089 | -0.03% |
| **All 543 frames, weighted** | **8.397** | **8.394** | **-0.04%** |

The difference is too small to distinguish from measurement noise, but the
direct sum was fractionally slower in every fixture and offers no numerical
advantage. Retain the stable difference form.

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
tests, then the exact emulator comparison. The measured exceptions are the
color/depth state at bitmap 1257 sequence 362 and the depth state at sequence
366; every other state must remain unchanged. Capture two ordinary hardware
runs and retain the candidate only if its measured gain and human image review
justify those rounding differences.

### 2. One perspective division per textured fragment

Apply, independently:

```bash
git apply docs/experiments/shared-perspective-reciprocal.patch
```

This independent experiment was completed on 2026-07-29. The original shader
computes:

```c
u = uNumerator * areaInverse / oneOverW;
v = vNumerator * areaInverse / oneOverW;
```

The patch computes `perspectiveScale = areaInverse / oneOverW` once and
multiplies both numerators by it. This removes one software `__divsf3` call
from every depth-passing textured fragment. Full target dumps established that
the nine changed color targets contain exactly one changed pixel apiece and
that all depth targets remain exact. Hardware testing then measured a
`+6.195%` weighted FPS gain over 543 frames. The area-reciprocal result did not
justify automatically stacking the two candidates.

The exact hardware-tested image and evidence are archived outside all Git
working trees at:

```text
~/Agon/mystuff/pingo-firmware-archive/07-shared-perspective-reciprocal-candidate
```

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

### 4. Naive subdivided-affine texture mapping — active next experiment

The next experiment implements the consequential part of Hecker-style
subdivided affine mapping, not another standalone edge-walker optimization.
It begins in an isolated checkout at exact-span commit `641d80d`; it does not
stack the shared-perspective-reciprocal, incremental-depth, float-area, or
rational-row-walker candidates.

For a textured triangle, define the three perspective attributes:

```text
q = 1/W
s = U/W
t = V/W
```

The renderer already retains reciprocal clip-space W in `a.w`, `b.w`, and
`c.w`, and already premultiplies each texture coordinate by that value.
Prepare constant X gradients from the existing barycentric X steps:

```text
dq/dx = (A12*a.w   + A20*b.w   + A01*c.w)   / area
ds/dx = (A12*tca.x + A20*tcb.x + A01*tcc.x) / area
dt/dx = (A12*tca.y + A20*tcb.y + A01*tcc.y) / area
```

Because `A12 + A20 + A01 == 0`, implement the algebraically equivalent stable
difference form:

```text
dq/dx = (A12*(a.w   - c.w)   + A20*(b.w   - c.w))   / area
ds/dx = (A12*(tca.x - tcc.x) + A20*(tcb.x - tcc.x)) / area
dt/dx = (A12*(tca.y - tcc.y) + A20*(tcb.y - tcc.y)) / area
```

This mirrors the numerically preferable form already selected by the
incremental-depth experiment and does not change the mathematical pipeline.

At the exact beginning of each row span, evaluate `q`, `s`, and `t` with the
existing barycentric expression. Advance those values across the span rather
than recomputing them from three barycentric products for every fragment.

Use an eight-pixel subdivision:

1. recover perspective-correct `U = s/q` and `V = t/q` at the start of a
   block;
2. advance `q`, `s`, and `t` to the next block boundary;
3. recover perspective-correct U and V there;
4. linearly interpolate U and V across the intervening pixels; and
5. carry that recovered right boundary forward as the next block's left
   boundary; and
6. repeat until the span is exhausted.

Recover the row's first boundary once, then perform only one new reciprocal
at each subsequent valid boundary. Never recalculate a shared boundary. This
is an essential part of the subdivided-affine saving: a long span requires
one initial reciprocal plus approximately one reciprocal per eight pixels,
not two reciprocals per block.

Always leave a final group of 1–8 pixels whose last endpoint is an actual
covered pixel. For a tail of `n > 1` pixels, recover U/V at the first and last
pixels and divide their difference by `n - 1`; for a one-pixel tail, use the
first endpoint directly. Earlier full groups use the exact boundary eight
pixels ahead and divide by eight. This avoids consulting an endpoint beyond
the triangle on the final group.

This first implementation is deliberately unconditional and naive:

1. every textured span uses this pipeline, regardless of triangle area, span
   length, distance, curvature, clipping, or expected cost;
2. there is no dispatch back to the old exact-per-fragment UV mapper;
3. short spans still use the subdivided-affine tail rule;
4. U/V and their block increments remain floating point;
5. fixed-point arithmetic and ESP-DSP are explicitly deferred; and
6. reciprocal-W endpoint checks are safety/rejection handling, not an
   optimization policy or a second rasterization path.

If a block's required first or last reciprocal-W endpoint is exactly zero,
the block is invalid. Still perform the existing per-fragment depth range
check and depth test/write; for each depth-passing fragment in that block,
increment the existing reciprocal-W rejection diagnostic and omit only its
color write. Initialize all invalid-block outputs so the one-pixel and rejected
paths cannot consume an uninitialized U/V delta. An invalid left boundary does
not poison later blocks: if its right endpoint is valid, carry that recovered
endpoint forward so the next block can resume normally.

For a valid block, preserve the old exact `q == 0.0f` fragment rejection by
evaluating the original barycentric expression
`(w0*a.w + w1*b.w + w2*c.w) * areaInverse` after a successful depth write,
at the same point and in the same operation order as the exact mapper. Do not
substitute an incrementally accumulated q for this guard; floating-point
rounding can make the two zero tests disagree. This safety expression adds no
division. Texture advancement must not be skipped when depth rejects a pixel.
Do not epsilon-clamp a projective pole: this renderer currently lacks true
mixed-W eye-plane clipping, and inventing a finite value would conceal that
separate defect.

Keep the exact-span coverage primitive, barycentric depth calculation, depth
test/write order, clipping, illumination, texture sampling, pixel output, and
all VDU-visible behavior unchanged. In particular, do not combine incremental
depth with this test. Z-buffer targets must therefore remain bit-exact; color
differences isolate the texture-coordinate approximation.

Add focused native coverage for:

1. a span shorter than eight pixels;
2. exactly eight pixels;
3. one or more full blocks followed by a partial tail;
4. a single-pixel span;
5. constant and rapidly varying reciprocal W; and
6. reciprocal-W endpoint rejection without undefined arithmetic.

Qualification proceeds in this order:

1. native tests, UBSan, diagnostics, command-surface scope, and both ordinary
   and diagnostic PlatformIO builds;
2. compiler/code-size/stack inspection, including the number and placement of
   remaining floating divisions;
3. all 1,447 headless-emulator target comparisons, requiring exact z-buffer
   targets and a quantified report of every changed color target and pixel;
4. repeatable headless-emulator performance bracketing against the exact-span
   base; and
5. an interactive emulator review by the author before any integration,
   hardware flash, commit, or push.

The current signed `area = orient2d(a_s,b_s,c_s)` is already available without
extra work and equals twice the projected triangle area before viewport
clamping. It may inform a later hybrid, but it is not used by this experiment.
Area alone is not a sufficient future dispatch metric: block amortization is
more directly related to actual clipped span lengths and depth-passing
fragments, while approximation error is related to reciprocal-W curvature.
Only after this unconditional mapper is stable, visually acceptable, and
clearly faster will thresholds or a cheaper small-triangle path be designed.

Before any later fixed-point version, add diagnostics for the observed ranges
of `1/W`, `U/W`, and `V/W` across the full and near-plane suites. A global
signed 16-bit Q format may lose too much relative precision at the far plane
or overflow on triangles crossing the camera plane; a per-triangle shared
exponent may make 16-bit mantissas viable. Although ESP-DSP is already linked,
its kernels target arrays while this renderer consumes scalar spans. Benchmark
plain inline fixed-point advancement before considering a library call.

#### Initial implementation qualification

The unconditional eight-pixel implementation was built in an isolated
checkout from exact-span commit `641d80d`. It introduces a small
`perspective_span.h` helper whose explicit boundary object carries the
recovered right endpoint into the next block. The renderer computes stable
X gradients once per triangle, recovers the first boundary once per span,
and prepares every textured block through the same path. The original exact
barycentric reciprocal-W expression remains solely as the post-depth-write
projective-pole guard.

The following gates pass:

1. the focused partition, tail, interpolation, zero-endpoint, and
   invalid-boundary recovery tests;
2. UBSan with floating-point divide-by-zero detection;
3. the complete ordinary native smoke suite;
4. the diagnostic native smoke suite;
5. the Pingo VDU command-surface check;
6. ordinary and diagnostic PlatformIO builds; and
7. all 1,447 headless-emulator frames in the full regression suite.

The full-suite z-buffer hashes are bit-identical to the exact-span baseline.
The corrected boundary-carry implementation also produces exactly the same
1,447 color hashes as the initially captured subdivided-affine candidate.
Against exact perspective mapping, 1,162 frames change at least one texel,
but only 16,637 of 111,129,600 output pixels differ
(`0.014970809%`). The worst frame is Cube near-plane bitmap `1257`, sequence
`119`: 419 of 76,800 pixels differ (`0.545573%`). These are color-only
texture-sampling differences; coverage and depth remain unchanged.

The ordinary embedded build uses 42,520 bytes of RAM, unchanged from the
baseline, and 1,066,137 bytes of flash, an increase of 648 bytes. The
diagnostic build adds 912 bytes of flash and no RAM. Ordinary `renderObject`
stack use grows by 16 bytes; diagnostic stack use grows by 32 bytes.

Static Xtensa inspection finds no per-fragment software floating division
remaining. The exact mapper's two inner-loop U/W and V/W division sites are
replaced by:

1. recovery of the first span boundary;
2. recovery of each new block boundary; and
3. one setup division for the final 2–8-pixel tail.

The full-block `1/8` reciprocal is constant-folded. Replacing the seven
possible tail reciprocals with constants is intentionally deferred as an
independent micro-optimization.

The full native-emulator A/B/A bracket used three fresh processes per phase,
1,447 frames per process, and the same emulator, MOS, SD suite, and ordinary
VDP artifacts throughout:

| Phase | Firmware | Mean render | Equivalent FPS |
| --- | --- | ---: | ---: |
| A1 | exact-span baseline | 374.026 us | 2,673.61 |
| B | subdivided affine | 367.527 us | 2,720.89 |
| A2 | exact-span baseline | 365.785 us | 2,733.85 |

Against the mean of A1 and A2, B is 0.643% faster by render time and 0.647%
faster by equivalent FPS. That is not a demonstrated native-emulator gain:
the baseline itself drifted 2.203% in render time and 2.253% in FPS from A1
to A2, and B is 0.476% slower than A2 alone.

The stream split does not rescue a whole-host claim. Bitmap `1257` is 2.796%
faster than the midpoint baseline by render time, but its A1-to-A2 baseline
drift is larger at 3.209%. Bitmap `1410` is 0.087% slower than its midpoint
baseline while its baseline drift is 1.860%. Ordinal A/B/A pairing is
likewise unstable.

Drift-normalized fixture aggregation does show a credible geometry-dependent
redistribution: EarthIco is faster relative to its stream in all three
triplets (mean 15.1%), while Cube (mean 8.2%), stationary Jet, and stationary
Airliner are slower in all three. Per-frame effects are not reproducible:
1,060 of 1,447 frames change effect sign across triplets. This is evidence
that span or mesh shape may eventually matter to dispatch policy, not evidence
for a threshold yet.

This native result is not grounds to reject the embedded experiment. The
host executes floating division in hardware, whereas Xtensa inspection shows
the ESP32 build calls the software `__divsf3` helper. Removing two
per-fragment software divisions can therefore have a materially different
cost on hardware. Treat the emulator bracket as a stability and gross-
regression check; hardware benchmarking remains the performance ground truth.
The author subsequently reported the complete interactive emulator suite
visually correct. The byte-qualified candidate firmware
(`abc7c4e009341fc35d5d7345741cd22f8d0b40d95e77eb3633be1220be07aca1`)
then flashed successfully to hardware with every written region verified.
The author observed no texture distortion or tearing on hardware.

Two complete hardware runs of the 543-frame quick chain then passed with the
exact expected bitmap `1257` sequences `0..253`, bitmap `1410` sequences
`0..288`, and no malformed record, reset, panic, or timeout:

| Fixture | Exact-span FPS | Subdivided-affine FPS | FPS gain | Render-time change |
| --- | ---: | ---: | ---: | ---: |
| Cube | 9.25 | 11.03 | +19.21% | -16.11% |
| HeavyTank | 20.64 | 22.50 | +9.03% | -8.28% |
| EarthUV | 10.62 | 11.26 | +6.06% | -5.71% |
| Earth party ellipse | 7.66 | 7.87 | +2.66% | -2.59% |
| Cube near-plane | 7.54 | 9.28 | +23.02% | -18.71% |
| EarthUV near-plane | 7.92 | 9.04 | +14.11% | -12.37% |
| **All quick-chain frames, weighted** | **8.27** | **8.94** | **+8.06%** | **-7.46%** |

This table is immutable evidence from the then-current 543-frame quick chain.
After qualification, HeavyTank was removed from future abbreviated runs
because its smaller projected scale is less representative of the full-screen
near-plane rasterizer work being optimized. The revised quick chain contains
507 frames: bitmap `1257` sequences `0..217` and bitmap `1410` sequences
`0..288`. Because removing HeavyTank shifts later sequence identities, capture
a fresh exact-span baseline before comparing future 507-frame candidates; do
not reinterpret the historical 543-frame logs.

The candidate's two-run weighted-mean spread is only 0.716 microseconds
against a mean of 111,880.911 microseconds. Every fixture improves, and the
largest gains occur on Cube and the near-plane paths, consistent with
amortizing endpoint recovery over longer visible spans. The hardware result
therefore supplies the clear performance advantage that the noisy native
emulator bracket could not establish.

The raw candidate capture, immutable comparison JSON, and refreshed local
HTML dashboard live under
`~/Agon/mystuff/pingoasm/benchmarks/render-spin`. The candidate is now
integrated in the working tree. Ordinary and diagnostic native smoke suites,
the Pingo command-surface check, and both embedded PlatformIO builds pass.
This is the commit checkpoint; no subsequent optimization is included.

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
