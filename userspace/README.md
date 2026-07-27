# TurboVega-baseline native VDP

This directory builds the `pingo-codex` VDP as a native shared object for Fab
Agon Emulator. It is build plumbing only: the Pingo renderer, math library,
Agon bridge, and protocol behavior remain the hardware-qualified TurboVega
baseline.

`FAB_ROOT` must name the owned Fab checkout with initialized
`userspace-vdp-gl` submodules:

```bash
make -C userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator
```

The ignored output is:

```text
video/build/userspace/vdp_pingo.so
```

Run the ABI and empty-scene smoke test with:

```bash
make -C userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator \
  smoke
```

The smoke test loads the module with immediate symbol resolution, starts the
native VDP, creates a 64×64 RGBA8888 bitmap, creates a TurboVega Pingo control,
renders an empty scene, and verifies that Fab exposes a live framebuffer.

The persistent comparison emulator must snapshot the resulting shared object.
It must not symlink directly to this build output, because later
`pingo-codex` builds will replace that file.
