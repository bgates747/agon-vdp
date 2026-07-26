# Decision Record 0001: Run Pingo in Fab and Forward-Port It to VDP 2.16

Date: 2026-07-26

Status: Accepted. The compatibility port, live visual Jet smoke, and
deterministic native Jet and textured-triangle target baselines are complete;
reference comparison and physical-hardware qualification remain pending.

Scope: Pingo source ownership, VDP modernization, Fab Agon Emulator
integration, and the order in which those changes will be developed and
validated.

## Decision

Preserve the current Alpha 7 firmware as the known-working historical and
hardware baseline. Re-express Pingo as a small, coherent feature change on top
of authoritative Agon VDP 2.16 rather than merging or rebasing the old Pingo
development history.

The durable source of truth will remain the Author's `bgates747/agon-vdp`
fork. Development will use three conceptual layers:

```text
archive/pingo-alpha7
    exact Alpha 7 baseline at 47a6609

pingo-v2.16
    official VDP 2.16.0
    + Pingo runtime
    + Pingo VDU protocol bridge
    + hardware build and tests

pingo-v2.16-userspace
    pingo-v2.16
    + Tomm's current USERSPACE adaptations
    + native Pingo build adapter
```

The Fab emulator itself will initially remain unchanged. Development builds
will be loaded through its existing `--vdp /absolute/path/vdp_pingo.so`
override. That seam is also sufficient for deterministic Pingo render-target
capture. Fab will be forked only if and when deterministic final Fab scanout,
a packaged Pingo firmware choice, repeatable outer build, or distributable
emulator profile is required.

The initial forward-port will preserve the behavior of the hardware-verified
Alpha 7 client contract. Robustness and API corrections will follow in
separate changes so that compatibility failures are not confused with
modernization failures.

## Evidence snapshot

The decision was made from these exact local revisions:

| Component | Revision | Role |
| --- | --- | --- |
| Author's Pingo VDP | `47a6609bf3d2409cb49a08ff93b22bd569476cd4` | Hardware-verified Alpha 7 baseline |
| Pingo/upstream merge base | `471dc92cba4df160048d40dc71a397cd3b37fe16` | Official VDP 2.9.2-era ancestry |
| Official VDP 2.16.0 | `c7ac293d2aa81ddfa693390549bcd909069c8fc3` | Modern hardware source baseline |
| Fab userspace VDP | `7bcf28e0a2376e32328a6a5554d0df852b75c80e` | VDP 2.16 plus host adaptations |
| `userspace-vdp-gl` | `ecb8aabffe97d66d6c6eb30a8a08e09932dbe2e4` | Host FabGL and ESP compatibility layer |
| Fab emulator | `98bbb392b75b196171cc620b60839220e5ce53ed` | Installed native emulator |

The official VDP source is
[`AgonPlatform/agon-vdp`](https://github.com/AgonPlatform/agon-vdp). The
emulator VDP and FabGL adaptations are maintained in Tom Morton's forks.

The physical Alpha 7 firmware at `47a6609` has already passed:

- a successful build and ESP32 flash;
- normal Agon boot with the expected custom version;
- a stock-VDU Wolfenstein 3D smoke test;
- approximately ten minutes of interactive execution of the current
  `moveair/jet.bin` Pingo demo.

The obsolete `crashpingo.asm` reset is separately explained by creating the
Pingo control structure before target bitmap 257. It is not evidence that the
current `jet.bin` or stock VDP behavior is broken.

## Why Fab firmware updates lag

Fab does not emulate an ESP32 and does not execute the ESP32
`firmware.bin`. Its VDP is a host-native dynamic library:

```text
agon-vdp video/video.ino
    + USERSPACE preprocessor adaptations
    + native userspace-vdp-gl
    + Fab's C ABI glue
    -> vdp_console8.so / DLL / dylib
    -> loaded into the Fab process
```

Moving an official VDP release into Fab is therefore a manual porting and
release workflow:

1. merge the official release into `tomm/agon-vdp:emulator-console8`;
2. repair host compiler and `USERSPACE` incompatibilities;
3. update `userspace-vdp-gl` when new graphics, UART, timing, or input
   behavior requires it;
4. update Fab's pinned submodule revisions;
5. build and package native libraries for each host operating system.

Historical delay varied from hours to months. That variation is maintenance
and behavioral-port latency, not slow firmware transfer or inherently slow
compilation.

Fab's userspace OTA functions are stubs. They consume firmware-update traffic
but do not write a boot partition, and `esp_restart()` is a no-op. Sending the
ESP32 firmware through the VDP updater inside Fab can consequently appear to
succeed without changing the loaded native library.

The dynamic library is retained for the lifetime of the Fab process. Native
Pingo iteration therefore means build and restart, not build and flash or hot
reload.

## Disposable feasibility proof

Before accepting this decision, a proof was built entirely under `/tmp`.
Neither the Author's VDP repository nor the upstream Fab checkout was changed.

The proof overlaid the Pingo runtime and four narrow integration points onto
Fab's current VDP 2.16 userspace source:

- buffered command `0x49`;
- extended solid RGBA2222 bitmap command `0x22`;
- the Pingo control dispatch and declarations;
- narrow parser access through friendship rather than making all VDU read
  methods public.

Pingo's 16 runtime C translation units were added explicitly to the native
link. The proof also supplied a host substitute for the unused `esp_dsp.h`
include and accommodated the C/C++ allocator boundary.

Observed results:

- the VDP 2.16 and Pingo sources compiled together;
- the native x86-64 shared object linked;
- all ABI symbols required by Fab were exported;
- `ldd -r` reported no unresolved relocations;
- the library loaded and initialized;
- the VDP completed its eZ80 general-poll handshake;
- extended bitmap selection created RGBA2222 bitmap 257 at 64x64;
- Pingo control buffer 1000 initialized successfully;
- Pingo rendered an empty scene into the target;
- framebuffer retrieval returned 640x480 at 59.94 Hz.

Representative successful output was:

```text
vdu_sys_sprites: bitmap 257 selected
vdu_sys_sprites: bitmap created for bufferId 257, format 1, (64x64)
bufferCreate: created buffer 1000, size 576
Pingo3dControl initialized
P3D: handle_subcommand(38)
Render to 64x64 took 0 ms (inf FPS)
native VDP started: 640x480 at 59.94 Hz
```

This proves architectural, compilation, ABI, command-dispatch, allocation,
and elementary rendering feasibility. It does not yet prove the full
`jet.bin` workload or visual equivalence.

The later tracked implementation did run the exact current `jet.bin` for a
deliberate 15-second headless interval without a crash. That later evidence,
including exact commits and artifact hashes, is in
[the VDP 2.16 validation ledger](../pingo-v216-validation.md). Visual
presentation was subsequently confirmed in a live interactive smoke test,
and repeated native captures established byte-identical Jet target bitmap 257
output. A textured-triangle fixture also produced a repeatable target. These
results establish repeatability at the Pingo renderer boundary, not
equivalence with Alpha 7 or deterministic final Fab scanout.

The proof also established two useful diagnostic facts:

- legacy one-byte bitmap selection maps bitmap 1 to buffered ID 64001 because
  the modern base is `0xFA00`; existing Pingo clients correctly use extended
  command `0x20` to select absolute bitmap 257;
- AddressSanitizer turns the unchecked target-bitmap dereference into a source
  stack trace rather than an unexplained VDP reset.

AddressSanitizer also reported an existing host teardown mismatch between the
fake ESP allocator and a C++ array deleter after the successful render. That
userspace diagnostic issue must be separated from Pingo rendering defects
before sanitizer runs can be treated as noise-free.

## Why a clean forward-port is required

The Pingo branch's displayed “2.10 Alpha” identity does not describe its Git
ancestry. Its common ancestor with modern upstream is the VDP 2.9.2-era
`471dc92`; the later 2.10 update was a manual source merge.

Relative to modern VDP history there are:

- 109 Pingo-side commits;
- 193 upstream-side commits;
- 12 content conflicts from a direct merge into the old Pingo branch.

Most of that history is experimentation, manual merge noise, formatting,
assets, and unrelated VDP drift. The coherent feature is much smaller:

- the Pingo math/render runtime and Agon wrapper;
- approximately 3,344 lines in 35 relevant engine/wrapper files;
- four VDP integration files with roughly 155 added lines;
- buffered selector `0x49`;
- extended bitmap creation selector `0x22`.

Both command values remain available in VDP 2.16. The current bitmap and
RGBA2222 APIs used by Pingo also remain present in current VDP-GL.

Replaying the old development history would obscure the feature boundary and
make future upstream merges difficult. A clean import makes that boundary
reviewable and allows hardware and userspace builds to share the same Pingo
feature commits.

## Implementation boundary

The forward-port should be expressed as small reviewable changes:

1. import only the runtime Pingo math/render sources and license;
2. add the Agon `Pingo3dControl` wrapper and VDU protocol hooks;
3. add current protocol documentation and identity/version changes;
4. on the userspace layer, compile the runtime C files explicitly and provide
   narrow host platform seams;
5. add regression fixtures without bundling host conversion tools into the
   firmware build.

`obj2vdu.c` is a host conversion utility and must not be compiled as ESP32
firmware. The historical PlatformIO layout compiled it recursively and relied
on link garbage collection to discard it.

The durable native build should compile Pingo's runtime sources as C rather
than depending on the proof's permissive single-C++-translation-unit shortcut.
The unused ESP-DSP include should be removed or guarded. ESP32 PSRAM allocation
and host allocation should meet behind a small explicit platform interface.

Parser read methods should remain encapsulated. A friend declaration or narrow
reader adapter is preferred to Alpha 7's broad move of parser internals into
the public interface.

## Alternatives considered

### Merge official VDP into the historical Pingo branch

Rejected. It starts from misleading manual-merge ancestry, retains stale VDP
code, creates broad conflicts, and makes it difficult to distinguish Pingo
behavior from unrelated modernization changes.

### Rebase or cherry-pick the 109 historical Pingo commits

Rejected. Those commits preserve valuable archaeology but are not a clean
product patch. Replaying abandoned renderer experiments, debug work, manual
copies, and partial changes would create unnecessary integration risk.

### Develop inside Fab's current VDP submodule and migrate later

Rejected as the durable workflow. The outer Fab checkout already contains
unrelated local changes, while nested submodule work can be detached,
unpublished, or omitted accidentally from an outer commit. A disposable spike
there is useful; authoritative development there is not.

### Fork Fab immediately

Deferred. The existing `--vdp` override already runs an external Pingo shared
library and proved sufficient for Pingo render-target capture. An outer fork
alone would not own unpublished changes inside the VDP or FabGL submodules.
Forking becomes useful when final Fab-composited scanout capture, packaging,
or repeatable submodule pinning becomes a deliverable.

### Flash or emulate the ESP32 firmware image inside Fab

Rejected. It bypasses Fab's established native VDP architecture and would
require ESP32, Arduino, FreeRTOS, UART, FabGL, display, and timing emulation.
It is far larger than the Pingo task and would provide less useful source-level
diagnostics.

### Clean feature forward-port in the owned VDP fork

Accepted. It creates one durable Pingo source of truth, preserves a known-good
baseline, minimizes divergence, supports both native and ESP32 validation, and
keeps changes out of upstream-owned checkouts.

## Known risks and deferred hardening

The compatibility port must record and test these defects without silently
changing them in the first transplant:

- initialization hard-dereferences bitmap 257;
- the output and optional background are hard-wired to bitmaps 257 and 258;
- render subcommand 38 reads but effectively ignores its bitmap argument;
- target dimensions and pixel format are not validated;
- `deinitialize()` is empty and subordinate allocations leak;
- framebuffer and texture storage are borrowed through raw pointers;
- replacing a bitmap can leave those pointers dangling;
- mesh and UV indexes lack uploaded-count validation;
- texture V wrapping uses width rather than height;
- the non-void texture shading path can fall through without a value;
- the dithering routines index a packed one-byte RGBA2222 framebuffer as
  four-byte RGBA data and are likely out-of-bounds when enabled;
- geometric near-plane clipping remains absent.

These will be addressed as explicit post-compatibility changes. Dithering and
invalid initialization order should not be exercised on hardware until their
memory safety is resolved.

The current Jet client creates an untextured KOAK object with bitmap ID zero.
The wrapper still binds a non-null `Texture` whose pixel pointer is null, so
visible textureless geometry can reach the inherited non-void `shade()` path
that has no return value. Native Jet signatures are consequently scoped to
the recorded source, inputs, and GCC/G++ 13.3.0 `-O2` toolchain until that
undefined behavior is corrected. The textured-triangle fixture does not
exercise this known null-texture path.

Fab remains a behavioral approximation:

- host allocation does not enforce real ESP32 PSRAM limits;
- FreeRTOS task priority, affinity, timers, queues, and watchdogs are not
  faithfully modeled;
- host rendering time is not an ESP32 performance measurement;
- a native fault terminates the emulator process rather than reproducing all
  VDP reset behavior.

Hardware remains authoritative for resource limits, timing, concurrency,
watchdog behavior, and final release qualification.

## Validation gates

The forward-port is not complete until it passes these gates in order:

1. build current stock VDP 2.16 for ESP32 without Pingo;
2. build the native VDP 2.16 userspace baseline without Pingo;
3. build and load the native Pingo shared object through `--vdp`;
4. repeat the 64x64 bitmap/control/empty-render smoke test;
5. run the exact existing `moveair/jet.bin` under Fab;
6. capture deterministic Pingo-target signatures for Jet and selected simple
   scenes, then compare them with Alpha 7 or another accepted reference; Jet
   and textured-triangle target repeatability are complete, while reference
   comparison remains pending;
7. run stock VDU workloads, including the existing Wolfenstein smoke test;
8. build the Pingo VDP 2.16 ESP32 image and record flash/RAM use;
9. flash physical hardware and repeat stock-VDU and current `jet.bin` tests;
10. only then begin robustness corrections.

The current Alpha 7 image costs 19,868 bytes more flash and 16 bytes more
static RAM than its contemporary stock baseline. Applying that measured cost
to the present VDP 2.16 build estimates approximately 83.7% application-flash
use, leaving roughly 213 KiB. At decision time, capacity was therefore not a
likely blocker, but the real port still had to record its own linked result.

The completed compatibility build confirmed that estimate: it costs 19,964
bytes of application flash and 16 bytes of static RAM over a clean stock VDP
2.16 build. It uses 1,097,393 of 1,310,720 application-flash bytes (83.7%).

## Consequences

Benefits:

- Alpha 7 remains reproducible and untouched;
- Pingo gains current VDP behavior and fixes;
- Pingo feature changes become reviewable rather than entangled with old
  branch history;
- Fab becomes a rapid command/render/crash diagnostic environment;
- the same feature implementation can be qualified natively and on ESP32;
- future official VDP updates begin from a current, intelligible base.

Costs:

- two active integration layers must initially be maintained;
- the native build requires explicit C-source and platform-boundary work;
- full client compatibility must be proven rather than assumed;
- emulator success cannot replace physical hardware testing;
- historical bugs must be separated carefully from forward-port regressions.

## Revisit triggers

Reconsider this structure if:

- official Agon VDP accepts Pingo or a generalized 3D extension;
- Tomm changes Fab's VDP plugin ABI or adopts a different firmware model;
- Pingo requires changes that belong generically in `userspace-vdp-gl`;
- maintaining separate hardware and userspace integration branches causes
  repeated divergence;
- the exact `jet.bin` workload exposes a fundamental incompatibility not seen
  in the feasibility probe.

## Related records

- [VDP 2.16 implementation and validation](../pingo-v216-validation.md)
- [Pingo reconstruction state](../pingo-reconstruction-state.md)
- [Pingo reconstruction TODO](../pingo-reconstruction-todo.md)
- [TurboVega point-of-departure audit](../pingo-turbovega-baseline.md)
- [2024 history handoff](../handoffs/pingo-history-2024.md)
