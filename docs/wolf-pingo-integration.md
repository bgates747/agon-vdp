# Wolf3DOrig and Pingo combined-VDP integration

Status: PingoWolf 0.1.0 Alpha 1 qualified for experimental release on
2026-08-01. The Author explicitly authorized this work as a critical juncture
and selected **Ultra** reasoning for the integration and review. Repeated
embedded heap/PSRAM exhaustion testing remains an explicit alpha limitation.

This file is the combined worktree's durable implementation record. The
normative product contract remains
`~/Agon/mystuff/agon-dev-env/cross-agent/wolf_pingo_vdp/integration-contract.md`,
accepted and authorized in `agon-dev-env` commit
`82c01453af433072d6d963d8def4206b79f70e93`.

## Reproducible source manifest

The combined product is being assembled in a clean, separate worktree. The
two qualified subsystem worktrees remain reference and rollback sources, not
integration scratch space.

1. Combined branch: `pingowolf`.
2. Combined worktree: `~/Agon/mystuff/agon-vdp-wolf-pingo`.
3. Exact upstream Agon VDP 2.16.0 base:
   `c7ac293d2aa81ddfa693390549bcd909069c8fc3` (`Bistromathics`).
4. Sole Pingo firmware input:
   `c26a0cec30b20ddc619060ec03195ebe81c81dc2`, from branch
   `pingo-v2.16-promotion` in
   `~/Agon/mystuff/agon-vdp-pingo-v216-promotion`.
5. Sole Wolf3DOrig firmware input:
   `ae689415e58fd8dbb1265931177d8241669b636e`, from branch
   `wolf3d/vdp-extension` in `~/Agon/mystuff/agon-vdp-wolf3d`.
6. Nominated Wolf3DOrig application counterpart:
   `a1c929140560908d1c8496830c0dff3d703d3bd4` in
   `~/Agon/mystuff/Wolf3dOrig`.
7. The qualified Pingo 2.15 commit is historical evidence only. It must not be
   imported in addition to `c26a0ce`.

Generated executables, ignored build trees, emulator-profile contents, and
later dirty changes in any source worktree are excluded from this manifest.
The combined branch imports the nominated semantics onto the exact base; it
does not merge either subsystem branch wholesale.

## Ownership and import rules

1. Pingo exclusively owns `video/pingo/**`, `video/pingo_3d.h`, Pingo-only
   userspace code, tests, protocol documents, and assembly fixtures.
2. Wolf3DOrig exclusively owns `video/wolf3d/**`, `video/wolf3d.h`, Wolf-only
   userspace code, tests, protocol documents, and application fixtures.
3. Neither exclusive source family is reformatted, renamed, regenerated, or
   semantically edited by the other subsystem's agent. A necessary change is
   first recorded through the collision protocol and reviewed by its owner.
4. Shared integration files are hand-reconciled. Choosing one branch's entire
   shared file is forbidden where it would discard the other subsystem's
   behavior or safety guards.
5. Later features require a new source nomination or an explicitly reviewed
   integration change. Historical textual differences are not requirements.
6. Pingo and Wolf state, callbacks, counters, tokens, and teardown paths stay
   independent. Alternating the two protocols must preserve their standalone
   behavior.

## Shared-file merge decisions

1. Keep adjacent buffered top-level opcodes: Pingo `0x49` and Wolf3DOrig
   `0x4A`. Subcommand values are local to those opcodes, so both may use
   subcommand `41`.
2. Preserve the distinct completion packets: `P3DR` for Pingo and `W3DR` for
   Wolf. Neither subsystem may advance or emit the other's notification state.
3. In `video/vdu_buffered.h`, include and dispatch both extensions. Preserve
   Pingo's registered-control teardown and mutation guards, and provide an
   independently managed Wolf control registry.
4. In `video/vdu_stream_processor.h`, keep both private friend/dispatcher
   boundaries. A public-adapter redesign is outside this first integration.
5. Retain Pingo's live-control ownership and type-safety guards in shared
   buffer, font, graphics, audio, sprite, and buffered-mutation paths.
6. Retain Pingo's sprite-local RGBA2222 solid-bitmap subcommand `0x22`; it is
   independent of buffered opcodes `0x49` and `0x4A`.
7. Reconcile the VDP 2.16 USERSPACE fixes once in `context/cursor.h`,
   `hexload.h`, and `vdu_layers.h`. Retain the common `size_t` fix in
   `ymodem.h` and one shared `arduino_yield_shim.h`.
8. Do not import Wolf's `vdu_sys.h` switch-buffer debug message; it is
   diagnostic noise, not nominated behavior.
9. Preserve Pingo's USERSPACE-safe diagnostic output in `video/video.ino`
   without changing the embedded serial path.
10. Build one combined userspace VDP adapter rather than linking both
    subsystem adapters, which each include all of `video.ino` and would
    duplicate the VDP and sound-accessor definitions. The joint Makefile must
    retain independently callable Pingo and Wolf suites plus aggregate and
    cross-subsystem targets.
11. Preserve the upstream identity and print the subsystem identities on
    separate boot lines:

    ```text
    Agon Platform VDP Version 2.16.0 Bistromathics
    Pingo 0.1.0 Alpha 1
    Wolf3DOrig 0.1.0 Alpha 1
    ```

12. Keep the Pingo and Wolf project-local emulator profiles independent. Add
    a combined integration profile; do not replace or mutate either enduring
    standalone profile.
13. Defer collision C-025's proposed Wolf `W3DV` version query. The accepted
    Wolforig recommendation and Author disposition retain the nominated Wolf
    protocol unchanged: opcode-local subcommand 40 is not implemented or
    advertised in this combined candidate. A future query requires a new
    coordinated nomination rather than an integration-only import.

## Reviewed integration safety resolutions

### 1. Wolf hidden render-scratch IDs collide with global resources (C-015)

The nominated Wolf bridge uses global buffer IDs `0xFFFC`, `0xFFFD`, and
`0xFFFE` as private render scratch. Wolf rendering clears and recreates those
IDs. Buffer IDs are application-selected global VDP resources, so a valid
Pingo control at any of those IDs would be torn down by an unrelated Wolf
render. Merely choosing different reserved numbers would retain the defect.

The reviewed resolution replaces registry-backed scratch with caller-owned,
PSRAM-backed `BufferStream` storage and local, non-owning RGBA2222 `Bitmap`
objects. Drawing still uses `Canvas::drawBitmap`, and every path drains queued
plot work before local bitmap/backing lifetimes end. The implementation no
longer reserves, clears, or recreates global IDs for Wolf scratch.

Required regression: create live Pingo controls at all three former scratch
IDs, render Wolf walls, sprites, and weapon, then render the three Pingo
controls again and verify their state and output are unchanged.

### 2. Wolf control lifetime and transactional publication

The nominated bridge stores controls in a function-local static
`std::map<uint16_t, Wolf3dControl>` and obtains them through `operator[]`.
Consequently, an unknown or malformed subcommand can allocate persistent
state, and no accessible buffer-clear, teardown, processor-reset, or global
reset path can erase that state. This conflicts with the required repeated
create/render/teardown lifecycle and makes heap/PSRAM accounting ambiguous.

The reviewed registry has explicit find, erase-by-ID, and reset-all paths and
does not reuse Pingo's registry or teardown counters. An unknown opcode-local
subcommand is rejected before registry lookup. An existing control is copied
to a local `shared_ptr` before its handler runs, so a reentrant clear or ID
replacement cannot destroy the object while a long handler is active.

For a new ID, a prospective Wolf control **may be allocated before its payload
has been completely parsed**. This is private temporary state, not published
state. Each handler reports parse success, and the dispatcher calls
`bufferClear(bufferId)` and inserts the control into the registry only after a
complete command succeeds. A truncated payload therefore causes no persistent
control publication, buffer/ID claim, or teardown of an ordinary or Pingo
occupant. Do not describe this as allocation-before-parse elimination; the
contractual result is no persistent state or ID mutation on truncation.

### 3. Callback reentrancy and completion ordering

General VDP callback buffers may remove themselves or one another while being
called. `bufferCallCallbacks` now snapshots callback IDs before dispatch so
those valid mutations do not invalidate iteration.

Pingo completion delivery is also reentrant: a `P3DR` callback can clear or
repurpose the active Pingo control synchronously. In the diagnostic render
path, target hashing and the complete diagnostic line now finish before
`send_render_complete(sequence)`, which is the final member action. No code
may dereference the in-place control after that call returns.

### 4. Formal lifetime for Pingo's raw buffer storage

Pingo controls continue to reside in raw `BufferStream` bytes, but
placement-new now formally begins the C++ object lifetime. Failed
initialization and normal deinitialization invoke the destructor explicitly.
`std::is_trivially_copyable` and `std::is_trivially_destructible` assertions
pin the assumptions required by Pingo's complete-representation clearing and
in-place lifetime management.

### 5. Read-only debug lookup

The upstream 2.16 `printBuffer` implementation is retained byte-for-byte and
uses `buffers.find()`. Debugging a missing or Wolf-only ID must not use
`buffers[bufferId]`, because that would materialize a ghost ordinary buffer
and interfere with reciprocal typed-ID replacement.

### 6. Wolf tilemap/Pingo type isolation

Wolf's `GetTile` checks `isPingo3dControlBuffer(tilemapBufferId)` at each use
before reading ordinary buffer bytes. A live Pingo control fails closed as a
solid wall rather than being interpreted as tilemap data. This is deliberately
a use-time check: if the same ID is later cleared and recreated as an ordinary
tilemap, an existing Wolf scene immediately reads it normally without being
reinitialized. The USERSPACE-only `UserspaceGetTile` hook exercises this real
renderer path rather than inferring type safety from framebuffer pixels.

### 7. Pingo-to-ordinary buffer replacement

A Pingo control's first ordinary buffer block is actually its in-place C++
object storage. Merely deinitializing the control before an ordinary write
would leave that stale raw block at index zero and append the caller's payload
behind it. `bufferWrite` now conditionally uses canonical `bufferClear` when
the ID is a live Pingo control, then stores the caller's first payload as the
sole block. For a Wolf-only or already ordinary ID it releases Wolf state but
preserves the established ordinary multi-block append behavior.

The native cross-test inspects the production buffer registry directly: the
Pingo-to-ordinary replacement must contain exactly one one-byte block with the
caller value; a second ordinary write must yield the original one-byte block
plus a two-byte appended block. Wolf's production tile accessor also confirms
that the replacement starts with caller data rather than stale Pingo storage.

### 8. Transactional Pingo and generic-buffer creation

Pingo subcommand 0 now consumes both dimensions, validates the complete
payload, constructs and initializes a candidate control in private storage,
and only then commits through canonical `bufferClear`. Invalid dimensions or
allocation/initialization failure preserve the prior ordinary, Pingo, or Wolf
occupant exactly; a successful creation atomically replaces that occupant and
publishes one registered Pingo backing block. Rejected replacement of an
existing Pingo control therefore preserves its callback token, render
sequence, and owned-allocation state.

The generic writable `bufferCreate` path follows the same fail-closed ordering
for typed-only occupants: it allocates and validates the candidate backing
before deinitializing Pingo or Wolf state. An allocation failure cannot erase
a live Wolf control merely because the caller attempted to repurpose its ID.
The native cross-test exercises malformed and allocation-failed replacement
over exact multi-block ordinary state, an existing Pingo control, and a live
Wolf tilemap, followed by successful ordinary-to-Pingo and Wolf-to-Pingo
commit and allocation-baseline recovery.

### Native regression coverage

`userspace/combined_subsystem_test.cpp` covers distinct and same-ID ownership,
all three former Wolf scratch IDs, exact `P3DR`/`W3DR` isolation, local-handler
teardown safety, callback-ID mutation, Pingo completion-triggered teardown,
read-only debug inspection, unknown and truncated Wolf commands, targeted and
global clear, repeated recreation, allocation-accounting recovery, live-Pingo
tilemap rejection, recovery after ordinary tilemap recreation, clean
Pingo-to-ordinary replacement, and preservation of ordinary multi-block
append. It also verifies transactional Pingo creation over ordinary, Pingo,
and Wolf occupants, including deterministic allocation failure and exact
state preservation. Pingo's direct and bridge sanitizer suites additionally
exercise repeated in-place construction and teardown.

### Deferred hardening and mandatory capacity gate

1. Wolf allocator OOM-contract hardening is deferred. The dispatcher handles
   a failed top-level `make_shared_psram`, and a privately allocated control is
   discarded when parsing fails, but this integration does not redefine or
   claim a fully graceful contract for every underlying allocator failure.
2. Generic output-stream orphan semantics are a broader buffered-VDP policy
   question and are deferred. They must not be silently redefined by either
   subsystem merely to close this integration.
3. Before combined acceptance, hardware qualification must repeatedly create,
   render, replace, and clear both control types while measuring heap and
   PSRAM. Successful native tests or a successful link do not waive this
   capacity/recovery gate.
4. C-025/W3DV is deferred by explicit Author disposition. The preflight checks
   that the combined Wolf allowlist remains subcommands 0 through 17 plus 41,
   and rejects `W3DV` or a `case 40` implementation until a future coordinated
   protocol nomination supersedes this decision.

## Reproducible integration preflight

Run the preflight from this repository's root while the combined changes are
still dirty or after they have been committed:

```bash
python3 scripts/check_wolf_pingo_integration.py
```

The checker resolves every nominated hash, derives both source manifests from
the exact base, compares all exclusive imports, accounts for every shared and
integration-only path, and rejects undeclared ownership-fence edits. It also
checks the protocol opcodes, multiline branding, shared lifecycle ordering,
callback snapshot, placement lifetime, completion ordering, read-only debug
lookup, transactional Wolf publication, test evidence, and use-time tilemap
guard.

The three reviewed exclusive-file deviations are pinned exactly:

1. `video/pingo_3d.h`:
   `7780c598ff02550f295c12c4b352a2b11c1dc65ba331dfb7114424ffce72ec7c`.
2. `video/wolf3d.h`:
   `bc97ba43a8dc024d9e17244e539eb41692363cf14e53fc5a2628ee08aa63c9fb`.
3. `video/wolf3d/render/wolf3d_draw.h`:
   `1e395f7fbf4f5730d73cb96eaf16646c14a35119080ffb26cebc1254bbecb494`.

These fingerprints and narrow semantic checks permit the reviewed integration
fixes without creating a broad exception for either exclusive namespace.
The test-only adaptation in `userspace/wolf3d_renderer_test.cpp` is separately
pinned at
`85d65e3b1e80566ccad21c1026c3d1ba7b471d36740872043a24c56e93323bec`;
it supplies the Pingo-control predicate and directly tests rejection followed
by ordinary tilemap recovery.

Two hand-composed shared integration surfaces are also pinned in their
entirety. This prevents a narrowly reviewed lifecycle correction from turning
into a general exception for shared dispatcher or regression-test edits:

1. `video/vdu_buffered.h`:
   `55caac2329180d2c269d633b76096c475a43caa24001d376a548add8a3fa338b`.
2. `userspace/combined_subsystem_test.cpp`:
   `2a1bdbdac5bc8f744b4eb61c17cc60f3ff243cc019a294e1b12bb156b4880a74`.

The four integration-owned emulator additions are also exact-file exceptions,
not directory-wide allowances:

1. `scripts/setup_combined_emulator.py`:
   `89ff20997b67b98ea6e40d3351c6c40b4c47f5cf673289f618848e0cd138af81`.
2. `scripts/run_combined_emulator.sh`:
   `47592f703d8861e85bf7825a13011dbf5161185b2341078a6fd3392c36f9a103`.
3. `userspace/combined_emulator_exit.asm`:
   `66e40cd06f1986f62b0843b2d129bd8e8ff0496bf34cfe2ef17c4c10e8d779ed`.
4. `docs/wolf-pingo-emulator.md`:
   `1e935d9ae237a3e7008d6b6b9644d337e8ebe6cfa8ab505c5f3165df0fdc7d1a`.

The preflight additionally checks their isolated profile path, pinned Wolf
snapshots (including hash-checked recovery from an already accepted combined
profile when mutable source outputs drift), two-way source/profile overlap
rejection before mutation, complete inventory of both selected Pingo fixture
directories and the MOS symbol map, checksum manifest, CRLF visual chain, and
freezing of every executable, firmware, fixture, and interactive-autoexec
launch input into a private run root. It also checks atomic, no-overwrite
smoke-log retention; fail-closed absolute launch; the 36-frame Pingo count;
Wolf dispatch evidence; dummy audio/video; and emulator-only status-zero exit.

## Qualification evidence to date

**Final current-source automated artifact ledger: complete.** These results
were regenerated after transactional Pingo publication and final C-025/W3DV
removal. They describe the exact source accepted by the 106-path preflight.
The Author's sequential visual-and-audio emulator review is complete. Real
embedded Wolf heap/PSRAM recovery remains a separate open gate below.

1. [x] The reproducible source-integration preflight passed all 102 paths at
   the automated qualification checkpoint. After adding the four reviewed,
   fingerprinted combined-emulator files, the current preflight passes all 106
   paths. It resolves the exact base, Pingo, Wolf, Wolf application, and
   accepted-contract hashes; verifies the exclusive/shared manifests; and
   accepts only explicitly reviewed deviations.
2. [x] The clean ordinary native smoke passes, including Pingo, Wolf, the
   128-cycle lifecycle regression, and the cross-subsystem suite. The
   11,282,432-byte `video/build/userspace/vdp_combined.so` has SHA-256
   `74584480ea7b735c7d93d80e93f622f2c2e66eb64080442b5452a4c751719838`.
3. [x] The clean diagnostic native smoke passes with the same cross-subsystem
   coverage plus the diagnostic tests and Pingo render diagnostics. The
   11,305,480-byte diagnostic `vdp_combined.so` has SHA-256
   `aeafac1344f575ca190866fd06807d9c2c77739501dae40d7c89ca0962d27932`.
4. [x] The final combined bridge passes its ASan/UBSan run, including Pingo
   bridge robustness and the cross-subsystem regression.
5. [x] The final native 128-cycle Wolf create/clear regression preserves a
   live Pingo control and independent ordinary-buffer state, returning the
   Wolf registry and Pingo-owned allocation count to baseline. This is strong
   host lifecycle evidence; it is explicitly **not** proof of ESP32 PSRAM
   exhaustion behavior or allocator recovery on embedded hardware.
6. [x] Clean final-current-source ESP32-PICO-D4 builds pass for all three
   supported configurations:

   | Configuration | PlatformIO environment | Flash bytes | Static RAM bytes | Binary bytes | `firmware.bin` SHA-256 |
   | --- | --- | ---: | ---: | ---: | --- |
   | Ordinary | `esp32dev` | 1,117,717 | 44,768 | 1,118,096 | `b6c529de07f46a6d9d75da0c87908b75dd81b3cbdee4f4aaa4a774522c2ed2ce` |
   | Diagnostics | `esp32dev-pingo-diag` | 1,120,293 | 44,768 | 1,120,672 | `26242f763fa210a300fca9a6603a99352a6ff8b6fcdabd162344c89458a45cc2` |
   | Unlit | `esp32dev-pingo-unlit` | 1,117,025 | 44,768 | 1,117,408 | `3fc38ee0cc6f5b2a2e048359b3d36ac39eaea10e6833cc388a24dba0931ad519` |

   The only compiler warning is the inherited Arduino ESP32 framework
   `uartSetPins` return-without-value warning. It is not introduced by the
   combined candidate.
7. [x] The final exact-target native module has SHA-256
   `58c6be56686ba0d7659d8124998799fa8f7ade36cd005d0db5edd3df48e079d5`.
   Its 36-frame Cube run at bitmap ID 1257 matches the standalone qualified
   Pingo 2.16 reference exactly for every color-buffer and z-buffer hash. The
   reference log is
   `/tmp/pingo-v216-current-36-cube-target-hash.log` (SHA-256
   `93bef0a0bd8b54742899f8fab5c48d2e6dae807f3161537e90c425e8ac64b6fb`)
   and the final combined log is
   `/tmp/wolf-pingo-20260731-c025-final-target-hash.log` (SHA-256
   `852def7b828fa3f0f9e4c26fe6964ae2ae0629080492923783ea6ac29b6ae492`).
8. [x] The canonical `~/Agon/mystuff/pingoasm` build produces 18 application
   binaries, and its complete 147-test Python suite passes.
9. [x] The hardened isolated combined-emulator headless smoke passes in one
   process with the final ordinary module: exactly 36 Pingo Cube render
   records, a real Wolf opcode-`0x4A` dispatch, and a status-zero clean exit
   through Fab's emulator-only output port. The retained log
   `/tmp/wolf-pingo-20260731-c025-final-headless.log` has SHA-256
   `65977498e61c8b8e720bbdaefd2bcaa825aab3ca2305c1410467d774eced1afc`.
10. [x] Python syntax/byte-compilation, shell syntax, and the CRLF-aware focused
    diff check pass for the final integration helpers and records.
11. [x] The Author passed the sequential combined-emulator review. Earth Party
    Flat rendered correctly; Escape advanced to the hardware-accepted
    audio-enabled Wolf application at commit `3695b7d`; Wolf rendering,
    gameplay, and sound effects worked; and Escape returned to MOS. The profile
    pinned `wolf3d.bin` SHA-256 `2d845e2882ae22012f0c64c7705d47b7413dc6b6301bc07bcdfdfadbe635f1c7`
    and `sfx.agnb` SHA-256
    `f294c1152bedca56e726e115ce1d1dc2f5aa6191e340a96a79b1e845296f1cd4`.
12. [ ] Real embedded repeated Wolf create/render/clear testing with measured
    heap and PSRAM capacity/recovery remains pending. The native 128-cycle test
    does not close this hardware gate.
13. [x] The Author accepted the packaged sequential hardware fixture on
    2026-08-01: Earth Party Flat rendered correctly, followed by the accepted
    audio-enabled Wolf application. The combined sequence worked beautifully
    on the flashed ordinary firmware and returned control normally.

### Superseded artifact provenance

These hashes identify earlier green checkpoints only. They are deliberately
retained for traceability and must not be substituted for the final ledger
above.

1. Ordinary and diagnostic native modules:
   `a8a519704e2f9164a97f2d87f96fda9c494550b5dd58fdab49c6eabb84fd8d62`
   and
   `c30fd3b9221a01c3bc5fdb7a821077122fddfbf8b7a37593031da8da83213820`.
2. Ordinary, diagnostic, and unlit firmware images:
   `6f647d0c58eafc4a12784ce0bddfe5b132236928e15ba55fdd0c8154f15e59b3`,
   `6d5344f4a168b39fa1de8a3fbb774485fca25e658f034542dbe48cf22dc12317`,
   and
   `91229f8b792536fcf4db93b3614f0be3f1df1a653f9351557a83b5ad6c4ef9f3`.
3. Exact-target module and combined target-hash log:
   `b05e472e7c68b5700d6d441db2cd6604cc5df20cad7af53d3dc45090fd741829`
   and
   `aaf45c9605f8e136a53ad03e06cc84e14db890fcf13e4e61267384538e622275`.
4. Earlier retained combined-emulator headless log:
   `8909d4d5ca3f630b35c2b7603ebeb97737ba2333900d5d86ec78b6afac93e790`.

## Validation and acceptance gates

The combined candidate is not accepted merely because it compiles, flashes,
boots, or passes one subsystem. Record commands, toolchain identity, build
flags, output sizes, RAM/PSRAM measurements, and SHA-256 hashes as the gates
are completed.

1. [x] Clean embedded builds, source branding checks, size ledger, and final
   artifact hashes are recorded above; all three configurations fit without
   weakening either subsystem.
2. [x] Pingo's command-surface, ordinary, diagnostic, sanitizer,
   deterministic-target, assembly-build, and Python-test gates pass.
3. [x] Wolf's native renderer, fizzle, and userspace smoke gates pass in both
   ordinary and diagnostic final composition.
4. [x] The final cross-subsystem native suite alternates `0x49` and `0x4A`,
   invokes both local subcommand `41` implementations, checks `P3DR`/`W3DR`
   isolation, exercises every reviewed safety resolution above, and repeatedly
   tears down and recreates both state types.
5. [ ] On the physical target, measure repeated state creation, rendering,
   replacement, teardown, and alternation with both implementations linked,
   including heap and PSRAM capacity and recovery. This gate is mandatory and
   cannot be inferred from the passing 128-cycle native regression.
6. [x] In the combined emulator profile, obtain the Author's visual review of
   the accepted Pingo lighting/shading and multi-object Earth Party fixtures,
   then the Wolf combat/death/fizzle fixture, and finally both sequentially
   without changing the loaded combined module.
7. [x] Emulator automation supplements but does not replace the Author's
   visual and experiential review. **Do not commit any emulator-facing
   repository change until the Author has explicitly validated the affected
   behavior.**
8. [ ] On physical hardware, exercise Pingo's demanding multi-object scene;
   Wolf combat, death/respawn, fizzle, Escape/mode restore, and held-key
   behavior; ordinary buffered commands; keyboard/MOS traffic; and a
   noncustom VDP smoke application. Confirm each subsystem still works after
   the other has run without an intervening reset.

Any undeclared exclusive-file edit, unresolved semantic collision, output
regression, capacity failure, cross-subsystem state leak, or missing human
emulator/hardware gate stops acceptance and is recorded before further work.
