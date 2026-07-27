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
The resulting builds and validation gates are recorded in
[pingo-v216-validation.md](pingo-v216-validation.md).

## Current priorities

1. [ ] Redo the Pingo-to-Agon port from the latest upstream
   `fededevi/pingo`, using TurboVega's Agon integration idioms as the porting
   model rather than treating the current forked Pingo runtime as the starting
   point. Before carrying forward any local geometry, projection, clipping,
   depth, UV, or rasterizer fix, determine whether upstream independently
   corrected the same defect and prefer the upstream design where applicable.
   Preserve the current VDP 2.16 port and its deterministic fixtures as the
   comparison baseline.
   1. [x] Create the isolated
      `~/Agon/mystuff/pingo-tv-clean-port` workspace pinned to upstream Pingo
      `f171c81` and TurboVega `f481481`.
   2. [x] Adapt latest upstream's Entity/Backend API to exactly TurboVega's
      commands `0`–`40`, excluding every later local extension, and produce a
      clean ESP32 firmware build.
   3. [ ] Create or select TurboVega-wire-compatible RGBA8888 fixtures and
      validate the port visually and behaviorally before treating it as a
      correctness baseline.

## Working principles

1. [ ] Keep archaeology read-only until a specific implementation change is
  agreed.
2. [ ] Treat current source and reproducible behavior as authoritative.
3. [ ] Use commit messages to locate evidence, then confirm conclusions from
  diffs and tests.
4. [ ] Establish correctness before comparing performance.
5. [ ] Keep observations separate from inference and recollection.
6. [ ] Preserve unrelated and uncommitted work.

## Phase 1: Establish the present state

1. [x] Record the VDP repository branch, commit, remotes, and working-tree
  status.
2. [x] Locate the canonical `pingoasm` checkout and record its status.
3. [x] Record the relevant branch tips and tags in both repositories.
4. [x] Identify the documented build, emulator, and hardware deployment
  workflows.
5. [x] Select the project-local toolchain or virtual environment where one is
  required.
6. [x] Build the current `pingo` branch without modifying source.
7. [x] Record build warnings, failures, generated firmware, and exact commands.
8. [x] Determine whether an existing Pingo demo can be run in the emulator or
  on hardware.

Deliverable: a reproducible environment and repository-state snapshot.

## Phase 2: Construct a functional map

1. [x] Trace Pingo VDU command decoding and dispatch.
2. [x] Trace scene, object, mesh, material, texture, camera, and renderer setup.
3. [x] Trace model, camera, projection, and viewport transformations.
4. [x] Locate face culling, depth culling, clipping, and `Z_THRESHOLD`
  behavior.
5. [x] Trace triangle rasterization and scanline intersection.
6. [x] Trace perspective-correct UV interpolation and texture sampling.
7. [x] Trace Z-buffer reads, comparisons, and writes.
8. [x] Trace conversion to RGBA2222/RGBA2222P and framebuffer output.
9. [x] Identify memory ownership, static limits, and buffer lifetimes.
10. [x] Document the boundary between upstream Pingo code and Agon-specific
  integration.
11. [x] Record the key files and functions for each stage.

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

1. `video/pingo_3d.h`
2. `video/pingo/vdu_pingo.md`
3. `video/pingo/render/renderer.c`
4. `video/pingo/render/rasterizer.c`
5. `video/pingo/math/`
6. The associated buffered-command handlers.

Deliverable: a compact architecture and execution-flow note.

## Phase 2A: Establish the point of departure

1. [x] Clone TurboVega's `agon-vdp-otf` `pingo3D` branch as a read-only
  reference.
2. [x] Prove whether it is copied, patch-equivalent, or direct Git ancestry.
3. [x] Identify the exact TurboVega tip and first local child commit.
4. [x] Clone the original `fededevi/pingo` repository as a read-only reference.
5. [x] Match TurboVega's import to the strongest original Pingo revision.
6. [x] Separate imported Pingo, TurboVega's Agon port, and the local
  continuation by feature and defect provenance.
7. [x] Rebuild the TurboVega tip in a disposable checkout.
8. [x] Determine whether its bundled protocol documentation and demos still
  describe the current firmware.

Deliverable:
[pingo-turbovega-baseline.md](pingo-turbovega-baseline.md).

## Phase 3: Reproduce the known-good alpha-6 milestone

1. [x] Inspect the VDP correction sequence from the manual VDP 2.10.0 merge
  through 2024-08-29.
2. [x] Check out or otherwise inspect the `pingoasm` tag
  `pingo3d2.10.0.alpha6` at commit `217c4e7`.
3. [x] Determine the exact matching VDP commit or smallest plausible commit
  range.
4. [x] Identify the demo, scene, camera position, and models used at that
  milestone.
5. [x] Locate or construct a reproducible close-camera regression scene.
6. [x] Capture expected output from the known-good revision.
7. [ ] Verify camera movement, Y orientation, face visibility, Z depth, and UV
  interpolation independently against documented, industry-standard 3D
  rendering conventions. Current hardware and emulator behavior matches the
  Author's intended behavior, but that observation alone does not establish
  conventional correctness.
8. [x] Record firmware and demo revisions, build commands, runtime procedure,
  output, and performance. The reconstruction state and VDP 2.16 validation
  report contain the recovered revisions, procedures, deterministic captures,
  hardware observations, and surviving performance measurements.

Deliverable: a reproducible correctness baseline for later comparisons.

## Phase 4: Answer the historical questions

### Perspective repair

1. [x] Identify the changes that restored camera transforms.
2. [ ] Determine why objects close to the camera previously rendered
  incorrectly. The Alpha 6 history identifies a collective repair sequence
  involving the Scratchapixel-derived triangle path, camera transforms,
  `Z_THRESHOLD`, reciprocal depth, perspective UV interpolation, culling, and
  Y orientation, but it does not isolate the upstream defect or minimal fix.
  Resolve this by comparing latest upstream Pingo with the clean Agon port.
3. [x] Explain the role of `Z_THRESHOLD`.
4. [x] Separate geometry/projection errors from clipping, depth, and UV errors.
5. [x] Identify the changes that corrected UV interpolation.
6. [x] Identify the changes that repaired Z depth.
7. [x] Identify the changes that corrected the inverted Y axis.
8. [ ] Test which changes are individually necessary for the regression scene.
  This is coupled to item 2 and should be answered by controlled comparisons
  during the clean upstream port, rather than by assuming the historical local
  repair sequence was intrinsically correct.

### Performance work

9. [x] Reproduce or validate the reported `earthuv` improvement from 3.10 to
  3.62 FPS.
10. [x] Attribute the improvement to direct RGBA222P conversion,
  direct-to-framebuffer rendering, or other changes.
11. [x] Confirm that the current output is visually correct. The Author
   confirmed the current firmware and emulator behavior visually.
12. [x] Identify the measurement method and whether timing includes frame
  clearing, command transfer, or presentation.

### Rasterizer experiments

13. [x] Compare rasterization on `pingo`, `hecker`, and `hecker2`.
14. [x] Identify the bounding-box implementation and restored scanline
  implementation.
15. [x] Find comments, filenames, citations, constants, or copied structure that
  might identify the 1990s magazine algorithm.
16. [x] Determine which experiment failed to work and what “failed” meant:
  the Hecker optimization work rendered garbage, in addition to the
  integration failures preserved in its branch history.
17. [x] Determine whether any useful portions of the experiment survived.

### End state

18. [x] Inspect the final Author commits on `pingo`.
19. [x] Explain the placeholder DSP matrix functions and debug cleanups.
20. [x] Verify that the September 2024 Alpha 7 tip (`47a6609`) builds, boots
   on hardware, preserves stock VDP behavior, and runs a matching Pingo
   sample.
21. [x] Correlate the 2024-10-24 `pingoasm` changes with the firmware state.
22. [x] List TODOs, debug code, placeholders, disabled paths, and suspicious
  comments remaining in the source.

Deliverable: an evidence-backed history organized by technical question.

## Phase 5: Establish the development baseline

1. [x] Identify the physical VDP serial interface and flash the current
  `47a6609` firmware successfully.
2. [x] Confirm that the custom `ScratchPingo 2.10.0 Alpha 7` build boots.
3. [x] Smoke-test stock VDU compatibility with the existing Wolfenstein 3D
  workload.
4. [x] Identify the crashing test as the old
  `pingoasm/crash/crashpingo.asm` artifact.
5. [x] Explain its reset: it sends `CCS` before creating bitmap 257, while
  Alpha 7 dereferences bitmap 257 during `CCS` without a null check.
6. [x] Locate companion-demo commit `b25a3be`, which deliberately moved target
  creation before control creation on 2024-09-03.
7. [x] Run current `moveair/jet.bin` on Alpha 7 hardware; it remained
  interactively playable for approximately ten minutes without a VDP crash.
8. [ ] Decide whether to harden control initialization against a missing target
  bitmap.
9. [ ] Choose a small set of correctness scenes covering:
   1. Use `cube` as the simplest complete rendering-pipeline regression
      fixture. At zero object rotation, its distinct face colors, signed axis
      labels (`+X`, `-X`, and so on), and asymmetric decorations must
      unambiguously expose incorrect face orientation, axis direction, UV
      placement, and texture mirroring.
   2. Use `heavytank` as a complementary geometry-transform fixture. Its
      chirality about all three axes must distinguish geometry transform errors
      from UV or texture-sampling errors when those failures would otherwise
      compound and obscure one another.
   3. Exercise geometry very close to and crossing the camera plane.
   4. Exercise camera translation and rotation independently from object
      movement.
   5. Exercise intersecting geometry and depth ordering.
   6. Exercise back-face and occluded-face behavior.
   7. Exercise both textured and textureless triangles.
   8. Define controlled poses, expected images, deterministic captures, and
      explicit pass/fail criteria for every selected fixture.
   9. If automated image inspection cannot reliably interpret the
      human-oriented cube texture, design a second machine-oriented diagnostic
      texture whose spatial code makes axis, orientation, UV, and mirroring
      errors easy to classify, even if it resembles colored noise to a human.
10. [x] Capture repeatable native Pingo-target results for current Jet and
  `moveobj/tri`.
11. [ ] Compare selected target results with Alpha 7 or another explicitly
  accepted reference.
12. [ ] Define a repeatable timing workload and measurement method.
13. [ ] Record image dimensions, pixel format, model data, camera parameters,
  and build configuration.
14. [ ] Run the baseline against relevant historical revisions and branches.
15. [ ] Ensure future performance work cannot silently reintroduce the repaired
  perspective defect.

Deliverable: a compact correctness and performance regression suite.

## Phase 5A: Forward-port Pingo to VDP 2.16 and Fab

1. [x] Determine how Fab represents and loads VDP firmware.
2. [x] Explain the historical lag between official VDP and Fab VDP releases.
3. [x] Prove that current VDP 2.16 and the Pingo feature can compile and link
  as a native Fab shared object.
4. [x] Run an elementary native Pingo bitmap/control/render smoke test.
5. [x] Record the ownership, branch, implementation, and validation decision
  in [Decision Record 0001](decisions/0001-pingo-fab-vdp216-strategy.md).
6. [x] Preserve `47a6609` with an explicit archival ref.
7. [x] Create isolated `pingo-v2.16` and `pingo-v2.16-userspace` development
  worktrees without modifying upstream-owned checkouts.
8. [x] Import the coherent Pingo runtime onto official VDP 2.16.
9. [x] Add the narrow `0x49` and `0x22` VDU integration.
10. [x] Compile Pingo runtime sources explicitly in the native VDP build.
11. [x] Add an explicit ESP32/userspace allocation seam and remove the unused
  ESP-DSP host dependency.
12. [x] Repeat the elementary native render test without permissive C++ build
  shortcuts.
13. [x] Run the exact current `moveair/jet.bin` under Fab.
14. [x] Add deterministic native Pingo render-target capture at the external
  VDP seam.
15. [x] Capture byte-identical Jet targets at selected render ordinals across
  fresh Fab processes.
16. [x] Capture a byte-identical textured `moveobj/tri` target in fresh Fab
  processes.
17. [ ] Extend deterministic output to the selected simple correctness-scene
  set and compare it with accepted references.
18. [x] Build the VDP 2.16 Pingo firmware and record real flash/RAM use.
19. [x] Flash the modern VDP 2.16 Pingo image and obtain an Author report that
  all performed tests passed. The exact workload list still needs to be
  enumerated for a formal release checklist.
20. [ ] Harden invalid target ordering, ownership, teardown, and packed-pixel
  dithering only after compatibility is established.
21. [x] Demonstrate deterministic Pingo-target capture without a Fab fork.
22. [x] Create `bgates747/fab-agon-emulator`, clone it separately under
  `~/Agon/mystuff`, preserve the official checkout, and publish a `pingo`
  branch with a reproducible fixture launcher.
23. [x] Add build, regression, state-report, and upstream-report helpers to the
  Fab integration branch to automate the edit-build-run-test loop.

Deliverable:
[pingo-v216-validation.md](pingo-v216-validation.md), with a reviewable Pingo
feature on current VDP, matched native and ESP32 builds, and explicit visual
and hardware gates.

## Phase 6: Produce the “where I left it” report

1. [x] Summarize what is working and verified.
2. [x] Summarize what appears to work but remains unverified.
3. [x] List known defects and their reproduction steps.
4. [x] List abandoned experiments and why they were abandoned.
5. [x] List unfinished intended work.
6. [x] List historical unknowns that require the Author's recollection.
7. [x] Identify technical debt that is relevant to resuming development.
8. [x] Recommend the smallest useful next implementation milestone.
9. [x] Update the project handoff with durable conclusions.

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

1. [x] Which Pingo demo or scene best represents the last known-good state?
   `movecam` and `moveobj` are the canonical correctness-test families. Each
   uses one object and moves either the camera or the object—by translation,
   rotation, or both—but never both camera and object. `moveair` and
   `movefsim` exercise application-like behavior and are premature as primary
   correctness fixtures. `wolf` was an exploratory maze-shooter application;
   the Author's present assessment is that Pingo is not yet suitable for it.
2. [x] Was the failed magazine algorithm remembered as a particular article,
   author, or technique? It was Chris Hecker's work. The imported reference
   code is believed to be all the original code the Author could locate.
   `gradient.txt` contains substantial explanatory/derivation text and
   references the August/September article, while the source headers identify
   Hecker's historical homepage. No complete, clearly titled copy of the full
   magazine series has yet been identified.
3. [x] Was development stopped because of one blocking defect or primarily
   because the performance work became unproductive? Development stopped
   because the attempted optimizations could not be made to work; in
   particular, the Hecker optimization rendered garbage.
4. [x] What feature or optimization was intended to come next? No particular
   feature was planned. The governing goal was to raise the 320×240 spinning
   globe (`earthuv`) from roughly 3 FPS to at least 15 FPS, which the Author
   considers the minimum playable threshold.
