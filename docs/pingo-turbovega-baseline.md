# TurboVega Pingo Point-of-Departure Audit

Date: 2026-07-26

Status: direct ancestry, source boundary, original protocol, and clean-build
status verified. Hardware behavior has not yet been rerun.

This document identifies the exact TurboVega code from which the local Pingo
work departed. It supplements
[pingo-reconstruction-state.md](pingo-reconstruction-state.md).

## Reference checkouts

### TurboVega Agon port

- Checkout: `/home/smith/Agon/TurboVega`
- Remote: `https://github.com/TurboVega/agon-vdp-otf.git`
- Branch: `pingo3D`
- Commit: `f4814813e8155780c5ad2602cd45f82ca5a72eec`
- Commit date: 2024-06-28
- Tree: `5dc04f99bda8815238b4af9efaf8125a6bf91a14`
- Working tree: clean

The checkout is an external-developer reference under `/home/smith/Agon` and
must remain read-only.

### Original Pingo

- Checkout: `/home/smith/Agon/pingo`
- Remote: `https://github.com/fededevi/pingo.git`
- Branch: `master`
- Current checkout commit: `f171c81aa597436e8db8fafd842ab7af6ef13b83`
- Historical source fingerprint:
  `216d2db1216b91797ff9672b909da9ae900fd170`
- Historical commit date: 2022-08-23
- License: CC0-1.0
- Working tree: clean

This is also an external read-only reference. Current `master` is useful for
later comparison, but it is not the source version imported by TurboVega.

## Exact relationship to this repository

This is not a copied or merely similar source tree. The exact commit and tree
objects from TurboVega's `pingo3D` tip already exist in this repository:

```text
TurboVega pingo3D tip       f481481...  tree 5dc04f9...
local pingo merge base      f481481...
first local child           ac6fcc9...
current local pingo tip     47a6609...
```

`git merge-base --is-ancestor f481481 pingo` succeeds. The first local commit,
`ac6fcc92829aa4d5424de632fe71d0143ba8522e`, has `f481481` as its sole parent
and was authored as `bgates747`.

There are 60 graph commits, 38 first-parent commits, and 50 commits touching
the selected Pingo and integration paths between the TurboVega tip and current
`pingo`. The exact historical boundary is therefore:

- through `f481481`: TurboVega/Curtis Whitley's port and demos;
- from `ac6fcc9`: the local continuation.

The tip commit itself only adds the Lara demo. Its parent `f0a1292` has the
same firmware renderer.

## Source layers

The surviving history provides three distinct authorship layers.

### Imported Pingo core

Commit `dacb520` added 38 files and 5,517 lines from `fededevi/pingo`,
including:

- vectors and matrices;
- scenes, renderables, objects, meshes, materials, and textures;
- depth helpers;
- the 2D rasterizer;
- the original 3D triangle renderer;
- the Pingo CC0-1.0 license and README.

No exact `fededevi/pingo` commit ID was recorded in the import commit, and no
upstream commit is byte-identical to all 38 files because `dacb520` already
contains small port adaptations. A complete-history comparison identifies
`216d2db` as the high-confidence source baseline:

- the README and license match exactly;
- every math file matches exactly;
- the unchanged portions of the render library match;
- remaining differences are recognizable Agon/ESP32 adaptations, include-path
  relocation, pixel/debug changes, and teapot UV data.

Four immediately earlier commits share the same relevant core tree, but
`216d2db` is the strongest overall fingerprint because its README also
matches exactly. The next upstream changes replaced example assets and later
began a major API refactor.

Upstream commit `a0ed0cb` from 2023 independently changed the projection
matrix, inverted the camera transform, and altered depth handling. That work
was developed on an unmerged refactor branch and did not reach upstream
`master` until merge `05761ad` on 2024-07-01, three days after TurboVega's
final tip. It was therefore not part of the Agon point of departure.

This later upstream implementation is useful comparative evidence, not an
automatic replacement: it still performs whole-triangle frustum rejection
rather than clipping and reconstructs UVs using post-divide Z rather than a
clearly preserved `1/w`.

### TurboVega's Agon port

Starting at `841c416`, TurboVega added the Agon integration and developed it
through `f481481`:

- the `VDU 23,0,&A0,...,&49` command bridge;
- `Pingo3dControl` stored in a VDP writable buffer;
- mesh and object maps;
- PSRAM-backed geometry, UV, frame, and depth storage;
- VDP bitmap binding for textures and render targets;
- camera, scene, and object transforms;
- the external command document;
- teapot, orientation, fighter, and Lara assets/demos;
- an OBJ-to-VDU conversion experiment;
- object-local UV-coordinate overrides.

TurboVega changed the experimental buffered selector from `&48` to `&49` in
`4e22ecc`. Commit `8e547a3` repaired color-channel multiplication, `c7bfc58`
added object-local UV coordinates, and `f0a1292` changed the light's Y
direction.

### Local continuation

The local work initially retained the inherited renderer while extending
transform behavior, camera tracking, UV ownership, Y orientation, dithering,
and textureless output. The alpha-6 work then replaced the 3D triangle path
with the Scratchapixel-derived perspective implementation. Alpha 7 introduced
packed pixels, direct-to-bitmap rendering, persistent renderer/camera state,
background clearing, and scanline-restricted traversal.

## Original external contract

### Wire envelope

```text
VDU 23, 0, &A0, sceneBufferId; &49, subcommand, payload...
```

The original command families were:

| Commands | Original meaning |
| --- | --- |
| 0 / 39 | Create or delete the buffer-backed control |
| 1 / 2 | Upload mesh vertices and vertex indexes |
| 3 / 4 | Upload mesh UV coordinates and UV indexes |
| 5 | Create an object from object, mesh, and texture-bitmap IDs |
| 40 | Override an object's UV-coordinate array |
| 6–17 | Object scale, rotation, and translation |
| 18–25 | Camera rotation and translation |
| 26–37 | Scene scale, rotation, and translation |
| 38 | Render to the supplied bitmap ID |

Words are little-endian 16-bit values; the subcommand is one byte. Index
counts are counts of indexes, not triangles, and the renderer consumes them
in triples.

### Numeric conventions

- positions: signed word divided by 32767;
- UVs: unsigned word divided by 65535;
- scale: unsigned word divided by 256;
- rotation: signed word multiplied by `2*pi/32767`;
- translation: signed word multiplied by `256/32767`;
- visible geometry: negative camera-space Z;
- projection: near 1, far 2500, FOV 0.6;
- original camera transform: used directly as the view matrix, not inverted;
- original screen mapping: positive projected Y mapped downward.

The original port selected four-byte `BGRA8888` in `pixel.h`, although the
actual structure fields are declared `r, g, b, a`. Texture and output bitmap
storage were borrowed from the VDP bitmap system.

### Original render path

```text
object and scene matrices
  -> supplied camera transform
  -> perspective projection
  -> reject only if all projected vertices are behind the camera
  -> perspective divide
  -> projected-winding cull
  -> screen-clamped triangle bounding box
  -> integer edge-function barycentrics
  -> normalized 32-bit depth test
  -> perspective-style UV reconstruction
  -> lighting and four-byte framebuffer write
  -> copy the intermediate frame to command 38's bitmap
```

The active 3D rasterizer was already `renderObject()` in `renderer.c`.
`rasterizer.c` primarily handled 2D sprite/texture transforms. There was no
geometric near-plane or frustum clipping at the point of departure.

## Inherited versus later changes

| Area | Inherited from TurboVega | Added or replaced later |
| --- | --- | --- |
| VDU bridge | Entire buffer-backed `&49` dispatch and control lifecycle | Local-transform, camera-track, and dithering commands |
| State | Mesh/object maps, transform state, PSRAM arrays, borrowed texture pointers | Persistent renderer and camera; direct target/background pointers |
| Transforms | Wire conversions and scale/rotation/translation composition | Local transforms, camera tracking, inverse/view corrections |
| Rasterization | Full bounding-box barycentric triangle traversal | Scratchapixel path, then scanline-restricted traversal |
| Clipping | Whole-triangle behind-camera rejection only | Different near tests and Z clamps, but still no geometric clipping |
| Depth | Imported normalized 32-bit helper and interpolated projected depth | Repaired reciprocal-depth interpolation and moved ownership |
| UVs | Normalized UVs, mesh UV/index arrays, optional object UV array | Object-owned UV indexes, V inversion, repaired interpolation |
| Pixels | Four-byte intermediate frame and final copy | One-byte `AABBGGRR`, then direct-to-bitmap rendering |
| Targets | Command 38 honored its bitmap ID | Target/background hard-wired to bitmaps 257/258 |
| Performance | Per-frame renderer, backend callbacks, lighting, frame copy | All recorded September optimizations |

## Protocol drift after the fork

The current `video/pingo/vdu_pingo.md` and bundled BASIC demos are
byte-identical to the TurboVega tip. They describe the original contract, not
the current implementation.

The decisive changes were:

- `5c75c3a`: commands 3/4 became object-owned UV coordinates/indexes and
  command 40 disappeared;
- `14c5779`: pixels became one-byte packed `AABBGGRR`;
- `376dd75`: rendering became direct to hard-wired bitmap 257;
- later initialization optionally borrowed bitmap 258 as a background;
- commands 41, 42, 141, 145, 149, and 153 were added without updating the
  original document.

The old demos often use the same numeric value for mesh ID and object ID,
which masks the command 3/4 framing change. They also create their output
bitmap after the control structure and render to full legacy bitmap IDs such
as 64102, which is incompatible with the current direct-target initialization.

The embedded document and BASIC files must therefore be labelled historical
until they are rewritten and verified against the present protocol.

## Demo and tool reliability

- `teapot.bas` is the smallest coherent original flow.
- `orientation.bas` is the strongest original mesh-reuse example: one arrow
  mesh, three objects, three solid textures, and independent axis rotations.
- `lara.bas` is unfinished as a validation program. It defines UV data but
  does not upload it in the expected sequence.
- `obj2vdu.c` is unfinished: two conversion helpers multiply an uninitialized
  local value instead of their coordinate parameter.
- `render.png` is an 800x600 illustrative image introduced before the port
  first rendered a teapot. Its chronology prevents treating it as captured
  output from the final TurboVega tip.

## Defect provenance

### Already present at the TurboVega boundary

- empty deinitialization and leaked subordinate allocations;
- borrowed bitmap pointers without retained lifetime ownership;
- no stored vertex/UV counts for index validation;
- object Z-only scale mistakenly writes Y;
- scene Z-only scale mistakenly writes Y;
- texture V wrapping uses texture width instead of height;
- fixed 32-renderable scene capacity with ignored overflow;
- no geometric near/far/frustum clipping;
- the claimed perspective correction divides UV by post-divide Z instead of
  preserving and interpolating `u/w`, `v/w`, and `1/w`;
- setters can create incomplete mesh/object placeholders that the renderer
  later dereferences;
- allocation failures are logged but initialization continues;
- render-target dimensions are checked but its four-byte pixel format is not;
- degenerate world-space triangles can be normalized before the later
  screen-space zero-area rejection.

### Introduced or exposed later

- stale command documentation after object-owned UV remapping;
- ignored render bitmap ID and hard-wired target/background IDs;
- four-byte dithering routines applied to a one-byte packed framebuffer;
- persistent framebuffer/Z-buffer ownership and replacement hazards;
- the persistent renderer's scene pointer left referring to a temporary scene
  after each render;
- a non-void `shade()` fall-through when texture pixels are absent;
- disabled lighting/culling paths and incomplete DSP substitutions.

This attribution matters: the project did not create every unsafe area during
the later perspective and optimization work. Some are debts inherited from
the original experimental port; others are regressions from deliberate later
changes.

## Clean-build evidence

A Git archive of `f481481` was compiled in
`/tmp/turbovega-pingo3d-build-jQC5oT`; the reference checkout remained clean.
The same PlatformIO Core 6.1.19 and Espressif32 6.6.0 toolchain were used, with
TurboVega's `vdp-gl#transformed-bitmaps` dependency resolving to `c161014`.

```text
RAM:       42,416 / 327,680 bytes (12.9%)
Flash:  1,011,941 / 1,310,720 bytes (77.2%)
Image:     1,012,320 bytes
SHA-256:   6ce49214a8d43dccf177731aca386eed1ec89448b73c1a6d53a5694fc80e6b7d
```

The link completed with warnings that the global symbol `tex_coords` has
different alignment and size in `obj2vdu.c` and `teapot.c`. This reinforces
that the converter source should not have been part of the firmware build.
There was also the same third-party Arduino UART return warning seen in the
current build.

The successful build proves present-day source/toolchain compatibility. It
does not prove historical or current hardware correctness.

## Implication for reconstruction

The most useful historical runtime sequence is now:

1. TurboVega `f481481` with `teapot.bas` and `orientation.bas`;
2. local alpha 5 `b2d45a4`;
3. known-good alpha 6 `6f18f26`;
4. current alpha 7 `47a6609`.

That sequence isolates the inherited port, the local transform/UV extensions,
the perspective repair, and the performance/refactoring endpoint. Each
revision requires its matching demo protocol and bitmap assumptions; current
demos cannot be used unchanged against every firmware revision.
