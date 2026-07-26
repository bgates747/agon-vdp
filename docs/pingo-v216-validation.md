# Pingo VDP 2.16 Port and Fab Validation

Date: 2026-07-26

Status: the compatibility port builds for ESP32 and native x86-64. The exact
current `moveair/jet.bin` passes headless liveness and live visual/interactive
smoke tests. Repeatable native captures of its 320x148 Pingo render target and
the 320x240 `moveobj/tri` target are established. The modern image has been
flashed, all tests performed afterward were reported passing, and the
triangle's remembered near-camera defect was absent on hardware and Fab.
Pixel-exact comparison with Alpha 7 or another accepted reference and
deterministic final Fab scanout remain pending.

This is the implementation evidence ledger for
[Decision Record 0001](decisions/0001-pingo-fab-vdp216-strategy.md). It records
what was changed, which exact revisions and artifacts were tested, why the
external-module seam was selected, how the later Fab orchestration fork is
bounded, and how the modern image was qualified on hardware.

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
| `pingo-v2.16-userspace` capture commit `c490406ee721c5a53f04c069ee10302a855b7564` | Opt-in deterministic native Pingo render-target capture |
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

The native adapter used for the original ABI, headless-liveness, and live GUI
tests was built from scratch on x86-64 with GCC/G++ 13.3.0:

```text
video/build/userspace/vdp_pingo.so
  10,664,312 bytes
  SHA-256 716127d32f4b2aac12fe872f4797bbfe6848cd703aa45a982607ccd8ed608ecc
```

After capture commit `c490406`, the clean capture-enabled build was:

```text
video/build/userspace/vdp_pingo.so
  10,779,168 bytes
  SHA-256 da645a9c79be1780759efa5d49c239e0c254515239abe571475fce0b5b591e26
```

`ldd -r` reported no missing relocations for both checkpoints. The shared
object exports all 15 entry points loaded by Fab's `VdpInterface`, and the
smoke harness checks all 15 with `RTLD_NOW` before exercising Pingo. It also
verifies the unmangled Pingo renderer symbol.

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
for the exact current client. That original run did not enable pixel capture.
The later render-target run below establishes repeatable Pingo target bytes;
it does not establish historical equivalence or deterministic final Fab
presentation.

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
surviving the command stream. It remains a qualitative presentation smoke
test, not a deterministic final-scanout or Alpha 7 equivalence oracle. The
separate deterministic result below covers only Pingo target bitmap 257.

### Deterministic Jet Pingo-target capture

Capture commit `c490406` adds an opt-in userspace hook at the owned VDP seam.
It copies packed target bitmap 257 immediately after the selected Pingo
render and configured dithering path. `PINGO_CAPTURE_FRAME` counts Pingo
render commands, not Fab display refreshes. The writer uses exclusive,
PID-scoped temporary files, never replaces an existing path, and publishes
metadata last as the completeness marker.

With dithering disabled, render ordinals 1 (repeated), 2, 3, and 5 were
selected in fresh Fab processes using the same Jet artifacts and disposable
SD profile. The final capture-enabled artifact above independently reproduced
ordinals 1 and 5. Every raw target was byte-for-byte identical:

```text
scope      Pingo render target bitmap 257
format     RGBA2222, packed AABBGGRR, one byte per pixel
size       320x148, 47,360 bytes
CRC-32     10A67048 (CRC-32/ISO-HDLC)
SHA-256    768f07b8115df6391d9a0a1611adf9e293a96740a4962d049788c15777ecdd5e
PPM        142,095 bytes
PPM SHA-256
           8dab28be024597f0cf278b9fa40a266224bd686bea16452e44e4df76054f15c8
```

The raw `.rgba2` bytes are the regression oracle; the generated PPM is only a
visual preview because it discards alpha. This capture is taken at the Pingo
renderer boundary. It precedes later VDU bitmap plotting, instrument-panel
composition, scaling, and Fab's final 640x480 scanout. The successful live GUI
run supplies qualitative evidence for that later presentation path, not a
deterministic scanout comparison.

Jet creates its KOAK object with bitmap ID zero while the wrapper binds a
non-null `Texture` whose pixel pointer remains null. Visible textureless
geometry can therefore reach the inherited non-void `shade()` path that has no
return value. The stable signature is scoped to the recorded revisions,
inputs, and GCC/G++ 13.3.0 `-O2` build; it is not a compiler-independent
language guarantee.

### Textured-triangle localization fixture

The existing `moveobj/tri` fixture exercises a textured object and avoids the
known null-texture `shade()` path:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `tri.bin` | 3,422 | `ae6b514c0c7739c88a6ed7315f16f937969a572ff8c2afd60eae131da7a9a9b5` |
| `blenderaxes.rgba2` | 1,156 | `6d7156081386707a0ed346256a95b64409fb3fe90045a833ac42d85b4b21e25c` |

The disposable SD profile placed those files under `/moveobj` and used this
CRLF `autoexec.txt`:

```text
SET KEYBOARD 1
cd /moveobj
load tri.bin
run
```

Two fresh Fab processes produced byte-identical first-render targets:

```text
format     RGBA2222, packed AABBGGRR, one byte per pixel
size       320x240, 76,800 bytes
CRC-32     323B33E6 (CRC-32/ISO-HDLC)
SHA-256    f81dd66876ef012a6f1e52bae2821c275f1cf33e9a7e977c193be93bad4b4958
PPM        230,415 bytes
PPM SHA-256
           dafd0f8efbb9685fd7e866730d18458b67c053469593739f4d8e0114d9b2e64a
```

The PPM preview showed the expected single textured triangle. This is a
repeatable native localization fixture, not yet a correctness comparison with
Alpha 7 or another accepted reference.

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

## Durable Fab integration

The initial validation deliberately required no Fab changes. After the
external VDP seam and visual fixture had proved useful, the Author chose to
make the cross-project workflow durable in a separate owned fork:

```text
GitHub       https://github.com/bgates747/fab-agon-emulator
checkout     /home/smith/Agon/mystuff/fab-agon-emulator
branch       pingo
base         98bbb392b75b196171cc620b60839220e5ce53ed
commit       1b582ed38e57541ca902319e42fe28677800316b
```

The official checkout at `/home/smith/Agon/fab-agon-emulator` remains the
untouched upstream reference. In the owned checkout, `origin` names the
Author's fork and `upstream` names `tomm/fab-agon-emulator`.

Commit `1b582ed` adds `scripts/run-pingo` and `docs/pingo.md`. The launcher:

- defaults to `moveobj/tri` and accepts fixtures such as `moveair/jet`;
- stages the selected binary and runtime data in a disposable SD directory;
- writes a CRLF `autoexec.txt` that loads and runs the program;
- uses absolute MOS and VDP paths and a fail-closed working directory;
- supports explicit repository and artifact path overrides;
- handles the host's user-local SDL3 runtime; and
- removes the temporary SD card on exit unless preservation is requested.

The fork's pinned submodules were initialized at their recorded baseline
commits. Its release executable built successfully with the existing
user-local SDL3 installation:

```text
LIBRARY_PATH=/home/smith/.local/lib cargo build --release
target/release/fab-agon-emulator
SHA-256 832f6f8a18e4608f420381124ba33c4b550a034eb3973facdd9e8108fef8264b
```

A six-second headless run using the fork's executable, the exact
capture-enabled Pingo module, and `moveobj/tri` reached repeated 320x240
rendering. Exit status 124 was the expected external timeout, and the
disposable SD directory was removed.

The same fixture was also reviewed interactively by the Author in Fab and
described as flawless, with none of the remembered near-camera distortion.
The Author made the same visual observation on physical hardware. This is
accepted human visual evidence, not pixel-exact final-scanout equivalence.

Fab integration commit `654ded9` adds the standard-library Python helper
layer:

```text
scripts/build-pingo-vdp.py  native build, ABI/render smoke, artifact identity
scripts/test-pingo.py       fresh-process deterministic fixture regressions
scripts/pingo-status.py     cross-repository commits, dirt, and artifact hashes
scripts/update-upstream.py  explicit Fab upstream report/fetch/merge
scripts/run-pingo --rebuild everyday build-smoke-launch loop
```

The helper unit suite passed four tests. The build helper passed the native
smoke test. The status tool produced both human and valid JSON reports. The
upstream helper fetched and reported `upstream/main` without merging. The
regression helper reproduced both accepted frame-1 hashes in fresh Fab
processes, and the rebuild launcher reached live 320x240 triangle rendering
before a deliberate timeout. Temporary SD cleanup passed.

## Validation gate ledger

| Gate | Result |
| --- | --- |
| Stock official VDP 2.16 ESP32 build | Passed |
| Clean native stock VDP rebuild | Not separately captured; installed Fab stock module boots |
| Native external Pingo VDP load and ABI | Passed |
| 64x64 native bitmap/control/render smoke | Passed |
| Exact current `moveair/jet.bin` under Fab | Passed for protocol and 15-second liveness |
| Live human visual/interactive Jet smoke | Passed |
| Deterministic native Jet Pingo-target repeatability | Passed: selected render ordinals 1, 2, 3, and 5 were byte-identical |
| Deterministic native `moveobj/tri` target repeatability | Passed in two fresh processes |
| Human visual `moveobj/tri` comparison on hardware and Fab | Passed; no remembered near-camera distortion observed |
| Pixel-exact Alpha 7/reference comparison or deterministic final Fab scanout | Pending |
| Stock VDU workload on the modern Pingo port | Author reported all performed tests passed; exact workload list was not recorded |
| Modern Pingo ESP32 build and resource audit | Passed |
| Modern Pingo image on physical hardware | Flash passed with esptool verification; subsequent tests reported passing |
| Robustness corrections | Intentionally not started |

## Decision at this checkpoint

The earlier instruction to defer a Fab fork is superseded. The external
`--vdp` seam remains sufficient for VDP execution and capture, but a fork is
now justified as the owned integration and orchestration layer. It does not
move VDP implementation ownership out of `agon-vdp`.

The earlier instruction not to flash the modern image is also superseded.
The recorded image was flashed successfully, and the Author reported that all
performed tests passed. Because the individual tests were not enumerated,
formal qualification should repeat them against an explicit checklist.

The next integration task is to extend the Fab branch from one launcher into
a compact build/test/status harness. Pixel-exact reference comparison and
final Fab scanout remain separate future gates.

Do not mix inherited correctness fixes into this compatibility checkpoint.
Missing-target handling, ownership and teardown, texture bounds, the
missing-return path, and packed-pixel dithering should become small,
individually tested commits after emulator visual parity and hardware
qualification.
