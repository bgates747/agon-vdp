# Pingo 2 target checkpoint — agent handoff, 2026-09-11

## Start here

Linux workspace: `/home/smith/Agon/mystuff/agon-vdp-pingo2`, branch `pingo2`.
This is a linked worktree of `/home/smith/Agon/mystuff/agon-vdp`.
Origin: `https://github.com/bgates747/agon-vdp.git`, existing branch `pingo2`.
Do not change shared branches or sibling worktrees. Their unrelated work remains
outside this checkpoint. Read the central agon-dev-env/codex/AGENTS.md first.

This is FSIM's Pingo 2 target-cost/compilation branch, not a complete playable
flight simulator or an accepted native emulator module. The project task owner
is agon-fsim; do not create a competing TODO in this firmware tree.

## What was dirty and why

Exactly three source files were dirty against `9f42ea1`:

1. `render/renderer.h` adds instance-owned `prepared_view`.
2. `render/renderer.c` refreshes that inverse camera matrix once per render.
3. `render/object.c` consumes the prepared view and computes invariant
   view/model composition and normalized world-light direction once per object.

Per-vertex arithmetic order, per-triangle normal/diffuse evaluation, pixel
representation and uint32 depth remain unchanged. Compile-gated diagnostics
count the moved operations; the matching callback implementations and C/Python
ABI live in FSIM's host facade, not this target probe.

This is the same O01 change as FSIM commit
`8f886459a7f275e49187c395ec493c9db32565a1`. All 38 math/render files compare
byte-for-byte with its freshly built effective corrected closure. Recorded
source identity: `27664296c1de16679badf6be277633e57813af65ac5760e977cab9287adc1c26`.
The Author explicitly authorized this migration checkpoint commit. This does
not constitute a new visual/physical acceptance result or close the full task.

## Evidence and build boundaries

All 47 FSIM Linux contracts passed during the immediately preceding migration
review: 7 facade, 25 correction, 2 article, 9 optimization, 4 pixel-format.
These test the identical effective engine, including retained render/depth
oracles and camera-change/reset repeatability.

Fresh 2026-09-11 builds passed all three profiles. Reported flash/RAM bytes:
ordinary 1,077,285 / 44,688; BGRA probe 1,087,001 / 50,928; native probe
1,086,985 / 48,616. These fresh absolute sizes differ from the dated August
measurements; no fresh baseline A/B cost comparison was performed.

Target build command from this worktree:

```sh
/home/smith/.platformio/penv/bin/pio run \
  -e esp32dev \
  -e esp32dev-pingo2-probe \
  -e esp32dev-pingo2-probe-rgba2222
```

`esp32dev` excludes Pingo 2 entirely. The BGRA8888 and native RGBA2222 probe
profiles explicitly compile the imported closure and target probe. Their
successful compilation does not prove physical rendering performance or
emulator integration. No firmware is flashed as part of migration.

Earlier August measurements in FSIM PINGO-017 reported exact framebuffer/depth
agreement, sanitizer success and modest flash/stack improvements. Treat those
as dated evidence, not fresh physical runtime measurements or broad speed claims.

## Handoff to the VDP/native-module agent

Read `platformio.ini`, `video/pingo2_target_probe.c`, and the FSIM records below.
Keep the imported renderer free of Wolf/Pingowolf renderer/protocol additions.
ADR-0004 requires a Pingo 2-only first bespoke target. The containing firmware's
ancestry is not permission to import combined-VDP behavior into the engine.

Preserve the two pixel modes and exact source correspondence. Host diagnostics
must use FSIM's matching facade ABI. Future changes to prepared state must
refresh on every frame and remain renderer-owned. Do not infer that a successful
probe build implements an application-facing protocol or accepted emulator.

## Handoff to the FSIM/optimization agent

Mac source/docs: `/Users/bgates/Agon/mystuff/agon-fsim`.
Linux task owner: `/home/smith/Agon/mystuff/agon-fsim`.
Read these in order:

1. `AGENTS.md` and `docs/decisions/ADR-0004-pingo-2-renderer-selection.md`.
2. `PINGO-LINEAGE-AND-VERSIONS-PRECIS.md` and `PINGO-ENGINE-CHANGES.md`.
3. `docs/tasks/PINGO-017.md` and `docs/task-dependency-gates.md`.
4. `docs/development/2026-08-24.md` and `docs/development/2026-09-11.md`.

The task record predates this VDP checkpoint and still describes its commit as
pending. Reconcile that evidence when next updating the task owner; do not
interpret the stale wording as missing source. Final O01 acceptance remains
unrecorded. PINGO-018's dependency gate is not automatically opened by this
migration commit. Later native module, emulator and physical qualification gates
remain distinct. Existing terrain assets and ignored Linux runtime/build state
were intentionally not migrated to the space-constrained Mac.
