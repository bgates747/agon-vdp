# TurboVega Pingo port on VDP 2.15

Status date: 2026-07-27

The `tv-port` branch has one deliberately narrow purpose: establish a clean,
reproducible baseline consisting of canonical Agon VDP 2.15 plus TurboVega's
Pingo port exactly as he last left it.

It must not contain later Pingo renderer innovations, whether from
`fededevi/pingo` or from the later local Pingo development branches. Those
changes can only be evaluated after this baseline passes its hardware
fixtures.

## Exact source pins

1. Canonical VDP base: `AgonPlatform/agon-vdp` tag `v2.15.0`, commit
   `5e628a81d2a1329cf33873b783684f1b7d9d8b7f`.
2. Pingo renderer, math library, and Agon bridge:
   `TurboVega/agon-vdp-otf`, branch `pingo3D`, final commit
   `f4814813e8155780c5ad2602cd45f82ca5a72eec`.

The following paths are restored from TurboVega's final commit:

1. `video/pingo/math/`
2. `video/pingo/render/`
3. `video/pingo_3d.h`

The renderer and math directories are byte-for-byte TurboVega-final sources.
The bridge differs in one packaging-only respect: its unused include of
`pingo/assets/teapot.h` is removed because sample assets are not embedded in
this firmware tree. No transform, projection, rasterization, or protocol
behavior is changed.

TurboVega's host-side sample assets are maintained separately as test
fixtures. They are not renderer source and are not required to build the
firmware.

## Strict TurboVega protocol surface

The Pingo envelope remains:

```text
VDU 23, 0, &A0, scene_id; &49, subcommand; arguments...
```

Arguments after the one-byte subcommand are little-endian 16-bit words.

1. `0`: create control.
2. `1`: define mesh vertices.
3. `2`: set mesh vertex indexes.
4. `3`: define mesh texture coordinates.
5. `4`: set texture-coordinate indexes.
6. `5`: create an object from an object ID, mesh ID, and bitmap ID.
7. `6`–`17`: object scale, rotation, and translation.
8. `18`–`25`: camera rotation and translation.
9. `26`–`37`: scene scale, rotation, and translation.
10. `38`: render to an existing bitmap.
11. `39`: TurboVega's incomplete delete route.
12. `40`: define object texture coordinates.

Run the following scope guard after bridge changes:

```bash
python3 scripts/check_pingo_scope.py
```

Later local commands `41`, `42`, `129`, `130`, `141`, `145`, `149`, and `153`
must not appear on this baseline branch.

## Build

Use the project-local PlatformIO environment:

```bash
~/Agon/mystuff/agon-vdp/.venv/bin/pio run --target clean
~/Agon/mystuff/agon-vdp/.venv/bin/pio run
```

Current clean build:

```text
RAM:     42,520 / 327,680 bytes (13.0%)
Flash: 1,063,353 / 1,310,720 bytes (81.1%)
firmware.bin SHA-256:
97e4a2c1c86ba6d2d6a524086441131d0c89b619d8cab0445c7f5462947239a1
firmware.elf SHA-256:
33a0b1386b075d4772c0d7cf07a85fae8f987957de9319e710246c1b5daeb5f1
```

The inherited Arduino UART missing-return warning remains. The build otherwise
completes successfully.

## Hardware qualification

The durable strict fixtures live in:

```text
~/Agon/mystuff/pingoasm/apps/turbovega
```

Their `src/` directories are tracked; assembled programs and RGBA8888 textures
are generated into ignored `tgt/` directories.

Qualification order:

1. Triangle: smallest geometry and texture-path check.
2. Cube: unambiguous face orientation, UV orientation, and object-transform
   check.
3. HeavyTank: chiral geometry that exposes compounded transform, winding,
   mirroring, and perspective defects hidden by simpler models.
4. Jukebox: non-Pingo smoke test confirming that ordinary VDP operation remains
   healthy.

TurboVega's exact original firmware runs all three strict fixtures, with the
historical rendering faults expected at this point of departure. In
particular, texture interpolation is not perspective-correct, and HeavyTank
renders upside down and appears inside out during rotation.

## Superseded experiment

An earlier construction of this branch mistakenly combined VDP 2.15 with the
latest `fededevi/pingo` renderer. Hardware testing first produced empty
light-blue targets because newer Pingo inverted the camera transform. Adding a
compatibility inversion made geometry visible, but models appeared too distant
and object rotation behaved like orbiting the scene origin.

Those results demonstrated broader transform-semantic drift and invalidated
that renderer as the clean TurboVega baseline. The experiment has been removed
rather than patched further. Any later upstream Pingo work must be introduced
incrementally on a separate branch after this baseline is hardware-qualified.
