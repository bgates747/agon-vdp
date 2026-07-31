# Wolf3D VDP extension

This directory will hold the Agon VDP support for a clean-sheet, Carmack-style
Wolf3D renderer (`wolf3Dport`). It's a placeholder — no renderer code exists
yet; this establishes the layout before implementation starts.

## Provenance and status

Branch: `wolf3d/vdp-extension`, cut from `upstream/main`. Not merged, not
proposed upstream. See the research devlog in the `Wolf3dOrig` project
(`agonport/doc/devlog_20260730.md`) for the feasibility/opcode/convention
research behind this layout, and `agonport/PORT_NOTES.md` for the render
target and screen-layout design decisions (RGBA2222, mode 136 320x240
double-buffered, view window / status bar / play border terminology).

## Layout

Following the same "own subdirectory, own docs" pattern as `../pingo/`
(see that directory's `README.md` for the worked example this mirrors):

```text
video/wolf3d/          this directory: renderer/support code, own docs
video/wolf3d/README.md this file
```

Subdirectories (e.g. a `render/` for the column renderer, an `assets/` for
texture/map decode) will be added as implementation starts, not speculatively
now.

## Dispatch plan

Same single-top-level-opcode pattern as Pingo's `BUFFERED_PINGO_3D` (`0x49`):
one `#define BUFFERED_WOLF3D ... 0x4A` in `video/agon.h` (candidate opcode,
not yet allocated in code — see devlog item 23), one `#include
"wolf3d/..."` and one `case BUFFERED_WOLF3D: { ... } break;` added to the
existing switch in `video/vdu_buffered.h`, mirroring:

```cpp
case BUFFERED_PINGO_3D: {
        bufferUsePingo3D(bufferId);
}       break;
```

No changes to `agon.h`/`vdu_buffered.h` have been made yet — this file
records the plan so implementation follows the established convention
instead of improvising a new dispatch shape.

Subcommand numbering: subcommand `41` under `BUFFERED_WOLF3D` is reserved
for the render-done callback (enable/disable + token), mirroring Pingo's
own subcommand `41` under `BUFFERED_PINGO_3D` for the identical purpose.
Same number, same job, different top-level opcode — deliberate parity,
not a coincidence.
