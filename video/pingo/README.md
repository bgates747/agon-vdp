# Embedded Pingo renderer

This directory contains the Pingo math and software-rendering code used by the
Agon VDP integration in `../pingo_3d.h`.

## Provenance

The base is TurboVega's final `pingo3D` port, commit
`f4814813e8155780c5ad2602cd45f82ca5a72eec`, placed on canonical Agon VDP
2.15. The `tv-port` branch preserves that historical control. Corrections are
developed incrementally on `pingo-codex`; this is not a wholesale import of
the latest `fededevi/pingo`.

## Alpha 1 rendering contract

1. Right-handed world: `+X` right, `+Y` up, `+Z` toward the viewer.
2. Unrotated forward is `-Z`.
3. The VDU bridge accepts camera pose commands and supplies one inverse view
   matrix to the renderer.
4. NDC `+Y` is up; viewport conversion reflects Y once for top-origin target
   memory.
5. Blender/OBJ UV V is bottom-origin; the texture sampler converts it once to
   top-row-first RGBA memory.
6. Perspective-correct UVs interpolate `u/w`, `v/w`, and `1/w`.

These rules have been visually qualified with labeled cube and chiral
HeavyTank fixtures on real hardware and the isolated Fab Agon emulator.

## Layout

```text
math/       vectors, matrices, transforms, and projection
render/     scene, mesh, object, rasterizer, depth, texture, and renderer
```

The Agon bridge borrows VDP bitmap memory for render targets and textures.
Ownership, validation, and VDU command compatibility therefore remain bridge
concerns rather than generic renderer APIs.

## Verification

Run the strict VDU-surface guard:

```bash
python3 scripts/check_pingo_scope.py
```

Build embedded firmware:

```bash
.venv/bin/pio run
```

Build and smoke-test the native module:

```bash
make -C userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator smoke
```

Hardware is the behavioral ground truth. Any refreshed emulator snapshot must
receive explicit human validation before related changes are committed.

See `docs/tv-port.md` for exact source pins, deployment, qualification, and
remaining work.
