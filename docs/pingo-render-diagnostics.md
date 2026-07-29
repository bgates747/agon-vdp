# Pingo render diagnostics

Pingo has an opt-in, compile-time renderer-attribution mode. It emits one
versioned `PINGO_RENDER` record for each successful command-38 render while
leaving the ordinary firmware build and its established record prefix intact.

This mode answers questions such as whether a workload is dominated by
triangle transformation, raster bounding boxes, overdraw, or legacy RGBA8888
output expansion. Hardware remains the performance ground truth.

## Build modes

1. `PINGO_RENDER_DIAGNOSTICS=0` is the default. The detailed fields, counters,
   phase clocks, and their runtime overhead are compiled out.
2. `PINGO_RENDER_DIAGNOSTICS=1` enables the version-1 diagnostic schema.
   There is no runtime VDU toggle: every successful command-38 render made by
   that firmware build emits the detailed record.
3. Native diagnostic objects live under
   `~/Agon/mystuff/agon-vdp/video/build/userspace-diagnostics`, separately from
   the ordinary `video/build/userspace` objects. This prevents a change of
   compiler flags from silently reusing incompatible object files.
4. Embedded diagnostic artifacts live in PlatformIO environment
   `esp32dev-pingo-diag`. Ordinary `pio run` still selects `esp32dev` because
   that remains `default_envs`.
5. The define must be identical in the C renderer and C++ VDP bridge. The
   diagnostic fields are conditionally present in `Renderer`; compiling those
   translation units with different settings would create a structure-layout
   mismatch.

The implementation is concentrated in:

1. `video/pingo/render/renderer.h` — diagnostic structure, clock contract, and
   renderer-owned per-frame state;
2. `video/pingo/render/renderer.c` — phase timing and geometry/fragment
   counters;
3. `video/pingo_3d.h` — platform clock, command-level phases, schema
   serialization, and debug output;
4. `userspace/pingo_renderer_diagnostics_test.c` — deterministic native
   counter and reset tests;
5. `userspace/Makefile` — isolated native build mode; and
6. `platformio.ini` — isolated embedded build environment.

## Version-1 record

The diagnostic build preserves the established first three fields and appends
a closed, versioned schema:

```text
PINGO_RENDER seq=<u32> bmid=<u16> render_us=<u32> d=1 w=<u16> h=<u16> fmt=<2|8> cmd=<u32> pre=<u32> clr=<u32> xf=<u32> ts=<u32> ras=<u32> out=<u32> ob=<u32> ti=<u32> tz=<u32> tf=<u32> td=<u32> to=<u32> tr=<u32> tv=<u32> pt=<u64> pc=<u64> pz=<u64> pd=<u64> pu=<u64> ps=<u64>
```

All values are unsigned decimal integers. Time fields are microseconds. The
abbreviations keep the one-record-per-frame serial cost manageable.

### Identity and timing fields

| Field | Meaning |
| --- | --- |
| `seq` | Full control-local render sequence. The callback protocol carries only its low 16 bits. |
| `bmid` | Command-38 output bitmap ID. |
| `render_us` | Existing independent monotonic measurement around `rendererRender()` only. In a diagnostic build it necessarily includes instrumentation overhead inside that call. |
| `d` | Diagnostic schema version; exactly `1` for this schema. |
| `w`, `h` | Pingo render width and height. |
| `fmt` | Target bitmap bits per RGBA channel: `2` for RGBA2222 or `8` for RGBA8888. |
| `cmd` | Valid command-38 handler entry, before reading `bmid`, through target-bitmap finalization and restoration of Pingo's private frame pointer. It is measured with the independent 64-bit microsecond clock and saturated to `u32`. |
| `pre` | After output-bitmap validation through the instant before `rendererRender()`: target selection, renderer/scene construction, object binding and modified transforms, projection, camera-view inversion, and scene transform. It is measured with the independent 64-bit microsecond clock and saturated to `u32`. |
| `clr` | Renderer entry through both buffer clears: depth clear, backend `beforeRender`, framebuffer-pointer refresh, and optional color clear. |
| `xf` | Per-triangle source lookup, model transformation, normal and diffuse-light calculation, view transformation, and projection. |
| `ts` | Per-triangle rejection and setup after projection: exact projected-Z test, perspective division, projected-winding test, screen coordinates, bounding-box clamp, integer-area test, barycentric setup, and perspective-UV preparation. |
| `ras` | Per-triangle fragment loops: edge tests, depth work, texture interpolation and sampling, illumination, pixel writes, and diagnostic counter maintenance. An empty clamped bounding box returns before this phase and contributes no raster time. |
| `out` | Output finalization after `rendererRender()` through target readiness. RGBA8888 targets include full one-byte-to-four-byte expansion; RGBA2222 targets require only final bookkeeping and frame-pointer restoration. It is measured with the independent 64-bit microsecond clock and saturated to `u32`. |

The timed phases do not form a perfect sum:

1. `render_us` uses a separate monotonic clock from the detailed tick
   accumulators.
2. `clr + xf + ts + ras` omits scene traversal, renderable dispatch,
   `afterRender`, and small instrumentation gaps inside `rendererRender()`.
3. `pre + render_us + out` omits part of command parsing and validation and can
   differ from `cmd` because the clocks and rounding differ.
4. The host summarizer reports these differences as renderer-unattributed and
   command-unattributed time rather than silently assigning them to a phase.

### Object and triangle fields

| Field | Meaning |
| --- | --- |
| `ob` | Object render calls reached. Scenes and sprites are not included. |
| `ti` | Indexed triangle triplets submitted by those objects. |
| `tz` | Triangles rejected by the renderer's exact projected-Z predicate: all three post-projection, pre-division Z values are greater than zero. This counter must not be interpreted as near-plane clipping. |
| `tf` | Triangles rejected by the projected floating-point winding/back-face predicate. |
| `td` | Triangles that survive the winding test but collapse to zero area after conversion to integer screen coordinates. |
| `to` | Triangles rejected because their viewport-clamped bounding box is empty. |
| `tr` | Triangles with a non-empty clamped bounding box whose raster loop is entered. |
| `tv` | Triangles whose screen-space bounding box was changed by viewport clamping. This is an independent tag, not a rejection category or geometric clipping operation. |

The triangle partition is:

```text
ti = tz + tf + td + to + tr
```

`tv` may overlap `to` or `tr` and is deliberately absent from that sum.

### Fragment fields

| Field | Meaning |
| --- | --- |
| `pt` | Candidate fragment positions in all non-empty, viewport-clamped triangle bounding boxes. This is the rasterizer's bounding-box workload, not visible area. |
| `pc` | Candidates passing the triangle edge/coverage test, counted before depth-range testing. |
| `pz` | Covered fragments rejected because interpolated depth is outside `[0,1]`. |
| `pd` | Covered, in-range fragments rejected by the depth buffer. |
| `pu` | Textured, depth-passing fragments rejected because interpolated reciprocal W is exactly zero. The current renderer has already written depth before this check. |
| `ps` | Fragments reaching `backendDrawPixel()`, whether textured or using the untextured fallback color. |

The fragment partition and bound are:

```text
pc = pz + pd + pu + ps
pc <= pt
```

These definitions make several comparisons direct:

1. `pc / pt` measures how much bounding-box work becomes triangle coverage.
2. `pd / pc` measures depth-test overdraw.
3. `ps / pc` measures the covered-fragment share that reaches shading and
   output.
4. `xf + ts`, interpreted alongside `ti`, shows geometry/setup pressure.
5. `out`, interpreted alongside `fmt`, isolates legacy target expansion.
6. `pt`, `pc`, and `ps` distinguish a low-poly object that covers much of the
   screen from a high-poly object with smaller projected triangles.

## State and emission invariants

1. The renderer clears the entire diagnostic structure at the beginning of
   every `rendererRender()` call. Counts and phase totals describe one frame,
   never lifetime accumulation.
2. The injected clock pointer and frequency survive that per-frame reset.
3. A valid, completed command-38 render produces exactly one debug record.
4. A missing, malformed, incorrectly sized, or unsupported output bitmap
   returns before rendering and produces no record or completion callback.
5. Diagnostic mode sends the opt-in render-completion callback immediately
   after target readiness and before formatting the longer debug record.
6. Formatting and transmitting the debug line are outside every reported
   timing interval.
7. Ordinary builds retain the shorter compatible record:

   ```text
   PINGO_RENDER seq=<u32> bmid=<u16> render_us=<u32>
   ```

8. Schema version 1 is closed. A record containing `d=1` must contain each
   version-1 field exactly once and no unknown fields. Adding, removing, or
   redefining fields requires a new schema version and parser support.

The callback wire ABI and application-side interrupt contract are separate
from this debug schema; see `docs/pingo-render-completion.md`.

## Clocks and measurement overhead

Detailed renderer phases use an injected, wrapping 32-bit tick counter so the
C renderer remains platform-neutral:

1. ESP32 uses the Xtensa cycle counter at `F_CPU`, currently 240 MHz. It wraps
   in approximately 17.9 seconds.
2. Native userspace uses the low 32 bits of a steady-clock microsecond count at
   1 MHz. It wraps in approximately 71.6 minutes.
3. Unsigned subtraction handles one wrap correctly. Only an individual clear,
   per-triangle transform, per-triangle setup, or per-triangle raster interval
   must finish within one tick-counter wrap. Per-frame phase totals accumulate
   those intervals in 64-bit fields and may span a wrap.
4. The accumulated tick totals are converted to microseconds once per frame
   using an overflow-safe quotient/remainder calculation and are saturated to
   `u32`.
5. `cmd`, `pre`, and `out` do not use the wrapping phase clock. They use the
   independent 64-bit monotonic microsecond clock and saturate their serialized
   results to `u32`.
6. `render_us` continues to use the pre-existing monotonic microsecond clock
   around `rendererRender()`, independently of the detailed phase ticks.

The diagnostic firmware is intentionally observant rather than
performance-neutral:

1. It reads clocks at every triangle phase boundary.
2. It maintains several counters in the fragment hot loop.
3. `ras` explicitly includes fragment-counter overhead. At a contiguous phase
   boundary, the helper samples the ending tick and reuses it as the next
   phase's start; the small amount of accumulator/bookkeeping work after that
   sample is consequently charged to the following phase. A terminal rejection
   or final raster boundary has no following timed phase, so its post-sample
   bookkeeping is unattributed.
4. Those operations add instructions, alter register pressure, and can affect
   cache behavior.
5. The long serial line can delay processing of the next queued VDP command
   even though its formatting and transmission are not timed.
6. Phase shares and counters may be compared across models built and exercised
   with the same diagnostic firmware.
7. Absolute release performance must be measured with ordinary firmware or a
   qualified archived firmware image. Do not compare diagnostic
   `render_us` directly with a release baseline and call the difference a
   renderer regression.
8. Emulator or native timing is useful for functional regression only. It is
   not an ESP32 performance substitute.

## Native verification

From anywhere, build the diagnostic module and run all native checks:

```bash
make -C ~/Agon/mystuff/agon-vdp/userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator \
  DIAGNOSTICS=1 \
  diagnostics-smoke
```

The isolated diagnostic module is:

```text
~/Agon/mystuff/agon-vdp/video/build/userspace-diagnostics/vdp_pingo.so
```

`diagnostics-smoke` performs:

1. the one-byte pixel and dual-format texture test;
2. the native shared-module ABI and command-path smoke test, including
   RGBA2222 and RGBA8888 render targets and render-completion behavior; and
3. `pingo_renderer_diagnostics_test`, which uses an injected deterministic
   clock and an 8×8 fake backend.

The direct renderer test covers:

1. an empty scene and nonzero clear timing;
2. a front-facing triangle and nonzero transform, setup, raster, coverage, and
   shade results;
3. a second render proving per-frame counters reset rather than accumulate;
4. reversed winding and back-face rejection;
5. two overlapping triangles and a nonzero depth-test rejection count;
6. exact projected-Z rejection;
7. integer-screen degeneracy independently of back-face rejection;
8. an offscreen empty-bounding-box rejection and a separately rasterized,
   viewport-clamped triangle;
9. covered fragments rejected by the `[0,1]` depth range;
10. unsigned elapsed-tick arithmetic across a 32-bit clock wrap; and
11. both triangle and fragment partition invariants.

Run the ordinary, non-instrumented native smoke independently when required:

```bash
make -C ~/Agon/mystuff/agon-vdp/userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator \
  smoke
```

If a clean diagnostic rebuild is needed, clean the same isolated variant:

```bash
make -C ~/Agon/mystuff/agon-vdp/userspace \
  DIAGNOSTICS=1 \
  clean
```

Native tests establish structure, arithmetic, state reset, command plumbing,
and ABI loading. They do not visually qualify a render and do not qualify
hardware timing.

## Embedded build and flash

The debug listener owns the same tty needed by PlatformIO. The hardware
procedure is:

1. Stop `listen_vdp_debug.py` with Ctrl+C.
2. Clean the diagnostic PlatformIO environment:

   ```bash
   ~/Agon/mystuff/agon-vdp/.venv/bin/pio run \
     --project-dir ~/Agon/mystuff/agon-vdp \
     -e esp32dev-pingo-diag \
     --target clean
   ```

3. Build the diagnostic firmware:

   ```bash
   ~/Agon/mystuff/agon-vdp/.venv/bin/pio run \
     --project-dir ~/Agon/mystuff/agon-vdp \
     -e esp32dev-pingo-diag
   ```

4. Flash the connected Agon:

   ```bash
   ~/Agon/mystuff/agon-vdp/.venv/bin/pio run \
     --project-dir ~/Agon/mystuff/agon-vdp \
     -e esp32dev-pingo-diag \
     --target upload
   ```

5. Restart the listener before resetting or running the selected benchmark.
6. Visually verify the fixture while capturing the records. A numerically
   plausible report does not supersede visual correctness.

The ordinary `esp32dev` and diagnostic `esp32dev-pingo-diag` build directories
are separate. Always name the diagnostic environment explicitly when building
or flashing an attribution run.

## Hardware capture and summarization

The reconnecting listener tees identical raw VDP debug bytes to the terminal
and an append-only log. A typical Cube capture is:

```bash
~/Agon/mystuff/pingoasm/build/scripts/listen_vdp_debug.py \
  --log ~/Agon/mystuff/pingoasm/benchmarks/render-spin/results/cube-pingo-diag.log
```

Then:

1. Run the fixture manually, or press `R` in the listener terminal when the
   hardware SD `autoexec.txt` already runs it.
2. Allow all five profile-declared series to finish. Each contains eight
   warmups and 36 measured frames.
3. Use Ctrl+C to stop capture.
4. Preserve the raw log beside any derived JSON result.
5. Parse it with the matching benchmark profile:

   ```bash
   ~/Agon/mystuff/pingoasm/.venv/bin/python \
     ~/Agon/mystuff/pingoasm/build/scripts/summarize_render_benchmark.py \
     ~/Agon/mystuff/pingoasm/benchmarks/render-spin/profiles/cube-rgba2222.json \
     ~/Agon/mystuff/pingoasm/benchmarks/render-spin/results/cube-pingo-diag.log \
     --platform hardware \
     --firmware "Pingo diagnostic build" \
     --require-diagnostics \
     --json-output \
       ~/Agon/mystuff/pingoasm/benchmarks/render-spin/results/cube-pingo-diag.json
   ```

The summarizer:

1. retains compatibility with ordinary `PINGO_RENDER` records;
2. selects the latest complete profile-declared suite of warmup/measured
   bitmap-ID signatures;
3. ignores malformed or truncated records outside that selected run, which
   permits capture through resets and reconnects;
4. rejects malformed selected records or a selected run mixing ordinary and
   diagnostic records;
5. validates schema completeness, version, dimensions, format, partitions, and
   coverage bounds;
6. reports existing render statistics;
7. reports phase totals, means, and shares plus unattributed time;
8. aggregates object, triangle, and fragment counters; and
9. derives coverage, depth-rejection, and shading ratios.

Use `--require-diagnostics` for an attribution run so accidentally flashed
ordinary firmware fails loudly rather than yielding a conventional timing
summary.

## Physical-display boundary

The widest current interval, `cmd`, ends when command 38 has made its selected
bitmap ready. It is not an end-to-end display-latency measurement:

1. It begins only when the VDP command handler executes. Serial transmission,
   command framing, and time already spent waiting in the VDP input queue are
   excluded.
2. Command 38 writes a bitmap. A later select, draw, flip, or buffered-command
   operation that places that bitmap on screen is a separate command and is
   excluded.
3. FabGL canvas work, VSYNC waiting, VGA scanout, monitor resynchronization, and
   photon arrival are excluded.
4. The optional completion packet is sent only after target readiness, but
   packet transmission, MOS interrupt dispatch, and application callback
   latency are excluded from `cmd`.
5. The debug record is emitted after the completion request and is also
   excluded.

Measuring presentation or scanout would require a separately designed
downstream marker tied to the display/VSYNC path. These diagnostics must not be
described as measuring physical frame presentation.
