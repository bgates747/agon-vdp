# Pingo History Handoff: Perspective Repair and Performance Work

Date recorded: 2026-07-26

## Codex session entry

Before acting on this handoff, read the
[canonical Codex instructions](../../../agon-dev-env/codex/AGENTS.md)
completely. The expected local file is
`/home/smith/Agon/mystuff/agon-dev-env/codex/AGENTS.md`. Keep reusable
environment and workflow guidance there; keep only Pingo- and VDP-specific
findings in this repository.

This note preserves an initial archaeology pass over the Author's Pingo work.
It is intended as a starting point for a later investigation, not as a final
technical account.

## Contemporary Discord evidence

In a Discord post dated 2024-10-24, the Author reported that Pingo's main
branch was at 2.10.0 and worth pulling because it contained major corrections
to the perspective-transform logic. The corrections fixed a severe failure to
render objects properly when they were very close to the camera.

The Author also reported stepping away after burning out while attempting to
improve rasterization performance by porting an apparently elegant algorithm
from a 1990s 3D-computing magazine aimed at Windows. That attempted port did
not work.

## Repository relationship

- This repository is the Author's fork of the Agon VDP firmware.
- The `pingo` branch contains most of the VDP-side Pingo engine work.
- The companion eZ80 assembly demos are in the separate `pingoasm`
  repository.
- `pingoasm` contains the tag `pingo3d2.10.0.alpha6` at commit `217c4e7`,
  dated 2024-08-29.
- The VDP fork's `hecker` and `hecker2` branches contain later or parallel
  attempts at the 1990s-era texture-mapping optimization.
- The `compression` branch is a separate experiment with a high-ratio but
  VDP-CPU-intensive compression algorithm.

## Later verified source provenance

A full source audit on 2026-07-26 established the exact point of departure:

- TurboVega's `agon-vdp-otf` branch `pingo3D` ends at
  `f4814813e8155780c5ad2602cd45f82ca5a72eec`.
- That exact commit and tree are already in this repository. It is a direct
  ancestor of current `pingo`, not a copied or reconstructed source snapshot.
- The Author's continuation starts at immediate child
  `ac6fcc92829aa4d5424de632fe71d0143ba8522e`.
- TurboVega imported the original `fededevi/pingo` core at `dacb520`. The
  strongest upstream source fingerprint is
  `216d2db1216b91797ff9672b909da9ae900fd170`; the import already contains
  small Agon/ESP32 adaptations, so no upstream commit is wholly byte-identical.
- TurboVega authored the Agon VDU bridge, buffer-backed control, PSRAM and
  bitmap integration, transforms, original protocol document, and initial
  demos. Several surviving defects predate the Author's continuation.

The complete evidence and inherited-versus-later attribution are in
[../pingo-turbovega-baseline.md](../pingo-turbovega-baseline.md). The broader
current-state report is
[../pingo-reconstruction-state.md](../pingo-reconstruction-state.md).
The later decision to preserve Alpha 7 and forward-port the coherent Pingo
feature to VDP 2.16 is recorded separately in
[Decision Record 0001](../decisions/0001-pingo-fab-vdp216-strategy.md).
The resulting ESP32 and Fab builds, exact Jet liveness and live visual runs,
deterministic native Pingo-target baselines, resource audit, and remaining
reference-comparison and hardware gates are recorded in
[the VDP 2.16 validation ledger](../pingo-v216-validation.md).

## Chronology from the Author's commit messages

This reconstruction deliberately uses only commits authored as
`bgates747 <brandon.gates@gmail.com>` and only their commit messages. No claims
below are attributed to other contributors, and the code diffs had not yet
been used to interpret intent.

### Perspective and rendering correction

- **2024-08-27:** Manually merged Pingo to VDP 2.10.0 and bumped alpha 6.
  Scratch-renderer commits progressed from “builds and runs but renders
  nonsense” to “mostly works except perspective calcs may be off, and camera
  moves don't affect render.”
- **2024-08-28:** Adjusted `Z_THRESHOLD`; recovered camera transforms while
  noting occluded faces; moved UV lookup after depth culling; enabled
  face-normal culling; recorded “correct uv interpolation”; then got Z-depth
  working and continued correcting the inverted Y axis.
- **2024-08-29:** Recorded “y axis inversion fixed.” The companion
  `pingoasm` repository was tagged `pingo3d2.10.0.alpha6` the same day.

The commit messages do not explicitly say “close-camera failure fixed.”
The best message-level matches are the camera-transform recovery, correct UV
interpolation, Z-depth repair, `Z_THRESHOLD` adjustment, and axis corrections.
The 2024-10-24 Discord post supplies the contemporary explanation that these
changes collectively corrected the severe near-camera rendering defect.

### Measurement and performance work

- **2024-08-28:** Added frame timing and moved UV lookup after depth culling.
- **2024-09-03:** Converted pixels directly to RGBA222P and recorded a 17%
  full-screen `earthuv` improvement from 3.10 to 3.62 frames per second.
  Direct-to-framebuffer rendering was then reported working.
- **2024-09-04 through 2024-09-06:** Simplified renderer/backend access,
  moved renderer and camera setup to initialization, restored background
  clearing, and changed camera FOV to 0.5. Companion assembly-demo commits
  added background images and a skydome.
- **2024-09-11:** Added textureless-triangle rendering and “restore[d]
  scanline intersection logic superceding bounding box rasterization.”
  From the message alone, this appears to reverse an unsuccessful
  rasterization direction, but it cannot yet be identified confidently as
  the magazine algorithm.
- **2024-09-17:** The final Author commits on `pingo` are small prototype and
  debug cleanups.
- **2024-10-24:** The companion `pingoasm` repository received the commit
  “bunch of changes to things i can't recall what for” on the same date as
  the Discord post.

The firmware work summarized in the Discord post therefore appears to have
been completed mostly from late August through mid-September. The October post
was a retrospective status report rather than a same-day announcement of a
new firmware correction.

## Suggested next archaeology pass

1. Inspect the diffs from the VDP 2.10.0 manual merge through the
   2024-08-29 correction sequence.
2. Correlate those changes with `pingoasm` tag `pingo3d2.10.0.alpha6` and its
   near-camera test models.
3. Identify a reproducible close-camera regression scene and separate
   geometry, depth, clipping, and perspective-correct UV behavior.
4. Compare the `pingo`, `hecker`, and `hecker2` rasterization implementations
   and locate the source magazine or copied algorithm if any citation,
   comments, or abandoned files survive.
5. Benchmark correctness before speed so a faster rasterizer cannot silently
   restore the previously corrected distortion.
