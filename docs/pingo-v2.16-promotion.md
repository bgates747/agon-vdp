# Pingo VDP 2.16 promotion

Status: qualified and accepted on machine tests, disposable emulator, and
physical Olimex hardware; ready for immutable Pingo 2.16 nomination.

## Purpose and scope

This branch advances Pingo alone from Agon VDP 2.15 to the exact upstream VDP
2.16.0 release. It is deliberately a qualification step before any combined
Wolf/Pingo firmware work:

1. The upstream base is tag `v2.16.0`, commit
   `c7ac293d2aa81ddfa693390549bcd909069c8fc3`.
2. The proven Pingo 2.15 reference input is commit
   `6c24691d7372f470f00c762103382ffed7967d5c`.
3. The 43 Pingo commits after `v2.15.0` were replayed with
   `git rebase --onto v2.16.0 v2.15.0` in the isolated
   `~/Agon/mystuff/agon-vdp-pingo-v216-promotion` worktree.
4. No Wolf source, dispatch case, protocol command, or application asset is in
   this branch. `scripts/check_pingo_scope.py` still passes.
5. The firmware identifies itself as `Agon Pingo VDP Version 2.16.0 Alpha 1
   Bistromathics`.

The historical `docs/tv-port.md` remains the provenance record for the
TurboVega-derived renderer and the 2.15 correction line. It is not rewritten
to pretend that the original port began on 2.16.

## VDP 2.16 compatibility changes

The embedded port replayed without functional source conflicts. VDP 2.16 did
expose two native userspace-adapter omissions:

1. The 2.16 VDP references the PlatformIO `CRC` library and `ESP32Time` from
   code included by the native module. `userspace/Makefile` now supplies their
   include paths and links `CRC16.cpp`, `CRC32.cpp`, and
   `CrcFastReverse.cpp` into the module.
2. Arduino's CRC sources call the global Arduino `yield()` function. The
   emulator adapter does not provide it, so `userspace/arduino_yield_shim.h`
   supplies the narrow host equivalent `std::this_thread::yield()`. The shim
   is force-included only while compiling the two CRC sources that need it;
   embedded builds are unchanged.
3. VDP 2.16 declared the YMODEM filename and filesize accessors with `size_t`
   but defined them with `unsigned`. Their definitions now match their public
   declarations. This is a type-correctness repair, not a Pingo behavior
   change.

These are common VDP 2.16 host-build accommodations. They do not import Wolf
code or alter Pingo protocol semantics.

## Qualification record

On 2026-07-31 the following machine gates passed from a clean build:

1. `python3 scripts/check_pingo_scope.py`.
2. `git range-diff` paired all 43 source commits with the 2.15 series. The
   only patch differences were the intentional `video/version.h` resolutions;
   every renderer, bridge, diagnostic, and hardening patch was identical.
3. The complete ordinary native `userspace` suite, including the real-module
   smoke and VDU bridge robustness harness.
4. Native renderer diagnostics.
5. Renderer AddressSanitizer/UndefinedBehaviorSanitizer tests.
6. Full bridge AddressSanitizer/UndefinedBehaviorSanitizer tests with only the
   already documented Fab host-adapter suppressions.
7. Clean PlatformIO builds of `esp32dev`, `esp32dev-pingo-diag`, and
   `esp32dev-pingo-unlit`.
8. Two silent, fresh-process emulator runs of the four-panel lighting and
   shading fixture on each of the 2.15 reference and 2.16 candidate.
9. An exact-output emulator oracle: all 72 frames of the two-revolution
   RGBA2222 Cube fixture matched the 2.15 reference byte-for-byte in both the
   final color target and z-buffer.

The exact-output comparison command was:

```bash
scripts/compare_pingo_target_hashes.py \
  --expected-stream 1257:72 \
  /tmp/pingo-v215-cube-target-hash.run-001.log \
  /tmp/pingo-v216-cube-target-hash.run-001.log
```

It reported:

```text
PASS: exact render-target match: 72 records (bmid 1257: 72)
```

Clean artifact identities before the human gates were:

| Artifact | SHA-256 |
| --- | --- |
| ordinary ESP32 firmware | `3020070288eab32ab5cbf8dc37d9c03c6528ef2d6871800d8ebab1bf3fdb0005` |
| diagnostic ESP32 firmware | `a6f048f83a54156390b1cc26770d683e0fe8a274b83e4ef63f313503f8f5e032` |
| unlit ESP32 firmware | `4f34847b280fbbca001594e8af0725c32f6981001842b66c04f649c3d677a2bf` |
| ordinary native module | `4be3955c7dbaeac8233bf531224707606481dbe116a665cf744f5de39860f65a` |
| diagnostic native module | `75bbd855f3768b387c4b875ba6a2fb66a509530c5a4c789d10653468791938fc` |
| exact-target native module | `af34c27967e7ea232863c4f31c69698eb3a1092d91ccf12be1320e4d3ceaea49` |

The ordinary build uses 44,736 bytes of RAM and 1,104,621 bytes of flash.
The only embedded compiler warning is the inherited Arduino ESP32
`uartSetPins` return-without-value warning.

## Remaining acceptance gates

1. [x] The Author visually accepted the disposable 2.16 emulator runs of both
   the four-panel lighting/shading fixture and the animated mixed-policy Earth
   Party scene on 2026-07-31. No enduring project-local emulator was refreshed
   before this gate.
2. [x] Flash the ordinary 2.16 firmware to the physical Olimex Agon Light 2.
   PlatformIO uploaded through `/dev/ttyUSB0`, verified every written block,
   and hard-reset the ESP32 successfully on 2026-07-31.
3. [x] Confirm ordinary non-Pingo smoke behavior. Jukebox ran successfully
   immediately after the Earth Party torture fixture without a VDP or machine
   reset. This is stronger lifecycle evidence than an isolated boot smoke: the
   Pingo workload returned control without leaving shared VDP buffers or state
   unusable.
4. [x] Visually exercise the promoted renderer on hardware. The canonical
   lighting/shading fixture and animated mixed-policy Earth Party scene both
   passed on the Olimex Agon Light 2 on 2026-07-31 after their complete SD-card
   payloads were refreshed and hash-matched to `~/Agon/mystuff/pingoasm`.
   Earth Party was considerably slower than in Fab, as expected for this
   deliberately full-screen multi-object torture workload; no visual defect
   was reported. The 72-frame Cube target/depth oracle separately covers exact
   frame equivalence, while the native suites cover clipping and near-plane
   edge cases.
5. [x] Retain deterministic diagnostic evidence. The ordinary, diagnostic,
   sanitizer, bridge, and exact color/depth oracle gates above cover the
   unchanged Pingo patch series on 2.16. A redundant hardware timing benchmark
   was not made a release gate; this promotion claims compatibility, not a
   performance change.
6. [x] Commit and push the qualified promotion branch as the final publication
   operation for this record.
7. [x] Update the cross-agent contract with the immutable promoted Pingo
   commit and only then permit Wolf/Pingo integration work to begin.

The external cross-agent manifest is authoritative for the immutable promoted
commit ID; unlike this in-commit record, it can name the commit containing this
document without creating a self-referential hash.
