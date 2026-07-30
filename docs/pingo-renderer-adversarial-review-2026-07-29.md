# Adversarial Review: Pingo Renderer Work

Date: 2026-07-29

This is a cold, hostile-in-the-constructive-sense review requested via
`~/Agon/mystuff/golem/docs/devlog/from_codex.md` (section 12, "Request:
adversarial review of the Pingo renderer work"). It covers the accepted
rasterizer checkpoint and the evidence supporting it. Please treat this as
a request for questions or defenses, not as an instruction to change code.

**Reviewed states:**

- `~/Agon/mystuff/agon-vdp`, branch `experiment/hecker-rasterizer`,
  commit `2c9acdb` "Advance Pingo depth incrementally"
  (`origin/experiment/hecker-rasterizer`).
- `~/Agon/mystuff/pingoasm`, branch `main`, published `8cd5bea`, local
  (unpushed at review time) `7383f11` "Record cumulative Pingo performance
  gains".

**Method:** both states were reproduced in isolated detached `git worktree`
checkouts outside the two working trees above (`/tmp/pingo-review/agon-vdp`
and `/tmp/pingo-review/pingoasm`). Neither shared working tree was modified
by this review.

## Must fix

### 1. Headline performance claims are not independently reproducible

Every quoted hardware A/B result (e.g. the accepted "+3.40% weighted gain"
for incremental depth over the subdivided-affine checkpoint, "+73%" versus
`working-pre-hecker`) is backed by a JSON summary that hash-pins two raw
two-run `.log` files. Those `.log` files are **globally gitignored** in
`pingoasm` (`.gitignore:91: *.log`, confirmed via `git check-ignore -v`).

Checking every `2026-07-29` comparison JSON under
`pingoasm/benchmarks/render-spin/results/` against what actually exists on
disk:

- `olimex-subdivided-affine-two-run-hardware-2026-07-29.log`,
  `olimex-subdivided-affine-plus-incremental-depth-two-run-hardware-2026-07-29.log`,
  `olimex-fixed16-two-run-hardware-2026-07-29.log`, and three others are
  **absent** from git history and from the separate
  `~/Agon/mystuff/pingo-firmware-archive` (they exist only, untracked, in
  the live `~/Agon/mystuff/pingoasm` working tree).
- Only `exact-span-candidate-two-run-hardware-2026-07-29.log` happens to be
  untracked-but-present in that same tree; `subdivided-affine-candidate-two-run-hardware-2026-07-29.log`
  is referenced by one JSON via a hardcoded absolute path into the author's
  live tree rather than a repo-relative path.

A reviewer following this handoff's own instruction to reproduce the
reviewed states outside the working trees cannot verify the per-frame data
behind the acceptance decision — only the aggregated JSON numbers, which
must be trusted at face value, and which would be permanently unrecoverable
if the author's local `results/` directory were ever cleaned.

**Recommendation:** commit the raw logs (or a checksummed archive of them,
as was done for `cb91c12` under `pingo-firmware-archive`) alongside the
derived JSON, or explicitly document that raw logs are ephemeral/local-only
and that the JSON summary is the sole checked-in source of truth — in which
case the recorded SHA-256 fields should say so, since they currently imply
independent verifiability that isn't actually available.

### 2. Unsafe mesh-upload path, and directly exploitable by the *next* planned experiment

`video/pingo_3d.h`, `define_mesh_vertices()` (~line 498): frees
`mesh->positions`, then on `heap_caps_malloc` failure logs and
**continues**, leaving `mesh->positions == NULL` while still draining the
full vertex stream from the wire. `set_mesh_vertex_indexes()` (~line 531)
has the same allocation-failure pattern for `pos_indices`, and sets
`mesh->indexes_count = n` **without validating `n % 3 == 0`**, and without
validating that index values fall within the uploaded vertex count.

`renderObject()` in `video/pingo/render/renderer.c` dereferences
`o->mesh->positions[o->mesh->pos_indices[i+0]]` with **no NULL check and no
bounds check**. A failed PSRAM allocation (plausible — a distinct
allocation-failure crash for bitmaps, via `createEmptyBitmap()` /
`bufferWrite()`, is already documented in
`pingoasm/docs/devlog-2026-07-29.md`) or a malformed/corrupted mesh asset
leads directly to a null or wild-pointer dereference on the next render,
rather than a controlled failure.

This is squarely in the blast radius of the next authorized experiment
(object-level AABB frustum culling): a bounds cache computed at upload time
must be explicitly invalidated on this exact allocation-failure path, or it
will cache stale/zero bounds for a mesh that cannot safely render at all —
this is precisely the "bounds-cache invalidation and partial/failed
uploads" attack surface the handoff asked to have attacked.

### 3. Near/eye-plane straddling triangles risk NaN/Inf undefined behavior

`triangleOutsideRemainingClipPlanes()` (introduced at `cb91c12`, i.e. before
the Hecker rasterizer work, but load-bearing for the coordinate/clip
contract this review was asked to challenge) only rejects a triangle
**wholly** behind the eye (`a.w<=0 && b.w<=0 && c.w<=0`) or wholly outside
one common plane. There is no general polygon clipping. A triangle with
mixed `W` sign (straddling the eye) is neither rejected nor clipped:

- `a.w = 1.0 / a.w` can yield `Inf`/`NaN` when `w` is near zero.
- `Vec2i a_s = { a.x*halfX+halfX, ... }` then performs a **float-to-int
  conversion on a NaN/Inf value — undefined behavior in C**.
- NaN comparisons are always false, so the backface reject
  (`clocking >= 0`) does **not** catch these triangles either; they fall
  through to rasterization.
- The same class of bug hits `depth_try_write()` in
  `video/pingo/render/depth.h`: `(uint32_t)(value * (float)UINT32_MAX)` is
  undefined behavior when `value` rounds to exactly `1.0`, because
  `(float)UINT32_MAX` itself rounds up to `2^32`, one past the
  representable range of `uint32_t`.

The benchmark suite already contains "near-plane" fixtures (Cube/EarthUV
near-plane) exercised for **performance timing**; nothing in the reviewed
evidence indicates the exact crossing frame was checked for **visual**
correctness (screen-filling garbage or flicker), which is the realistic
symptom of this gap.

## Worth measuring

- **Shared-edge double-draw** is deliberate and inherited (see the comment
  in `triangle_span.h`: "deliberately preserving Pingo's current shared-edge
  ownership"). The per-triangle span logic itself is exhaustively verified:
  `userspace/pingo_triangle_span_test.c` brute-forces every small
  triangle/viewport combination against a reference oracle, plus 200,000
  randomized fuzz cases and explicit `INT32_MIN` edge cases. The remaining
  risk is cross-triangle: incremental depth's documented drift (max
  1,792/2^32 ≈ 4.2e-7 normalized) interacting with near-coplanar/z-fighting
  geometry at shared edges — worth a dedicated coplanar-quad stress fixture
  rather than treating the current 1,268/1,447-frame hash-diff count as
  sufficient evidence on its own.
- Inconsistent path styles in the benchmark result JSONs (repo-relative vs.
  one hardcoded absolute author-machine path) are a smaller symptom of the
  same reproducibility gap as finding #1.

## Interesting but speculative

- `depth_try_write()` (the new fused test+write helper in `depth.h`)
  duplicates and bypasses the macro-selected `depth_write()`/`depth_check()`
  in `depth.c`, hardcoding 32-bit width regardless of the
  `ZBUFFER32`/`ZBUFFER16`/`ZBUFFER8` selector. If `ZBUFFER16` or `ZBUFFER8`
  were ever selected, this code wouldn't even compile (those branches
  typedef a struct named `Depth`, not `PingoDepth`). Currently a dead path
  since only `ZBUFFER32` is defined, but worth cleaning up before anyone
  touches buffer width.
- `mat4Perspective()` mixes double-precision `cos()` and `2.0` literals into
  a `float` result — harmless today (computed once per camera update, not
  per-pixel), but worth a note if `F_TYPE`/precision is ever scrutinized for
  the hot path.

## Verified accurate

- `953ec2b` (revert of the rejected fixed-point/Hecker signed-16.16
  experiment) is **byte-identical** to the pre-experiment tree
  (`git diff 7a1f9ba 953ec2b` is empty) — the "cleanly recoverable, no
  residue" claim holds.
- The `depthStepX` algebra added in `2c9acdb` is algebraically identical to
  the unchanged per-pixel expression (uses the `A01 = -(A12+A20)` identity,
  the same technique already used for `textureStepX`) — the reformulation
  itself introduces no correctness change.
- `set_render_notification()`'s mode/token parsing and disable-on-unsupported-mode
  behavior in `pingo_3d.h` match the async-completion handoff's description
  exactly.
- Object-level AABB frustum culling does **not** yet exist at `2c9acdb`
  (confirmed by search) — consistent with it being the *next*, not-yet-implemented
  experiment rather than something already shipped.
