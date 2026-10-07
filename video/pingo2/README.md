# Corrected Pingo 2 source closure

This directory contains the plain-C `math/` and `render/` closure from Pingo
commit `05761ad232c80445a97550b25e92530882d02c04` (tree
`283801a4505608bb32fe231f9115d9dd6f20bfee`) with the accepted Agon Flight
Simulator corrected source overlays substituted directly. This closure now
includes PINGO-015 R01 compile-time BGRA8888/RGBA2222 pixels and C13
camera-invariant world lighting. `esp32dev-pingo2-probe` explicitly selects
BGRA8888; `esp32dev-pingo2-probe-rgba2222` selects direct FabGL `AABBGGRR`.

The retained license is the donor repository's `LICENSE`. This import contains
no Wolf renderer, Wolf protocol, registry, or combined-VDP customization.

## PINGO-017 O01 migration checkpoint

The corrected closure includes frame-owned camera inversion and per-object
view/model composition and light normalization. It matches the effective
FSIM source at commit `8f886459a7f275e49187c395ec493c9db32565a1`.
See [the agent handoff](AGENT-HANDOFF.md) for provenance, validation, remaining
acceptance gates, and the boundary between probe builds and runtime support.

## PINGO-018 O02 bounds contract

The retained closure adds conservative whole-object rejection and
the bounded `Mesh` contract. Zero-initialize each mesh; set its caller-owned
arrays and index count, then call `mesh_prepare_bounds(mesh, position_count)`
with the real allocated position extent. The probe follows this contract.
Invalidate with `mesh_invalidate_bounds()` before in-place edits and refresh
afterward; do not mutate arrays during rendering. Pointer/count replacement
automatically disables cached rejection until refresh. An invalid/nonfinite
cache retains the ordinary triangle path; invalid position indices skip safely.

No new allocation, Wolf code, VDU command, emulator module, or scene registry
is introduced. Source identity, host equivalence, target build costs, and
acceptance are recorded in fsim's PINGO-018 task and engine ledger. PINGO-017
O01 and PINGO-018 O02 were accepted on 2026-10-06; the handoff above is a
historical migration checkpoint, not the current source identity.

## PINGO-019 O03–O05 accepted fragment changes

The retained closure now shares a triangle-local framebuffer/depth index,
uses exact inline nearest sampling and shading, and prepares four channel
light results per rasterized triangle in the direct RGBA2222 build. External
texture/pixel ABI wrappers remain. Geometry, coverage, perspective divisions,
sample/shade-before-depth order, lighting equation, alpha and quantization are
unchanged. Texture and framebuffer metadata remain caller-owned and immutable
during rendering. No new engine allocation or platform feature is introduced.

Matched FSIM engine identity:
`672170f5d8f8e065daec3a478f8c6efed7d23a8c16e632aa3a2f7e7e5320dca8`.
Each checkpoint passed exact host comparison, sanitizers and all three ESP32
builds. The Author accepted this checkpoint on 2026-10-06, as recorded in FSIM
PINGO-019 and its engine ledger.
Build/map evidence does not substitute for emulator or physical qualification.

## PINGO-020 O06 accepted exact row spans

The retained closure intersects each row with the exact three signed-edge
inequalities before fragment work. Inclusive zero-edge ownership, half-open
clipped box maxima, barycentric arithmetic, depth ties, texture and shading
are unchanged. Span normalization/jumps use wide intermediates, but bound
division remains unsigned 32-bit (native Xtensa integer division). The helper
supports all int32 edge inputs; it does not enlarge the existing rasterizer's
integer edge-setup domain. No ABI, buffer layout, heap policy, Wolf or platform
feature changes. Test-only reference/guard variants are compiled out normally.

Matched FSIM engine identity:
`5b379d635d80720ce5398b40010f5fa19bd0756d9678c3b7fae1a15882a0acdb`.
See FSIM PINGO-020 and the engine ledger for exhaustive coverage proof, exact
host qualification and target costs. The Author accepted O06 on 2026-10-06
and authorized committing/pushing the accumulated exact-output work before
the separately accepted PINGO-021 texture approximation is integrated.

## PINGO-021 O07 accepted perspective subdivision

The normal textured path now recovers perspective-correct U/V at carried
eight-pixel boundaries and interpolates between them using float additions.
The final tail ends at its last covered pixel. Invalid arithmetic falls back
to the original exact mapper; texture state advances for every covered pixel,
including depth rejects. Depth, coverage, clipping, lighting, sample/write
order, ABI and caller ownership remain unchanged. No Wolf, VDU, emulator or
application feature is introduced.

Matched FSIM engine identity:
`d4dfe19205d715cefebd1e41b9bf9905f35d9290c94883cabcbadb62c66c8c8a`.
The Author accepted the colour-only approximation and disclosed costs on
2026-10-06, and authorized integration and grouped commits/pushes. Exact O06
remains available at this repository's commit `173b2c9`.

The native corpus changes 19/128,000 colour pixels; the worst reviewed
checkerboard changes 542/51,200. These are corpus results, not universal error
bounds. Isolated target builds add 1,632/1,684 flash bytes (native/BGRA),
144 bytes to the compiler's object stack frame, and no static RAM. Host tests
and build evidence do not establish physical ESP32 speed or emulator runtime
qualification. See FSIM PINGO-021, its retained tests and engine ledger.

Final retained validation passes all 64 FSIM tests, both leak-enabled
ASan/UBSan builds, native execution of the unchanged target probe, and all
three ESP32 profiles in this worktree. Flash/RAM/stack match the reviewed
candidate costs exactly. PINGO-006 ancestry is preserved. The emulator module
and physical hardware validation remain separate, unstarted successor work.
