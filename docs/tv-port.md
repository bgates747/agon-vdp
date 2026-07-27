# Latest-upstream Pingo / TurboVega Agon port

Status date: 2026-07-27

The `tv-port` branch is the clean point of departure for renewed Pingo
development. It combines the current canonical Agon VDP with the current
upstream Pingo renderer while exposing only the Agon-facing feature surface
present at TurboVega's final `pingo3D` commit. It intentionally contains none
of the later `bgates747/agon-vdp` Pingo enhancements or extensions.

## Exact source pins

1. Canonical Agon VDP base:
   `AgonPlatform/agon-vdp` branch `main`, commit
   `c7ac293d2aa81ddfa693390549bcd909069c8fc3` (VDP 2.16.0).
2. Agon integration reference:
   `TurboVega/agon-vdp-otf` branch `pingo3D`, commit
   `f4814813e8155780c5ad2602cd45f82ca5a72eec`.
3. Pingo renderer:
   `fededevi/pingo` branch `master`, commit
   `f171c81aa597436e8db8fafd842ab7af6ef13b83`.
4. The upstream Pingo repository publishes no tags or GitHub releases.
   `f171c81` is therefore a pinned development-branch tip, not a versioned
   release.

The active `video/pingo/math` and `video/pingo/render` trees came from upstream
`f171c81`. The only renderer configuration change is selecting
`PINGO_PIXEL_RGBA8888`, because Agon RGBA8888 bitmap memory is byte ordered
R, G, B, A. Upstream defaults to B, G, R, A.

## Strict TurboVega protocol surface

The only Pingo envelope is:

```text
VDU 23, 0, &A0, scene_id; &49, subcommand; arguments...
```

Arguments after the one-byte subcommand are little-endian 16-bit words.

1. `0`: create control.
2. `1`: define mesh vertices.
3. `2`: set mesh vertex indexes.
4. `3`: define mesh-owned UV coordinates.
5. `4`: set mesh-owned UV indexes.
6. `5`: create an object from an object ID, mesh ID, and texture bitmap ID.
7. `6`–`17`: object scale, rotation, and translation.
8. `18`–`25`: camera rotation and translation.
9. `26`–`37`: scene scale, rotation, and translation.
10. `38`: render to an existing bitmap.
11. `39`: TurboVega's outer delete route; its deinitializer remains
    incomplete.
12. `40`: define an object-local UV-coordinate override. UV indexes remain
    mesh-owned through command `4`.

Run `python3 scripts/check_pingo_scope.py` to verify the dispatch whitelist.

The following later-local commands are deliberately absent:
`41`, `42`, `129`, `130`, `141`, `145`, `149`, and `153`. Commands `3` and
`4` retain TurboVega's mesh semantics rather than the later local object
semantics.

## API adaptation

Latest Pingo changed its C API after TurboVega's port:

1. `Scene` was replaced by callback `Renderable` objects and transformed
   `Entity` objects.
2. Object transforms moved from `Object` to `Entity`.
3. `BackEnd` became `Backend` and lost TurboVega's client-data field.
4. Renderer entry points became snake-case functions.

The Agon bridge adapts those changes without restoring the old renderer:

1. A small root `Renderable` iterates at most 32 objects in ascending object-ID
   order, preserving TurboVega's scene limit and ordering.
2. Each Agon object owns an upstream `Object` and `Entity`.
3. Each object also owns a shallow mesh view. Command `40` substitutes only
   that view's UV-coordinate pointer, retaining shared mesh geometry and UV
   indexes without modifying upstream `Object`.
4. An `AgonPingoBackend` embeds upstream `Backend` as its first member and
   carries the owning control pointer beside it.
5. The latest C headers use `this` as a parameter identifier. The C++ bridge
   temporarily macro-renames that identifier while parsing the headers; the
   upstream C sources remain unchanged.
6. Rendering still uses a private four-byte PSRAM frame and four-byte depth
   buffer, then copies the completed frame into the requested VDP bitmap.

## Deliberate safety and correctness decisions

These are fixes within TurboVega-supported functions, not added features:

1. Texture and output bitmaps are rejected unless their FabGL format is
   `RGBA8888`. This enforces TurboVega's documented four-byte contract and
   prevents four-byte reads or writes through later one-byte `RGBA2222`
   bitmaps.
2. TurboVega's command `8` and `28` copy/paste errors wrote Z-only scale values
   into Y. This port writes the supported Z scale as documented.
3. The host-only `pingo/assets/obj2vdu.c` converter is excluded from firmware
   compilation. Its large globals otherwise collide with the embedded teapot
   symbols at link time.

No attempt has yet been made to repair TurboVega's incomplete command `39`
teardown, borrowed bitmap lifetime, allocation-failure continuation, or
unvalidated geometry indexes.

## Validation

Latest upstream Pingo was built independently with GCC 13.3. Its six math test
groups passed. The suite has no automated renderer correctness assertions.
Warnings remain in upstream code for strict-aliasing in `object.c`, unused
projection-extraction variables, and an implicit test-runner declaration.

The combined ESP32 firmware builds cleanly apart from the inherited Arduino
UART missing-return warning:

```bash
~/Agon/mystuff/agon-vdp/.venv/bin/pio run
```

Initial `tv-port` build result:

```text
RAM:     44,688 / 327,680 bytes (13.6%)
Flash: 1,095,777 / 1,310,720 bytes (83.6%)
firmware.bin SHA-256:
31f5c001f17533e0b73ee40eba2801f0f67de25eff4ef14dfaa19ef12aff3b1a
```

The final ELF SHA-256 is
`d3a26cf2e6db2227b2777cc7f20c3127d9cdbfbe3474a3d2ac641d8d02176ba0`.
The firmware identifies itself as `Pingo TV Port 2.16.0`.

## Runtime observations

The earlier isolated prototype was flashed successfully and booted normally.
The new canonical-VDP `tv-port` branch has compiled but has not yet been
flashed.

1. Jukebox runs successfully. It is not a complete VDU exercise, but it is a
   useful smoke test against a fundamental non-Pingo firmware regression.
2. TurboVega's `video/pingo/assets/teapot.bas` remains alive and repeatedly
   renders, but its image is garbage resembling a motion-streaked line. This
   points toward a deterministic 3D transform/projection/bitmap-path defect
   rather than an immediate crash or random memory corruption.
3. TurboVega's exact final firmware subsequently passed `teapot.bas` and
   `orientation.bas` on hardware. This confirms that those fixtures do not
   expose all general pipeline defects.
4. The strict triangle, cube, and HeavyTank RGBA8888 move-object fixtures run
   on TurboVega's exact final firmware. Triangle and cube expose the inherited
   perspective-incorrect texture interpolation. HeavyTank additionally
   renders upside down and appears inside out during rotation, although its
   historically post-processed model data remains an independent variable.

The strict fixtures now live durably in
`~/Agon/mystuff/pingoasm/apps/turbovega`. Their `src/` directory is tracked;
their assembled binaries and RGBA8888 textures live in the ignored `tgt/`
directory.

## Upstream findings to test, not assume

Latest upstream includes the camera-view inversion, projection, and depth
changes TurboVega never saw. It also still has no geometric near-plane
clipping. Its Entity root behavior, zero-length Array initialization,
texture/index validation, and advertised early-Z switch contain suspicious or
incomplete paths.

Consequently, successful compilation proves the port boundary and API
translation—not visual correctness. The cube/orientation fixtures must decide
whether latest upstream behavior is correct.
