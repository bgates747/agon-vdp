# Pingo Reconstruction TODO

Created: 2026-07-26

## Goal

Become familiar with the Pingo integration in the Agon VDP firmware and
reconstruct the state in which the work was left: what works, what remains
unfinished, which experiments were abandoned, and what the best next
development step should be.

The historical starting point is
[handoffs/pingo-history-2024.md](handoffs/pingo-history-2024.md).
Current findings are recorded in
[pingo-reconstruction-state.md](pingo-reconstruction-state.md).
The original implementation boundary is recorded in
[pingo-turbovega-baseline.md](pingo-turbovega-baseline.md).
The accepted VDP 2.16 and emulator strategy is recorded in
[Decision Record 0001](decisions/0001-pingo-fab-vdp216-strategy.md).

## Working principles

- [ ] Keep archaeology read-only until a specific implementation change is
  agreed.
- [ ] Treat current source and reproducible behavior as authoritative.
- [ ] Use commit messages to locate evidence, then confirm conclusions from
  diffs and tests.
- [ ] Establish correctness before comparing performance.
- [ ] Keep observations separate from inference and recollection.
- [ ] Preserve unrelated and uncommitted work.

## Phase 1: Establish the present state

- [x] Record the VDP repository branch, commit, remotes, and working-tree
  status.
- [x] Locate the canonical `pingoasm` checkout and record its status.
- [x] Record the relevant branch tips and tags in both repositories.
- [x] Identify the documented build, emulator, and hardware deployment
  workflows.
- [x] Select the project-local toolchain or virtual environment where one is
  required.
- [x] Build the current `pingo` branch without modifying source.
- [x] Record build warnings, failures, generated firmware, and exact commands.
- [x] Determine whether an existing Pingo demo can be run in the emulator or
  on hardware.

Deliverable: a reproducible environment and repository-state snapshot.

## Phase 2: Construct a functional map

- [x] Trace Pingo VDU command decoding and dispatch.
- [x] Trace scene, object, mesh, material, texture, camera, and renderer setup.
- [x] Trace model, camera, projection, and viewport transformations.
- [x] Locate face culling, depth culling, clipping, and `Z_THRESHOLD`
  behavior.
- [x] Trace triangle rasterization and scanline intersection.
- [x] Trace perspective-correct UV interpolation and texture sampling.
- [x] Trace Z-buffer reads, comparisons, and writes.
- [x] Trace conversion to RGBA2222/RGBA2222P and framebuffer output.
- [x] Identify memory ownership, static limits, and buffer lifetimes.
- [x] Document the boundary between upstream Pingo code and Agon-specific
  integration.
- [x] Record the key files and functions for each stage.

Expected high-level path:

```text
VDU command
  -> Pingo control and scene setup
  -> transforms and projection
  -> culling and clipping
  -> rasterization
  -> depth and texture lookup
  -> Agon framebuffer
```

Initial files of interest:

- `video/pingo_3d.h`
- `video/pingo/vdu_pingo.md`
- `video/pingo/render/renderer.c`
- `video/pingo/render/rasterizer.c`
- `video/pingo/math/`
- the associated buffered-command handlers

Deliverable: a compact architecture and execution-flow note.

## Phase 2A: Establish the point of departure

- [x] Clone TurboVega's `agon-vdp-otf` `pingo3D` branch as a read-only
  reference.
- [x] Prove whether it is copied, patch-equivalent, or direct Git ancestry.
- [x] Identify the exact TurboVega tip and first local child commit.
- [x] Clone the original `fededevi/pingo` repository as a read-only reference.
- [x] Match TurboVega's import to the strongest original Pingo revision.
- [x] Separate imported Pingo, TurboVega's Agon port, and the local
  continuation by feature and defect provenance.
- [x] Rebuild the TurboVega tip in a disposable checkout.
- [x] Determine whether its bundled protocol documentation and demos still
  describe the current firmware.

Deliverable:
[pingo-turbovega-baseline.md](pingo-turbovega-baseline.md).

## Phase 3: Reproduce the known-good alpha-6 milestone

- [x] Inspect the VDP correction sequence from the manual VDP 2.10.0 merge
  through 2024-08-29.
- [x] Check out or otherwise inspect the `pingoasm` tag
  `pingo3d2.10.0.alpha6` at commit `217c4e7`.
- [x] Determine the exact matching VDP commit or smallest plausible commit
  range.
- [x] Identify the demo, scene, camera position, and models used at that
  milestone.
- [x] Locate or construct a reproducible close-camera regression scene.
- [x] Capture expected output from the known-good revision.
- [ ] Verify camera movement, Y orientation, face visibility, Z depth, and UV
  interpolation independently.
- [ ] Record firmware and demo revisions, build commands, runtime procedure,
  output, and performance.

Deliverable: a reproducible correctness baseline for later comparisons.

## Phase 4: Answer the historical questions

### Perspective repair

- [x] Identify the changes that restored camera transforms.
- [ ] Determine why objects close to the camera previously rendered
  incorrectly.
- [x] Explain the role of `Z_THRESHOLD`.
- [x] Separate geometry/projection errors from clipping, depth, and UV errors.
- [x] Identify the changes that corrected UV interpolation.
- [x] Identify the changes that repaired Z depth.
- [x] Identify the changes that corrected the inverted Y axis.
- [ ] Test which changes are individually necessary for the regression scene.

### Performance work

- [x] Reproduce or validate the reported `earthuv` improvement from 3.10 to
  3.62 FPS.
- [x] Attribute the improvement to direct RGBA222P conversion,
  direct-to-framebuffer rendering, or other changes.
- [ ] Confirm that the optimized output remains visually correct.
- [x] Identify the measurement method and whether timing includes frame
  clearing, command transfer, or presentation.

### Rasterizer experiments

- [x] Compare rasterization on `pingo`, `hecker`, and `hecker2`.
- [x] Identify the bounding-box implementation and restored scanline
  implementation.
- [x] Find comments, filenames, citations, constants, or copied structure that
  might identify the 1990s magazine algorithm.
- [ ] Determine which experiment failed to work and what “failed” meant:
  incorrect output, instability, integration difficulty, or poor performance.
- [x] Determine whether any useful portions of the experiment survived.

### End state

- [x] Inspect the final Author commits on `pingo`.
- [x] Explain the placeholder DSP matrix functions and debug cleanups.
- [ ] Determine whether the September 2024 tip builds and runs as intended.
- [x] Correlate the 2024-10-24 `pingoasm` changes with the firmware state.
- [x] List TODOs, debug code, placeholders, disabled paths, and suspicious
  comments remaining in the source.

Deliverable: an evidence-backed history organized by technical question.

## Phase 5: Establish the development baseline

- [x] Identify the physical VDP serial interface and flash the current
  `47a6609` firmware successfully.
- [x] Confirm that the custom `ScratchPingo 2.10.0 Alpha 7` build boots.
- [x] Smoke-test stock VDU compatibility with the existing Wolfenstein 3D
  workload.
- [x] Identify the crashing test as the old
  `pingoasm/crash/crashpingo.asm` artifact.
- [x] Explain its reset: it sends `CCS` before creating bitmap 257, while
  Alpha 7 dereferences bitmap 257 during `CCS` without a null check.
- [x] Locate companion-demo commit `b25a3be`, which deliberately moved target
  creation before control creation on 2024-09-03.
- [x] Run current `moveair/jet.bin` on Alpha 7 hardware; it remained
  interactively playable for approximately ten minutes without a VDP crash.
- [ ] Decide whether to harden control initialization against a missing target
  bitmap.
- [ ] Choose a small set of correctness scenes covering:
  - ordinary textured geometry;
  - geometry very close to or crossing the camera plane;
  - camera translation and rotation;
  - intersecting geometry and depth ordering;
  - back-face and occluded-face behavior;
  - textured and textureless triangles;
  - Y-axis orientation and UV landmarks.
- [ ] Capture reference images or deterministic framebuffer results.
- [ ] Define a repeatable timing workload and measurement method.
- [ ] Record image dimensions, pixel format, model data, camera parameters,
  and build configuration.
- [ ] Run the baseline against relevant historical revisions and branches.
- [ ] Ensure future performance work cannot silently reintroduce the repaired
  perspective defect.

Deliverable: a compact correctness and performance regression suite.

## Phase 5A: Forward-port Pingo to VDP 2.16 and Fab

- [x] Determine how Fab represents and loads VDP firmware.
- [x] Explain the historical lag between official VDP and Fab VDP releases.
- [x] Prove that current VDP 2.16 and the Pingo feature can compile and link
  as a native Fab shared object.
- [x] Run an elementary native Pingo bitmap/control/render smoke test.
- [x] Record the ownership, branch, implementation, and validation decision
  in [Decision Record 0001](decisions/0001-pingo-fab-vdp216-strategy.md).
- [ ] Preserve `47a6609` with an explicit archival ref.
- [ ] Create isolated `pingo-v2.16` and `pingo-v2.16-userspace` development
  worktrees without modifying upstream-owned checkouts.
- [ ] Import the coherent Pingo runtime onto official VDP 2.16.
- [ ] Add the narrow `0x49` and `0x22` VDU integration.
- [ ] Compile Pingo runtime sources explicitly in the native VDP build.
- [ ] Add an explicit ESP32/userspace allocation seam and remove the unused
  ESP-DSP host dependency.
- [ ] Repeat the elementary native render test without permissive C++ build
  shortcuts.
- [ ] Run the exact current `moveair/jet.bin` under Fab.
- [ ] Capture deterministic output for simple Pingo correctness scenes.
- [ ] Build the VDP 2.16 Pingo firmware and record real flash/RAM use.
- [ ] Re-run stock-VDU and Pingo smoke tests on physical hardware.
- [ ] Harden invalid target ordering, ownership, teardown, and packed-pixel
  dithering only after compatibility is established.
- [ ] Decide whether a Fab fork and packaged Pingo profile are warranted.

Deliverable: a reviewable Pingo feature on current VDP with matched native and
ESP32 validation.

## Phase 6: Produce the “where I left it” report

- [x] Summarize what is working and verified.
- [x] Summarize what appears to work but remains unverified.
- [x] List known defects and their reproduction steps.
- [x] List abandoned experiments and why they were abandoned.
- [x] List unfinished intended work.
- [x] List historical unknowns that require the Author's recollection.
- [x] Identify technical debt that is relevant to resuming development.
- [x] Recommend the smallest useful next implementation milestone.
- [x] Update the project handoff with durable conclusions.

Deliverable: a concise current-state report and prioritized next-step proposal.

## Evidence log template

Use one entry per investigation or test:

```text
Date:
Question:
Repository/branch/commit:
Commands or procedure:
Observed result:
Conclusion:
Confidence:
Remaining uncertainty:
Artifacts:
```

## Questions for the Author

Add questions here only after repository evidence has been exhausted.

- [ ] Which Pingo demo or scene best represents the last known-good state?
- [ ] Was the failed magazine algorithm remembered as a particular article,
  author, or technique?
- [ ] Was development stopped because of one blocking defect or primarily
  because the performance work became unproductive?
- [ ] What feature or optimization was intended to come next?
