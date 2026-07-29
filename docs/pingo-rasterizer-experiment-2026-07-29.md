# Pingo rasterizer experiment — 2026-07-29

## Status

The current experimental head is `b87c95e` on
`experiment/hecker-rasterizer`. It is an emulator-qualified candidate awaiting
hardware timing and human visual qualification. It is not a release and has
not replaced the `working-pre-hecker` baseline.

The hardware was deliberately left on the exact known-good pre-Hecker
firmware. Two attempts to obtain the execution environment's privileged
upload approval timed out before PlatformIO was started; no unattended flash
occurred.

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

## Rejected or deferred experiments

1. `1.0f / (float)area` removes a costly software-double path, but changed the
   final color target in one near-plane frame (`bmid=1257`, `seq=362`). It was
   rejected and reverted.
2. Sharing one perspective reciprocal between U and V changed the color target
   in 9 of 1,447 frames. All z-buffer hashes remained identical. This is
   texel-boundary rounding drift, so the candidate was rejected and reverted.
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

## Next gate

1. Build the ordinary candidate from clean source and verify its SHA.
2. Flash only after the serial listener is stopped.
3. Restart capture, reset the eZ80, and run the complete 1,447-record chain
   three times.
4. Reject on any malformed record, sequence discontinuity, reboot, crash,
   timeout, or material fixture regression.
5. Have the author visually qualify the moving fixtures on hardware before
   promoting or tagging the candidate.
6. Exercise an RGBA8888 textured hardware fixture separately; the unattended
   suite is deliberately RGBA2222-only.
