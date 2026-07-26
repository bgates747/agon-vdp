# Pingo Reconstruction State

Date: 2026-07-26

Status: archaeology, physical Alpha 7 baseline, emulator feasibility, and the
resumption strategy are established. The current firmware builds, flashes,
boots, passes an initial stock-VDU workload, and runs the current
`moveair/jet.bin` Pingo demo on physical hardware. A disposable native proof
has also run elementary Pingo rendering on VDP 2.16.

This is the working evidence report for
[pingo-reconstruction-todo.md](pingo-reconstruction-todo.md). Historical
claims inherited from contemporary messages remain in
[handoffs/pingo-history-2024.md](handoffs/pingo-history-2024.md).
The exact original source boundary is recorded separately in
[pingo-turbovega-baseline.md](pingo-turbovega-baseline.md).
The accepted modernization and emulator strategy is recorded in
[Decision Record 0001](decisions/0001-pingo-fab-vdp216-strategy.md).

## Evidence conventions

- **Verified** means observed directly in the current source, Git objects,
  tracked artifacts, or a recorded command result.
- **Inferred** means strongly suggested by chronology or code, but not stated
  explicitly in surviving contemporary evidence.
- **Unverified at runtime** means that the behavior has not yet been exercised
  with a known-valid workload on the physical Agon.

Firmware and demo conclusions use the recorded local refs; those repositories
were not fetched. The two external point-of-departure repositories were
freshly cloned on 2026-07-26.

## Repository snapshot

### VDP firmware

- Checkout: `/home/smith/Agon/mystuff/agon-vdp`
- Branch: `pingo`
- Commit: `47a6609bf3d2409cb49a08ff93b22bd569476cd4`
- Version: `ScratchPingo 2.10.0 Alpha 7`
- Recorded upstream: `origin/pingo` at the same commit
- Relevant recorded tips:
  - `origin/hecker`: `ccd3781`
  - `origin/hecker2`: `dfc921b`
  - `origin/main`: `b8c2862`
- Tracked source was clean at the start of the pass. The project-local
  reconstruction documents are untracked.

`pingo` and the recorded `origin/main` share merge base `471dc92` from
2024-07-10. They have 109 and 129 unique commits respectively. The 2.10.0
update on `pingo` was a manual source merge, not Git ancestry from official
tag `v2.10.0`.

### Companion demos

- Checkout: `/home/smith/Agon/mystuff/pingoasm`
- Branch: `main`
- Commit: `4d14d308711a4e611c3bece11ba08a8215688ac4`
- Recorded upstream: `origin/main` at the same commit
- Working tree: clean
- Alpha-6 tag: `pingo3d2.10.0.alpha6` at
  `217c4e7b0035e64f2e7f0d01b52914b8012ff2a5`

This is the only user-owned `pingoasm` checkout found. It is not yet listed in
the shared canonical repository catalog.

### TurboVega Agon port

- Checkout: `/home/smith/Agon/TurboVega`
- Remote: `https://github.com/TurboVega/agon-vdp-otf.git`
- Branch: `pingo3D`
- Commit: `f4814813e8155780c5ad2602cd45f82ca5a72eec`
- Working tree: clean

This exact commit and tree are present in the local firmware history.
`f481481` is the merge base and direct ancestor of the current `pingo` branch;
the local continuation begins at its immediate child `ac6fcc9`.

### Original fededevi Pingo

- Checkout: `/home/smith/Agon/pingo`
- Remote: `https://github.com/fededevi/pingo.git`
- Current `master`: `f171c81aa597436e8db8fafd842ab7af6ef13b83`
- High-confidence historical source revision:
  `216d2db1216b91797ff9672b909da9ae900fd170`
- Working tree: clean

TurboVega's `dacb520` import already contains small port edits, so no upstream
commit matches all 38 files byte-for-byte. Revision `216d2db` is the strongest
fingerprint: its README, license, every math file, and the unchanged renderer
files match the import.

## Build and execution state

### VDP firmware

The documented build is `pio run`. A project-local Python 3.14.6 environment
now contains PlatformIO Core 6.1.19. The configuration selects:

- `esp32dev`;
- `espressif32@6.6.0`;
- Arduino;
- `-O2` and PSRAM flags;
- `vdp-gl#get-bitmap-pixel`, ESP32Time, and CRC dependencies.

On 2026-07-26, this checkout built successfully with:

```text
.venv/bin/pio run
```

PlatformIO installed:

- Espressif32 platform 6.6.0;
- Arduino-ESP32 2.0.14;
- Xtensa ESP32 toolchain 8.4.0+2021r2-patch5;
- esptool.py 4.5.1;
- vdp-gl 1.0.5 at `9bd8fc8`;
- ESP32Time 2.0.6;
- CRC 1.0.4.

The result used 42,440 of 327,680 bytes of RAM (13.0%) and 1,025,713 of
1,310,720 bytes of application flash (78.3%). The generated upload image is
1,026,096 bytes:

```text
.pio/build/esp32dev/firmware.bin
SHA-256 a5a9fd0388af89cba00e8cc5e7b4ef583aa0e5d851dec27edac34d67c6aae528
```

The only build warnings came from pinned third-party tools: a Python
`SyntaxWarning` in esptool and a missing return value in Arduino-ESP32's
`esp32-hal-uart.c`. No project-source warning was emitted.

### Companion assembly

The installed assembler is `/home/smith/.local/bin/ez80asm`, version 2.1.
Unmodified 2024 sources initially fail because the current assembler expands a
short macro parameter inside a longer parameter name. Examples:

```text
VERTICES inside VERTICES_N -> model_vertices_N
MID inside BMID            -> Bmid
```

A disposable copy was repaired by giving the overlapping parameters distinct
names:

```text
VERTICES / VERTICES_N -> VERTEX_DATA / VERTEX_COUNT
UVS / UVS_N           -> UV_DATA / UV_COUNT
NORMALS / NORMALS_N   -> NORMAL_DATA / NORMAL_COUNT
MID / BMID in CO      -> MESH_ID / BITMAP_ID
```

This changes no emitted bytes:

- Current `main` `movecam/earthuv.asm` rebuilt byte-identically:
  28,952 bytes, SHA-256
  `cadd37c02030c08ba02cdb0a50cf21f5c733fb05f9ac9a58a92a7f25f753bf52`.
- Alpha-6 `movecam/cube.asm` rebuilt byte-identically:
  3,760 bytes, SHA-256
  `dedd196b8326001b92a7e714b980d81fcad3d07041ae45502a3171ea638c6e64`.

The repair exists only in `/tmp`; `pingoasm` remains unchanged. This
establishes that the surviving assembly and binaries are reproducible once the
macro-source compatibility issue is addressed.

### Emulator and hardware

The installed Fab Agon emulator does not ship a Pingo-capable Console8 VDP
shared object, and there is no packaged Pingo emulator profile. Fab does,
however, compile VDP source into a host-native shared library and support an
explicit `--vdp` path. It does not execute the ESP32 `firmware.bin`.

A disposable proof overlaid Pingo on Fab's current VDP 2.16 userspace source,
built and loaded a native shared object, created RGBA2222 bitmap 257 and a
64x64 Pingo control, rendered an empty scene, and retrieved a 640x480
framebuffer at 59.94 Hz. The exact `jet.bin` workload has not yet been run
under Fab. Architecture, evidence, ownership, rejected alternatives, and the
accepted forward-port plan are in
[Decision Record 0001](decisions/0001-pingo-fab-vdp216-strategy.md).

Physical hardware became available later on 2026-07-26. The VDP was identified
as a Silicon Labs CP2104 USB-to-UART bridge:

```text
/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_027E3885-if00-port0
```

PlatformIO successfully flashed the `47a6609` build. Esptool identified an
ESP32-PICO-D4 revision 1.1; every written flash region passed hash
verification, and the firmware booted as `ScratchPingo 2.10.0 Alpha 7`.

Runtime smoke-test observations:

- the Agon booted normally and displayed the customized Pingo version;
- an existing Wolfenstein 3D game, which uses stock VDP operations rather than
  Pingo 3D, ran successfully;
- the current `moveair/jet.bin` demo loaded from the SD card and remained
  interactively playable for approximately ten minutes without a VDP crash;
- the Pingo test based on
  `/home/smith/Agon/mystuff/pingoasm/crash/crashpingo.asm` crashed and reset
  the VDP into a state that required a hard restart.

`crashpingo.asm` is an older artifact committed on 2024-07-19. It creates the
Pingo control structure (`CCS`) before it creates render-target bitmap 257
(`CTB`). Current Alpha 7 initialization immediately obtains bitmap 257 and
dereferences its data pointer without checking whether the bitmap exists:

```cpp
auto tgtbmp = getBitmap(257).get();
m_renderer.frameBuffer.pixels = (p3d::Pixel*) tgtbmp->data;
```

The observed reset is therefore explained by the obsolete initialization
order, not by `moveair/jet.bin` or by stock VDU behavior. The companion-demo
history confirms this exact compatibility transition: commit `b25a3be` on
2024-09-03 moved target-bitmap creation before control-structure creation in
all then-current demos, with the explicit message “create target bitmap before
creating control structure so we can use it during intialization.”

This is still a firmware robustness defect: a missing prerequisite bitmap can
cause a null-pointer fault instead of a rejected command or recoverable error.

The debug serial monitor connects at 115200 baud but remains silent during
normal operation, as expected from `DEBUG` being set to `0` in
`video/video.ino`.

## Functional map

### Command dispatch

```text
VDU 23
  -> vdu_sys()
  -> vdu_sys_video()
  -> VDP_BUFFERED (&A0)
  -> vdu_sys_buffered()
  -> BUFFERED_PINGO_3D (&49)
  -> bufferUsePingo3D(scene ID)
  -> Pingo3dControl::handle_subcommand()
```

Principal locations:

- general VDU dispatch: `video/vdu.h`
- system and buffered dispatch: `video/vdu_sys.h`,
  `video/vdu_buffered.h`
- Agon/Pingo bridge and protocol implementation: `video/pingo_3d.h`
- imported and forked Pingo core: `video/pingo/`

Wire words are little-endian and timeout-aware through `readWord_t()`.

### Persistent state

The scene ID names a normal VDP writable buffer whose bytes hold a
`Pingo3dControl`. That control contains:

- persistent camera and scene transform state;
- a renderer and vestigial backend;
- heap-created maps of mesh IDs and object IDs;
- render width and height;
- a dithering selector;
- borrowed pointers into VDP bitmaps.

Geometry position/index arrays and object UV/index arrays are allocated in
PSRAM. The Z buffer is also PSRAM-backed. Bitmap pixels are not copied:
textures, the output framebuffer, and the optional background borrow their
storage from the VDP bitmap system.

Core limits and ownership:

- A render-local `Scene` holds at most 32 renderables.
- Objects are added in ascending object-ID order.
- Meshes store no vertex-count field and objects store no UV-count field, so
  uploaded indexes cannot be bounds-checked.
- The render-local scene dies after the call, temporarily leaving the
  renderer's scene pointer dangling until the next render.
- Bitmap lifetime is not retained with a shared owner, so replacing a borrowed
  bitmap can invalidate a Pingo pointer.

### Transform and rendering path

Incoming fixed-width values are converted as follows:

- position: signed word divided by 32767;
- UV: unsigned word divided by 65535, with V inverted on ingestion;
- scale: word divided by 256;
- rotation: signed word multiplied by `2*pi/32767`;
- translation: signed word multiplied by `256/32767`.

The camera is initialized once with near `1.0`, far `2500.0`, FOV `0.5`, and
the output aspect ratio. Each frame then follows:

```text
update dirty object transforms
  -> construct temporary Scene
  -> update camera and scene transforms
  -> clear Z and color buffers
  -> model/scene transform
  -> camera view transform
  -> projection
  -> whole-triangle near rejection
  -> perspective divide and Z clamp
  -> projected-winding rejection
  -> raster-space conversion and screen bounding box
  -> scanline intersections
  -> barycentric coverage
  -> per-fragment near and reciprocal-depth tests
  -> perspective-correct UV sampling
  -> packed-pixel write
```

The active 3D rasterizer is the static `rasterize()` in
`video/pingo/render/renderer.c`, not the generically named
`video/pingo/render/rasterizer.c`. The latter retains the older 2D
sprite/texture-transform paths.

The triangle loop finds two intersections per integer scanline, iterates only
that span, and then repeats a barycentric coverage check. UV lookup happens
after depth rejection. Sampling is nearest-neighbor.

The current hot path uses one-byte packed `AABBGGRR` pixels. It copies texture
samples directly to the output bitmap; the RGBA conversion helpers are not in
the active triangle loop.

Face-normal lighting and culling are currently disabled. Active back-face
culling uses projected winding.

### Upstream boundary

The source has three verified layers:

1. `fededevi/pingo` supplies the math, render abstractions, depth and texture
   helpers, 2D rasterizer, and original bounding-box 3D renderer.
2. TurboVega supplies the complete Agon VDU bridge, buffer-backed control,
   PSRAM/bitmap integration, wire conversions, transforms, demos, and
   object-local UV extension through exact commit `f481481`.
3. The local work begins at direct child `ac6fcc9`, later replacing the 3D
   perspective path, remapping UV ownership, adding transforms/tracking and
   dithering, packing pixels, rendering directly to a bitmap, and restoring
   scanline-restricted traversal.

The current renderer labels its projection/interpolation code as
Scratchapixel-derived. It labels the later scanline intersection helpers as
neither Scratchapixel nor upstream Pingo. Exact attribution and defect
provenance are in
[pingo-turbovega-baseline.md](pingo-turbovega-baseline.md).

## Alpha-6 anchor

The matching alpha-6 pair is:

- VDP: `6f18f2689c033271e897cfd70b3290358519eace`
  at 2024-08-29 10:00 EDT;
- `pingoasm`: tag `pingo3d2.10.0.alpha6` / commit `217c4e7`
  at 2024-08-29 10:50 EDT.

Confidence is very high because:

- both identify `ScratchPingo 2.10.0 Alpha 6`;
- `6f18f26` is the last VDP commit before the demo tag;
- the next VDP commit explicitly changes the version to alpha 7.

This does not prove which untracked binary was physically flashed in 2024.

The tag contains several simple candidate regression scenes. `movecam/tri`
and `movecam/cube` were introduced during the perspective rewrite, begin with
the object around Z = -5, and move the camera in roughly 0.25-unit increments.
They are the best current candidates for approaching and crossing the near
region. This is inferred; no surviving source labels one as the original
close-camera regression.

## Perspective-repair sequence

| Commit | Verified effect |
| --- | --- |
| `66cfc56` | Manually copies/merges VDP 2.10.0 changes. |
| `65a0fef` | Changes version to alpha 6. |
| `b4c2746` | Introduces the Scratchapixel-style floating-point barycentric and perspective-correct path; initially produces nonsense. |
| `dd4cc9b` | Restores most model/projection processing, but still omits the view transform. |
| `5b7f44b` | Tightens the Z clamp from `-0.01` to `-0.000001`. |
| `4807ef6` | Applies the camera view matrix to vertices, restoring camera movement. |
| `72050ec` | Says “correct uv interpolation,” but its diff adds face-normal work rather than changing the UV formula. |
| `017a91b` | Repairs reciprocal-depth storage/comparison, adds a per-pixel near test, and temporarily hardcodes the near threshold. |
| `2ad7b10` | Changes perspective division from `1/z` to `-1/z`, recorded as fixing Y inversion. |
| `138223d` | Merges the scratch branch; the renderer is effectively the scratch implementation. |
| `6f18f26` | Hardcodes near = 1, restores lighting, and produces the alpha-6 anchor. |

At `6f18f26`, the normal-culling block calculates `faceNormal` but performs its
dot product with `cameraNormal`. It is therefore camera-orientation-dependent,
not face-dependent. A later commit disables this block.

The corrected path never gained geometric near-plane clipping. It rejects
only wholly near/behind triangles and clamps individual projected Z values.
Triangles crossing the plane must remain a separate regression case.

## Disposable host-render probe

To obtain execution evidence without altering firmware, the pure C renderer
was compiled in `/tmp` with GCC, AddressSanitizer, and UndefinedBehaviorSanitizer.
The alpha-6 source was read from `6f18f26`; the current source used
`47a6609`. The current probe supplied only host stubs for ESP heap and DSP
headers.

The probe renders a textured triangle into a 64x64 framebuffer. Results:

| Case | Vertex Z values | Alpha-6 changed pixels | Current changed pixels |
| --- | --- | ---: | ---: |
| Ordinary | `-5, -5, -5` | 722 | 685 |
| Near visible edge | `-2.01, -2.01, -2.01` | 3854 | 3854 |
| Near rejected edge | `-1.99, -1.99, -1.99` | 0 | 0 |
| Near crossing | `-1.5, -5, -5` | 1541 | 1524 |
| Camera-plane crossing | `1, -5, -5` | 1596 | 1203 |

The probe's 64-bit framebuffer signatures were:

| Case | Alpha 6 | Current |
| --- | --- | --- |
| Ordinary | `4026f0e21d13d9c1` | `f1220134b00c82a1` |
| Near visible edge | `2fe3ee145e5131c1` | `8f3130da625c0abc` |
| Near rejected edge | `be7fb0911be00383` | `f65555da8aadc383` |
| Near crossing | `9cc7f459af369b21` | `12808fc62cc392e8` |
| Camera-plane crossing | `9078ed6cba668b0b` | `c2f1fe049add66f1` |

No sanitizer error occurred for these cases. The different ordinary/crossing
coverage counts are expected because alpha 6 tests the whole bounding box at
pixel centers while current `pingo` first restricts iteration to integer
scanline spans.

This probe provides two important verified observations:

1. The renderer compares already-projected Z to the camera's near value. With
   this matrix and direct camera-space test geometry, the visible/rejected
   transition occurs around Z = -2 even though `near` is 1.
2. A triangle with one vertex across the near or camera plane is still
   rendered as a large screen-clamped polygon rather than geometrically
   clipped.

These are deterministic host observations, not substitutes for output from
the ESP32 build. The framebuffer hashes are revision- and pixel-format-specific
and should only become canonical if the disposable probe is promoted into a
tracked regression harness.

## Recorded performance evidence

The final `pingoasm` commit contains raw timing logs in:

- `src/asm/fpscomparisons.txt`
- `src/asm/fpscomparisons2.txt`
- `src/asm/fpscomparisons3.txt`

For the main September `earthuv` sequence:

| State | Mean time | Mean FPS | Change from baseline |
| --- | ---: | ---: | ---: |
| Baseline | 323.615 ms | 3.094 | — |
| Packed RGBA2222P pixels | 276.500 ms | 3.615 | +16.8% |
| Direct render to bitmap | 266.000 ms | 3.760 | +21.5% |
| Lighting disabled | 231.092 ms | 4.328 | +39.9% |

The raw data validates the reported 3.10-to-3.62 FPS improvement. The code diff
shows that the first gain primarily came from changing `Pixel` from four-byte
BGRA8888 to one-byte packed RGBA2222P. Direct-to-bitmap rendering then removed
a full-frame copy.

The timer surrounds scene setup, renderer execution, optional dithering, and
the then-applicable final framebuffer handling. It is an end-to-end render
measurement, not an isolated inner-loop benchmark. The logs do not record
hardware identity, build flags, visual correctness, or dithering state.

Other surviving measurements include:

- `fsim`: 4.951 to 5.432 FPS under an “iterative-depth” experiment, +9.7%;
- October `earthuv`: 4.745 to 4.783 FPS with precomputed transform matrices,
  about +0.8%;
- alternate matrix implementations that were neutral or slower.

These figures are historical validation only; none was reproduced in this
session.

## Failed magazine optimization

The source is identified conclusively as Chris Hecker's Game Developer
Magazine texture-mapping series.

Branch topology:

```text
47a6609  pingo tip
  -> 18a3a9f  shared first Hecker port
       -> hecker:  c31f8ac -> a0ad1cc -> 831e553 -> ccd3781
       -> hecker2: 173bba8 -> 7c07a4c -> 604fcce -> dfc921b
```

The September 11 scanline restoration predates this experiment and is not the
magazine port. `rasterizer.c` is byte-identical on all three branches; the
experimental code lives in `renderer.c` and `hecker.c`.

The shared first attempt ports Hecker's floating-edge, divide-per-pixel
`DIVFLFL` mapper. Its commit records output that is not scaled to the screen.

The `hecker` branch then performs an invasive flattened-mesh rewrite, records a
segfault while uploading texture-coordinate indexes, and ends by calling the
one-vertex-at-a-time direction a “blind rabbit hole.” Its tip has the Hecker
mapper disabled.

The `hecker2` branch imports Hecker's original Win32 reference package and
eventually ports the `SUBAFXFL` design:

- 28.4 fixed-point screen edges;
- floating reciprocal depth and UV gradients;
- eight-pixel affine spans between perspective-correct endpoints.

The final port is not production-ready:

- no Z-buffer checks or writes;
- no framebuffer clipping before raw pointer writes;
- normalized Pingo UVs are treated as direct integer texel coordinates;
- no multiplication by texture dimensions;
- lighting is bypassed;
- no successful benchmark or correctness commit survives.

The imported reference package states “Copyright 1997 Chris Hecker, All Rights
Reserved” and supplies no permissive license. It should remain reference
material unless reuse permission is established.

The code evidence makes `hecker2` the high-confidence match for the remembered
elegant but unsuccessful magazine algorithm. The precise visual failure still
requires recollection or execution.

## Current high-confidence unfinished or unsafe areas

These are findings, not yet an implementation plan.

### Protocol and documentation drift

- The bundled Markdown and BASIC demos are unchanged from TurboVega's original
  contract and are historical, not current instructions.
- Original commands 3/4 uploaded mesh UV data; `5c75c3a` remapped them to
  object-owned UV data and indexes.
- Original object UV command 40 was removed by the same change.
- Documentation requires RGBA8888 textures; the active path expects packed
  one-byte RGBA2222P.
- Render command 38 accepts a bitmap ID, but the code ignores it and renders
  to hard-wired bitmap 257.
- Background is hard-wired to bitmap 258.
- Commands 41, 42, 141, 145, 149, and 153 are implemented but undocumented.

### Correctness and memory

- There is no geometric near/far/frustum clipping.
- Single-axis object Z-scale and scene Z-scale handlers mistakenly assign Y.
- Dithering still indexes the one-byte framebuffer as four-byte RGBA pixels,
  causing access beyond the logical packed framebuffer when enabled.
- Deinitialization is empty; maps, geometry/UV arrays, and the Z buffer leak.
- Replacing borrowed bitmaps can leave dangling framebuffer/texture pointers.
- Allocation and stream failures can still lead to null dereferences or
  unchecked writes.
- Geometry and UV indexes have no uploaded-count validation.
- Texture sampling wraps V by texture width rather than height, making
  non-square textures incorrect and potentially unsafe.
- Missing texture pixels can reach a non-void `shade()` path with no return.

### Incomplete cleanup

- The backend abstraction is mostly vestigial.
- Face-normal culling, lighting, and illumination are disabled.
- ESP-DSP matrix replacements are commented placeholders; scalar code remains
  active.
- Setup and render paths contain unconditional diagnostic `printf()` calls.
- The current header exports declarations for static renderer helpers that are
  only defined in `renderer.c`.

## Provisional “where it was left”

### Working in surviving evidence

- The alpha-6 perspective repair sequence reached a coherent, tagged demo
  milestone.
- Camera transforms, reciprocal depth, Y orientation, and perspective-correct
  texture interpolation were reported working.
- Alpha 7 changed the pixel path to packed RGBA2222, rendered directly into a
  VDP bitmap, supported background clearing, and restored scanline-restricted
  triangle traversal.
- Textured and textureless triangle paths exist.
- Prebuilt demos, model assets, and substantial raw timing evidence survive.
- Current and alpha-6 demo sources are byte-reproducible with a small
  assembler-compatibility rename.

### Not yet reverified

- Camera controls and object tracking.
- Correct depth ordering on intersecting models.
- Background, textureless, and dithering behavior.
- The exact scene that demonstrated the close-camera repair.
- Any historical FPS figure on present hardware.

### Abandoned experiments

- `hecker`: flattened mesh and one-vertex-at-a-time integration, ending in a
  recorded segfault/rabbit hole.
- `hecker2`: incomplete Chris Hecker subdividing-affine rasterizer port.
- ESP-DSP matrix substitutions: placeholders and measurements, with no
  production replacement.

## Recommended next milestone

The smallest useful next milestone is a reproducible correctness baseline, not
another optimization:

1. Preserve the now-verified `47a6609` firmware as the stock-VDU-compatible
   hardware baseline.
2. Preserve the successful `moveair/jet.bin` run as the first known-good
   Pingo hardware baseline.
3. Add a null check or explicit render-target binding contract so an old or
   malformed client cannot reset the VDP during control initialization.
4. Apply the byte-neutral macro-parameter repair in `pingoasm`.
5. Run alpha-6 `tri` and `cube`, then current `tri`, `cube`, and `earthuv`.
6. Capture exact firmware/demo commits, build hashes, camera parameters,
   screenshots or framebuffer hashes, and timing output.
7. Promote the disposable host probe into a tracked test only after its
   intended compatibility contract is agreed.
8. Fix correctness and ownership defects before revisiting rasterizer speed.

The project-local toolchain, clean build, physical flash path, and basic stock
VDU smoke test are complete. A current, revision-matched Pingo workload is
available in the post-`b25a3be` companion demos.

## Questions that evidence has not answered

- Which model originally exposed the severe close-camera defect?
- Was `6f18f26` the exact flashed alpha-6 firmware, or was the good build made
  from uncommitted changes?
- Were the 3.10/3.62 FPS results measured on physical hardware, with dithering
  disabled?
- What did the final `hecker2` attempt display before it was abandoned?
- Are any historical screenshots, firmware binaries, or SD-card snapshots
  available?
- What feature was intended to follow the optimization work?
