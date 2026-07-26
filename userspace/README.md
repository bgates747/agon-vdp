# Pingo native VDP build

This directory builds the Pingo-enabled VDP as a native shared object for
Fab Agon Emulator. It does not modify Fab or any of Fab's submodules.

The build deliberately keeps the language boundary used by the firmware:

- Pingo's 16 math and rendering translation units compile as C;
- the Agon VDP and Pingo protocol wrapper compile as C++;
- `pingo_host_platform.cpp` exposes a two-function C ABI that routes Pingo
  allocations through Fab's simulated ESP32 PSRAM allocator.

## Prerequisites

`FAB_ROOT` must name a Fab checkout with initialized `userspace-vdp-gl`
submodules. The compatibility baseline for this branch is:

- Fab Agon Emulator `98bbb392b75b196171cc620b60839220e5ce53ed`;
- userspace-vdp-gl `ecb8aabffe97d66d6c6eb30a8a08e09932dbe2e4`;
- Fab VDP adaptations `7bcf28e0a2376e32328a6a5554d0df852b75c80e`.

Build the shared object:

```sh
make -C userspace FAB_ROOT=/absolute/path/to/fab-agon-emulator
```

The output is ignored by Git and written to:

```text
video/build/userspace/vdp_pingo.so
```

Run the native ABI and empty-scene smoke test:

```sh
make -C userspace \
  FAB_ROOT=/absolute/path/to/fab-agon-emulator \
  smoke
```

The smoke test loads the shared object with immediate symbol resolution,
starts the native VDP, creates RGBA2222 bitmap 257, creates Pingo control
buffer 1000, renders an empty 64x64 scene, and reads the emulator framebuffer.

Set `PINGO_CAPTURE_PREFIX` to a fresh absolute path to capture an exact Pingo
render target. The native-only hook writes packed RGBA2222 bytes, a viewable
RGB PPM preview, and a metadata file immediately after rendering:

```sh
PINGO_CAPTURE_PREFIX=/tmp/pingo-frame \
  PINGO_CAPTURE_FRAME=1 \
  make -C userspace \
    FAB_ROOT=/absolute/path/to/fab-agon-emulator \
    smoke
```

This produces `/tmp/pingo-frame.rgba2`, `/tmp/pingo-frame.ppm`, and
`/tmp/pingo-frame.txt`, then prints a machine-readable CRC32 record to
standard error. The raw `.rgba2` file is the regression oracle; the PPM drops
alpha bits and is only a visual preview. Raw pixels are tightly packed in
top-to-bottom row order with a top-left origin and Agon's `AABBGGRR` bit
layout. The metadata identifies the checksum as CRC-32/ISO-HDLC.

`PINGO_CAPTURE_FRAME` defaults to 1 and counts Pingo renders, not Fab display
refreshes; a set value must be a positive decimal integer. No final output path
may already exist. PID-scoped temporary files are created exclusively, and
publication never replaces an existing path. The `.txt` file is published
last and is the completeness marker: ignore `.rgba2` or `.ppm` without it,
since a failed multi-file publication can leave an incomplete final set. The
hook is disabled when `PINGO_CAPTURE_PREFIX` is unset and compiles to a no-op
in ESP32 builds.

The hook captures Pingo's target bitmap immediately after the selected render
and configured dithering path. It does not capture later VDU bitmap plotting,
composition, scaling, or Fab's final 640x480 scanout.

Fab can load the result without an emulator fork:

```sh
/absolute/path/to/fab-agon-emulator/target/release/fab-agon-emulator \
  --vdp /absolute/path/to/agon-vdp/video/build/userspace/vdp_pingo.so
```

## Exact `jet.bin` compatibility run

The first full-client gate uses exactly the current `moveair/jet.bin` and its
two runtime assets. Create a disposable SD-card directory:

```sh
FAB_ROOT=/absolute/path/to/fab-agon-emulator
PINGO_VDP_ROOT=/absolute/path/to/agon-vdp
MOVEAIR_ROOT=/absolute/path/to/pingoasm/src/asm/moveair
JET_SD="$(mktemp -d)"

mkdir -p "$JET_SD/moveair"
cp "$MOVEAIR_ROOT/jet.bin" "$JET_SD/moveair/"
cp "$MOVEAIR_ROOT/jet.rgba2" "$JET_SD/moveair/"
cp "$MOVEAIR_ROOT/fsimpanel.rgba2" "$JET_SD/moveair/"
printf 'SET KEYBOARD 1\r\ncd /moveair\r\nload jet.bin\r\nrun\r\n' \
  > "$JET_SD/autoexec.txt"
```

Run Fab from the Pingo VDP checkout, not from the Fab checkout:

```sh
cd "$PINGO_VDP_ROOT"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout --signal=INT 15s \
  "$FAB_ROOT/target/release/fab-agon-emulator" \
  --renderer sw \
  --firmware console8 \
  --mos "$FAB_ROOT/firmware/mos_console8.bin" \
  --vdp "$PINGO_VDP_ROOT/video/build/userspace/vdp_pingo.so" \
  --sdcard "$JET_SD" \
  --verbose -z
```

Exit status 124 is expected when `timeout` ends a healthy 15-second run.
Successful execution prints repeated Pingo subcommands and `Render to 320x148`
messages. This headless test proves command-stream stability but not visual
correctness.

Fab falls back to `./firmware/vdp_console8.so` when an explicit `--vdp`
library cannot be loaded. Running from a directory without that fallback and
using absolute `--mos` and `--vdp` paths makes a bad Pingo library fail
instead of silently exercising stock VDP. Confirm the path printed by
`--verbose` before accepting a result.

## Alpha 7 compatibility limits

Use a fresh VDP process for each Pingo client run. Alpha 7 keeps raw pointers
to bitmap storage, does not implement teardown, and cannot safely recreate its
control buffer after a client replaces bitmap 257.

Do not enable dithering yet. The inherited dither routines treat the packed
one-byte RGBA2222 target as four-byte RGBA and write beyond the bitmap.

The render target must be RGBA2222 bitmap 257 and must exist before the Pingo
control command. These constraints are preserved temporarily so the existing
`moveair/jet.bin` remains the compatibility oracle.
