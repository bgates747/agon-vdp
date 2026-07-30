# Action notes from the adversarial renderer review

Date: 2026-07-29

Source review:

```text
docs/pingo-renderer-adversarial-review-2026-07-29.md
```

Claude reviewed the accepted incremental-depth checkpoint at `2c9acdb`.
These notes reassess its findings against `cc7aa96`, which additionally
contains the accepted cached object-AABB rejection. This is a follow-up
queue, not authorization to change the renderer immediately.

## Disposition

The review found no reason to roll back the accepted rasterizer or object
culling. Its reproducibility criticism remains valid for several historical
comparisons. The unsafe mesh/UV ingestion, near/eye-plane clipping, and depth
conversion findings have now been corrected and qualified as one isolated
correctness tranche. Later review items remain separate work.

## Action queue

1. [x] Make every mesh-array upload transactional and cross-validated.

   Position, position-index, mesh-UV, object-UV, and texture-index uploads now
   use staging allocations. Negative counts are rejected before conversion,
   non-triplet index counts are drained but not published, and a partial read
   or allocation failure preserves the previous component. Explicit counts
   and recomputed geometry/texture validity support arbitrary component
   upload order while preventing null, short, non-finite, or out-of-range
   arrays from reaching the renderer.

   Native validation and bridge smoke tests cover malformed triplets,
   out-of-range indices, absent UV data, safe non-renderability, complete
   payload consumption, and recovery after later valid uploads. Deterministic
   allocator-failure injection remains a worthwhile test-harness enhancement,
   but the production failure path is transactional and fail-closed.

2. [x] Clip triangles that cross the near/eye plane before perspective
   division.

   An allocation-free homogeneous Sutherland-Hodgman clipper now enforces
   `-W <= X,Y <= W` and `-W <= Z <= 0`, interpolates raw UVs at generated
   vertices, and triangulates the resulting polygon as a fan. With Pingo's
   production projection, the near half-space also guarantees positive
   projection `W`; a separate arbitrary eye epsilon is unnecessary.
   Projection additionally rejects non-finite or unsafe float-to-integer
   results.

   Native tests cover exact and adjacent plane values, one- and two-vertex
   crossings, large lateral coordinates, malformed values, winding, and
   attribute interpolation. The Cube, EarthUV, Jet, and Airliner near-plane
   emulator fixtures and the physical multi-object camera suite passed visual
   review without the former missing-band or screen-filling failure modes.

3. [x] Centralize safe depth quantization.

   Every 32-bit depth path now uses one checked quantizer. It handles the
   exact `1.0f` endpoint without multiplying by a float-rounded
   `UINT32_MAX`, rejects non-finite and out-of-range input, and preserves the
   established finite interior mapping. Endpoint, adjacent-float,
   NaN/infinity, fused/split equivalence, and comparison semantics are covered
   by the native suite.

4. [ ] Finish preserving historical benchmark evidence.

   The newly accepted object-AABB comparison is independently reproducible:
   all three raw B/A/B logs and its JSON report are now tracked. Several
   earlier accepted or rejected comparisons still reference raw logs that
   are present only in the live pingoasm tree. At the time of this note these
   include:

   ```text
   olimex-subdivided-affine-two-run-hardware-2026-07-29.log
   olimex-subdivided-affine-plus-incremental-depth-two-run-hardware-2026-07-29.log
   olimex-fixed16-two-run-hardware-2026-07-29.log
   subdivided-affine-candidate-two-run-hardware-2026-07-29.log
   ```

   Inventory every comparison JSON, verify each available raw log against
   the recorded SHA-256 before staging it, and identify genuinely missing
   sources rather than silently substituting another capture. Regenerate the
   one comparison that records absolute `/home/smith/...` source paths so it
   uses repository-relative provenance.

   Establish a durable policy: qualified raw hardware captures are tracked
   evidence, while transient listener logs remain ignored. Encode that policy
   in `.gitignore` or in the qualification procedure so future agents do not
   need to remember an unexplained `git add -f`.

5. [ ] Add a shared-edge and near-coplanar depth stress fixture.

   The triangle-span primitive itself has strong coverage, but the current
   suite does not isolate two triangles competing at a shared edge with
   almost equal depth. Build a planar two-triangle quad plus a nearly
   coplanar overlay. Exercise both draw orders, small depth separations,
   rotation, and near-plane motion. Compare color and depth hashes and inspect
   for cracks, double-draw instability, or z-fighting before changing depth
   precision or ownership rules.

6. [ ] Resolve the dead selectable-depth-width abstraction before attempting
   a smaller z-buffer.

   `depth_try_write()` assumes `PingoDepth` and 32 bits while the nominal
   `ZBUFFER16` and `ZBUFFER8` branches declare incompatible types and
   semantics. This does not affect the accepted build because only
   `ZBUFFER32` is enabled. Before experimenting with memory width, either
   remove the misleading selectors and state the 32-bit contract or make one
   generic depth type, quantizer, comparison direction, and test suite cover
   every supported width.

7. [x] Defer the `cos()`/`sin()` versus `cosf()`/`sinf()` observation.

   Projection and rotation matrices are not constructed in the fragment hot
   loop, and changing libm entry points can alter accepted matrices by a few
   bits. Revisit this only as part of a measured transform-precision tranche;
   it is not a free renderer optimization.

8. [x] Preserve the accepted object-AABB implementation.

   Current bounds publication is transactional for position uploads, invalid
   and nonfinite bounds fail open, the renderer checks source pointers, and
   the complete 1,447-frame state gate plus physical B/A/B review passed.
   None of Claude's findings demonstrates a false object rejection. The
   remaining upload and clipping work must be independently attributable and
   must not be smuggled into the accepted culling checkpoint.

## Order of attack

The preferred correctness order is:

```text
transactional mesh ingestion
    -> safe homogeneous clipping and finite projection
    -> centralized depth quantization
    -> coplanar/shared-edge fixture
```

Historical evidence preservation can proceed independently because it does
not change firmware. Selectable depth width and float-specific matrix
functions remain deferred design work.

## Accepted qualification

The combined correctness tranche passed:

1. Native ordinary and diagnostic smoke suites.
2. ASan/UBSan plus float divide-by-zero and float-cast-overflow checks.
3. The Pingo VDU command-surface guard.
4. Two deterministic 1,447-frame emulator captures.
5. Human visual review of Cube, EarthUV, Jet, and Airliner near-plane
   fixtures.
6. A verified Olimex firmware upload followed by one complete, contiguous
   867-frame physical multi-object capture and human visual approval.

All 324 ordinary single-model rotation frames remained color- and
z-buffer-identical to the accepted object-AABB checkpoint. The 517 changed
frames were confined to near-plane/offscreen and multi-object camera-crossing
workloads: 476 changed both color and depth, 41 changed depth only, and none
changed color alone. This is the expected locality for clipping and safe depth
endpoint corrections.

## Additional follow-up findings

These were discovered while auditing the reviewed paths but are not part of
the accepted clipping tranche:

1. [ ] Define texture-bitmap ownership or invalidate bound Pingo objects when
   a backing bitmap is cleared or replaced.
2. [ ] Bound the time spent draining a declared but truncated upload; repeated
   per-word serial timeouts can otherwise make malformed large payloads very
   slow to abandon.
3. [ ] Complete Pingo control teardown for owned arrays, maps, framebuffers,
   and z-buffers.
4. [ ] Add deterministic bridge-level allocator-failure injection.
5. [ ] Add the separately planned shared-edge and near-coplanar depth fixture
   before changing depth precision or edge ownership.
