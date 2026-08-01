# Pingo on Agon VDP 2.15

Status: Pingo 2.15.0 Alpha 1, hardware and emulator qualified on 2026-07-27.

## Provenance and branches

The immutable `tv-port` branch is the historical control:

1. Agon VDP tag `v2.15.0`, commit
   `5e628a81d2a1329cf33873b783684f1b7d9d8b7f`.
2. TurboVega `pingo3D` final commit
   `f4814813e8155780c5ad2602cd45f82ca5a72eec`.

TurboVega supplied `video/pingo/math`, `video/pingo/render`, and
`video/pingo_3d.h`. The unused embedded teapot include was removed for
packaging. No later `fededevi/pingo` renderer was merged.

Active correction work is on `pingo-codex`. Alpha 1 preserves TurboVega's VDU
commands `0` through `40`; the hardware-qualified callback milestone adds
opt-in render notification command `41`. The rendering corrections are:

1. Camera VDU transforms are poses; the bridge inverts the pose once to create
   the view matrix.
2. Viewport conversion reflects world/NDC `+Y` into top-origin target memory.
3. Rasterizer edge tests accept the corresponding screen-space area sign.
4. Texture sampling converts bottom-origin UV V to top-row-first image memory,
   clamps endpoints, and uses the actual texture height.
5. UV interpolation uses `u/w`, `v/w`, and `1/w`.

The boot banner is:

```text
Agon Pingo VDP Version 2.15.0 Alpha 1 SEP Field
```

`SEP Field` is the upstream VDP 2.15 release name; `Pingo` is the only variant
branding.

## Coordinate contract

1. The world is right-handed: `+X` right, `+Y` up, `+Z` toward the viewer.
2. An unrotated object or camera faces `-Z`.
3. Object, scene, and camera VDU transforms all describe poses.
4. Render-target memory is top-left origin with `+Y` downward; the viewport
   performs the sole geometry Y reflection.
5. UVs use Blender/OBJ convention (`v=0` bottom); RGBA memory is top-row first,
   so the sampler performs the sole texture Y conversion.
6. Existing Pingo matrix storage and multiplication behavior remain unchanged.

## Protocol guard

The envelope is:

```text
VDU 23, 0, &A0, scene_id; &49, subcommand; arguments...
```

Arguments after the subcommand are little-endian 16-bit words. Run after
bridge changes:

```bash
python3 scripts/check_pingo_scope.py
```

The immutable `tv-port` branch contains only TurboVega commands `0`–`40`.
`pingo-codex` additionally permits the qualified render-notification command
`41`. Other commands from later local ports (`42`, `129`, `130`, `141`, `145`,
`149`, and `153`) must not appear.

## Embedded build and flash

Use the project-local environment:

```bash
~/Agon/mystuff/agon-vdp/.venv/bin/pio run --target clean
~/Agon/mystuff/agon-vdp/.venv/bin/pio run
~/Agon/mystuff/agon-vdp/.venv/bin/pio run --target upload
```

Alpha 1 uses 42,520 bytes RAM (13.0%) and 1,063,717 bytes flash (81.2%).

## Native emulator module

Build and smoke-test the exact same sources:

```bash
make -C ~/Agon/mystuff/agon-vdp/userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator smoke
```

Refresh the isolated project profile only after hardware passes:

```bash
cd ~/Agon/mystuff/agon-dev-env
python3 scripts/setup_emulator.py pingo-tv-baseline \
  --refresh-baseline-vdp
```

The historically named profile lives at
`~/Agon/mystuff/pingoasm/emulators/tv-port-baseline`. Its copied module and
manifest form the current qualified comparison snapshot; it does not alter the
older extended-Pingo emulator.

## Qualification fixtures

Canonical clients and assets live in
`~/Agon/mystuff/pingoasm/apps/turbovega`. Test hardware before the emulator:

1. Triangle checks the smallest textured geometry path.
2. Cube checks axes, face selection, UV orientation, and perspective.
3. HeavyTank checks winding and compounded perspective with chiral geometry.
4. Jukebox is a non-Pingo VDP smoke test.

Alpha 1 passed cube and HeavyTank on hardware and the isolated emulator. The
fresh canonical HeavyTank OBJ is outward-wound; the historical OBJ was
inward-wound and caused the apparent inside-out rendering. Numbered and
axis-modified HeavyTank experiments were removed.

The opt-in diagnostic firmware now attributes render time and records
object, triangle, and fragment outcomes without changing the ordinary build.
See [Pingo render diagnostics](pingo-render-diagnostics.md) for the schema,
qualification limits, and capture workflow.

The current cached object-bounds experiment advances the diagnostic wire
record to closed schema 3. The `working-pre-optimization` tag remains the
schema-1 control; schema 2 adds the distinct whole-triangle frustum rejection
counter, and schema 3 adds object bounds-test, object-frustum-rejection, and
avoided-triangle counters. The pingoasm parser accepts all three complete
schemas and rejects mixed or incomplete records.

The same experimental branch has now screened modern upstream Pingo without
adopting its incompatible `Entity` object model. The emulator-qualified
candidate retains only:

1. the guarded normalization and translation-only inverse ideas from upstream
   `fb67d951c4f05ca5fa04c45dcb3cb3b02f861163`; and
2. a locally derived, once-per-object view/projection composition inspired by
   upstream transform-composition lineage (`a0ed0cb`), while preserving this
   port's model-space lighting and camera-pose contract.

Native math tests cover the new invariants. A temporary Cube probe found the
final 76,800-byte RGBA2222 Pingo target byte-identical before and after the
upstream sweep. The exact candidate subsequently completed the 1,447-record
hardware regression chain with no panic, reboot, sequence gap, or duplicate.
All fifteen fixtures improved, from 0.42% on HeavyTank to 8.41% on the
multi-object camera-dolly scene. The ordinary build uses 42,520 bytes of RAM
and 1,065,125 bytes of flash; its flashed `firmware.bin` has SHA-256
`c65e4ec31272ddecb0f04a2cb9f097c97e62789215bce392d1fa9cbba0ec6fd7`.

The remaining renderer priorities are robust malformed-input handling,
near-plane clipping, objective image tests, and measured optimization toward
the 15 FPS globe target.
