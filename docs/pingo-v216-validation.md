# Pingo VDP 2.16 Port and Fab Validation

Date: 2026-07-26

Status: the compatibility port is implemented and builds for ESP32 and native
x86-64. The exact current `moveair/jet.bin` runs under Fab without crashing
and has passed a live human visual/interactive smoke test. A deterministic
framebuffer baseline and physical-hardware qualification are still
deliberately pending.

This is the implementation evidence ledger for
[Decision Record 0001](decisions/0001-pingo-fab-vdp216-strategy.md). It records
what was changed, which exact revisions and artifacts were tested, why the
Fab repository was not forked, and why the modern image has not yet replaced
the known-good Alpha 7 image on hardware.

## Durable revision structure

The temporary worktree paths may be discarded; the refs and commits live in
this repository:

| Ref or commit | Purpose |
| --- | --- |
| `archive/pingo-alpha7` at `47a6609bf3d2409cb49a08ff93b22bd569476cd4` | Exact hardware-verified Alpha 7 source |
| `pingo-v2.16` feature commit `72b17cc251aa74b39ac3d6af8d5871b585d9e0d9` | Pingo compatibility port on official VDP 2.16.0 |
| `d8e8bfa` | Import the 16-file Pingo renderer runtime and its license |
| `72b17cc` | Add the narrow Pingo protocol bridge to official VDP 2.16 |
| `pingo-v2.16-userspace` adapter commit `d0bb3e13c876a9465c5ba19d8d53b97424eca5fa` | Native Fab adapter and tests |
| `da1d3d4` | Merge parents `7bcf28e` and `72b17cc` without changing Fab |
| `d0bb3e1` | Compile and load Pingo as an external Fab VDP module |

The refs are local and have not been pushed.

The hardware feature branch contains only the coherent Alpha 7 runtime and
protocol integration on top of official VDP 2.16.0. It does not replay the
109-commit experimental branch history. The port:

- preserves buffered command selector `0x49`;
- preserves extended solid RGBA2222 bitmap selector `0x22`;
- keeps parser internals private and grants the Pingo control narrow friend
  access;
- guards malformed buffered commands before consuming their parameters;
- excludes historical assets and the host-only `obj2vdu.c` utility from the
  firmware import.

The userspace branch merges that feature with Tom Morton's exact
`emulator-console8` revision and adds only the host boundary:

- all 16 Pingo translation units compile as C11;
- the VDP wrapper compiles as C++17;
- all 29 required userspace-vdp-gl translation units compile as C++11 into a
  local, recreated archive;
- ESP32 and host PSRAM allocation meet behind `pingo_platform_alloc/free`;
- the unused `esp_dsp.h` dependency is removed;
- `-Wl,-z,defs` rejects unresolved shared-library symbols;
- build products stay under ignored `video/build/userspace`.

## ESP32 build evidence

Both comparisons were clean builds with PlatformIO Core 6.1.19,
Espressif32 6.6.0, Arduino-ESP32 2.0.14, and the same
`vdp-gl` revision `ac2dd598`.

| Measurement | Stock VDP 2.16.0 `c7ac293` | Pingo VDP 2.16 `72b17cc` | Delta |
| --- | ---: | ---: | ---: |
| PlatformIO static RAM | 44,688 bytes | 44,704 bytes | +16 |
| PlatformIO application flash | 1,077,429 bytes | 1,097,393 bytes | +19,964 |
| Application-flash utilization | 82.2% | 83.7% | +1.5 points |
| `firmware.bin` | 1,077,552 bytes | 1,097,776 bytes | +20,224 |
| ELF text | 781,145 bytes | 800,521 bytes | +19,376 |
| ELF data | 296,285 bytes | 297,129 bytes | +844 |
| ELF BSS | 25,169 bytes | 25,185 bytes | +16 |

The Pingo image leaves 213,327 bytes of the configured 1,310,720-byte
application partition unused. The measured result closely matches the
historical Alpha 7 cost and establishes that capacity is not the current
blocker.

Artifact identities:

```text
stock firmware.bin
  1,077,552 bytes
  SHA-256 28bd53fb4cea1ee3e772522ece5722d8b9e94d4725a2fb9e255e911578df7779

Pingo firmware.bin
  1,097,776 bytes
  SHA-256 f44a5aa034c5c38c26dc8e7da3b81d96c4b00f4b11f9fd60b6a1eefc2d7ad089
```

The only ESP32 build warning was the inherited Arduino UART missing-return
warning. No physical flash was performed from `pingo-v2.16`; the Agon still
has the known-good Alpha 7 firmware.

## Native adapter evidence

The native adapter was built from scratch on x86-64 with GCC/G++ 13.3.0:

```text
video/build/userspace/vdp_pingo.so
  10,664,312 bytes
  SHA-256 716127d32f4b2aac12fe872f4797bbfe6848cd703aa45a982607ccd8ed608ecc
```

`ldd -r` reported no missing relocations. The shared object exports all 15
entry points loaded by Fab's `VdpInterface`, and the smoke harness now checks
all 15 with `RTLD_NOW` before exercising Pingo. It also verifies the unmangled
Pingo renderer symbol.

The tracked smoke test:

1. initializes the native VDP and completes the eZ80 poll handshake;
2. creates RGBA2222 bitmap 257 at 64x64;
3. creates Pingo control buffer 1000;
4. renders an empty scene;
5. retrieves a 640x480 framebuffer at 59.94 Hz.

The final result was:

```text
Pingo native smoke passed: 640x480 at 59.94 Hz
```

The build intentionally leaves one Pingo warning visible:
`shade()` can fall through without returning when a texture exists but its
pixel pointer is null. Fixing that inherited undefined path belongs in the
post-compatibility hardening series, not in the transplant.

## Exact Jet compatibility run

The full-client test used only these current companion artifacts:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `jet.bin` | 57,944 | `de9202f5713d04f01df43d67bb2a7503df7bd938b0e199817d9a4514d03d9af9` |
| `jet.rgba2` | 262,144 | `b9756411fb1785a3e2a8c1a9e225017b8cbe317b9ac0ead3b2cfed4523475c40` |
| `fsimpanel.rgba2` | 29,440 | `53ca5a440747a7c92854d6c263bb0f9d563aa12636e0f2f4cc538dad50d11fb1` |

A disposable SD profile used CRLF `autoexec.txt` lines:

```text
SET KEYBOARD 1
cd /moveair
load jet.bin
run
```

Fab ran for 15 seconds with dummy video and audio, the software renderer,
absolute MOS and VDP paths, and the external library whose hash is recorded
above. Exit status 124 was the deliberate timeout, not a fault. The log
showed:

- Pingo initialization at 320x148;
- object, mesh, material, texture, and transform commands;
- repeated subcommands 141, 145, 42, and 38;
- repeated `Render to 320x148` output through the full interval.

The emulator did not crash or reset. This passes protocol and liveness gate 5
for the exact current client. It does **not** establish pixel correctness:
Fab has no screenshot or frame-dump option, and SDL's dummy driver exposes no
window to inspect.

The reproducible build, disposable SD profile, headless command, expected
markers, and safety constraints are in `userspace/README.md` on the
`pingo-v2.16-userspace` branch.

### Live visual run

The same external library and disposable SD profile were then launched in a
Wayland Fab window with the software renderer. The log confirmed the absolute
Pingo VDP path, completed Jet initialization, and continued rendering the
320x148 viewport while the Author exercised the application. The Author
reported the test successful.

This is the first direct visual/interactive confirmation of Jet on the modern
port. It proves that the emulator presents a usable scene rather than merely
surviving the command stream. It is still a smoke test, not a deterministic
equivalence oracle: no reference screenshot or framebuffer hash was captured.

## Fail-closed emulator procedure

Fab tries the requested `--vdp` path and then may fall back to
`./firmware/vdp_console8.so`. A broken external library can therefore produce
a false pass when Fab is launched from its own checkout.

The validated procedure launches Fab from a directory with no local
`firmware` fallback, supplies absolute `--mos` and `--vdp` paths, enables
`--verbose`, and confirms the printed VDP path. A deliberately invalid
external path then fails fatally instead of loading stock VDP.

Use a fresh Fab process for every Jet run. Alpha 7 teardown is empty, control
buffer 100 can retain borrowed bitmap pointers, and recreating the scene in
one process is not a valid lifecycle test. Do not press Space: Jet uses it to
cycle dithering, and the inherited dither paths write four-byte pixels through
a packed one-byte RGBA2222 target.

## Upstream checkout integrity

No source, submodule ref, build file, or SD content was changed in
`/home/smith/Agon/fab-agon-emulator`. Its pre-existing state remained:

```text
main 98bbb392b75b196171cc620b60839220e5ce53ed
 M sdcard
 M src/audio.rs
 M src/main.rs

vdp-console8       7bcf28e0a2376e32328a6a5554d0df852b75c80e (clean)
userspace-vdp-gl   ecb8aabffe97d66d6c6eb30a8a08e09932dbe2e4 (clean)
```

The companion `pingoasm` checkout also remained clean.

## Validation gate ledger

| Gate | Result |
| --- | --- |
| Stock official VDP 2.16 ESP32 build | Passed |
| Clean native stock VDP rebuild | Not separately captured; installed Fab stock module boots |
| Native external Pingo VDP load and ABI | Passed |
| 64x64 native bitmap/control/render smoke | Passed |
| Exact current `moveair/jet.bin` under Fab | Passed for protocol and 15-second liveness |
| Live human visual/interactive Jet smoke | Passed |
| Deterministic framebuffer comparison | Pending |
| Stock VDU workload on the modern Pingo port | Pending |
| Modern Pingo ESP32 build and resource audit | Passed |
| Modern Pingo image on physical hardware | Pending |
| Robustness corrections | Intentionally not started |

## Decision at this checkpoint

Do not fork Fab yet. The external `--vdp` seam is sufficient for compilation,
ABI, protocol, liveness, and native crash diagnostics, and it keeps durable
changes in the Author's VDP fork. A Fab fork becomes justified when one of
these is an actual deliverable:

- deterministic framebuffer capture or frame CRC;
- timed/headless graceful exit and keyboard injection;
- a packaged Pingo firmware profile;
- a repeatable outer build that pins and distributes all native libraries.

Do not flash the modern image yet. The next smallest useful gate is a
deterministic framebuffer capture using a simple scene and Jet, now anchored
by the successful live visual run. Once that passes, flash `pingo-v2.16`, run
stock VDU workloads first, and only then run the Pingo demos. Alpha 7 remains
the immediate recovery image throughout.

Do not mix inherited correctness fixes into this compatibility checkpoint.
Missing-target handling, ownership and teardown, texture bounds, the
missing-return path, and packed-pixel dithering should become small,
individually tested commits after emulator visual parity and hardware
qualification.
