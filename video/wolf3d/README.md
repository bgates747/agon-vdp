# Wolf3D VDP extension

This directory contains the custom VDP half of the clean-sheet,
Carmack-style Wolf3D renderer. The eZ80 owns authoritative gameplay state;
this extension consumes snapshots, performs the column DDA and sprite
projection, draws the in-game HUD, and reports render completion to the
callback-gated client.

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

`render/` owns view/projection, wall/door DDA, and sprite projection.
`hud/` owns persistent status values and the stable original-WL1 bitmap-ID
contract. `wolf3d_world.h` is the wire/state model, while the parent
`video/wolf3d.h` orchestrates VDP-local bitmap blits and completion.

## Dispatch plan

The implemented dispatch follows the same single-top-level-opcode pattern as
Pingo's `BUFFERED_PINGO_3D` (`0x49`): `BUFFERED_WOLF3D` is `0x4A` in
`video/agon.h`, with one `case BUFFERED_WOLF3D` in
`video/vdu_buffered.h`, mirroring:

```cpp
case BUFFERED_PINGO_3D: {
        bufferUsePingo3D(bufferId);
}       break;
```

Subcommand numbering: subcommand `41` under `BUFFERED_WOLF3D` is reserved
for the render-done callback (enable/disable + token), mirroring Pingo's
own subcommand `41` under `BUFFERED_PINGO_3D` for the identical purpose.
Same number, same job, different top-level opcode — deliberate parity,
not a coincidence.

## Current play-screen composition

The accepted 256x160 view is centered at `(32,0)` inside the 320x160 play
area. The side surround and one-pixel bevel mirror the original play border.
At `y=160`, buffer `$40F0` supplies a 320x80 lower panel: the original 320x40
shareware status strip followed by a deliberate neutral extension through the
extra forty rows in Agon mode 8. Individual weapons, keys, digits, and face
frames use buffers `$4000 + originalChunkId`; the complete status state is
redrawn into every hidden buffer before render completion is reported.

## Static-object rendering

The eZ80 submits the real level's stable static slots with subcommand 6.
Decorations and pickups are tile-centered billboards; a negative shapenum is
the persistent removed sentinel. The renderer transforms all active statics,
culls them against the view, sorts them with actors far-to-near, and masks
their scaled columns against the wall-height buffer before one bitmap draw per
sprite. Collision and pickup effects deliberately remain eZ80 concerns; the
VDP is a write-only snapshot consumer.

## Actor rendering and wire lifetime

Actors use stable slots `0..149` and the same persistent, dirty-only lifetime
as statics. Subcommand 4 replaces one complete render record:

```text
actorId(word), baseShapenum(word), x(long), y(long),
facingAngle(word), rotations(byte)
```

The payload is exactly 15 bytes. Words and signed 16.16 longs are
little-endian, with each long sent low word first. `baseShapenum=-1` is the
inactive sentinel; subcommand 5 with `actorId(word)` is the shorter,
equivalent removal operation. A subsequent set on the same slot replaces its
complete state, so slot reuse remains entirely eZ80-owned.

`facingAngle` uses the player-angle convention (east=0, north=90, integer
degrees). `rotations` is the exact number of view-relative frames: 0, 2, or 8.
The eZ80 resolves its gameplay state/AI direction or projectile angle to this
base shape and effective facing only when that actor changes. The VDP chooses
the camera-relative frame on every render, so camera movement does not create
spurious actor updates. Cached tile coordinates, hit points, AI flags, and
state-machine data never cross this render-only ABI.

## Ordinary actor/combat hardware milestone

The matching eZ80 client now owns the complete ordinary E1L1 actor path:
patrol, area connectivity, awareness, chase/dodge movement, guard/officer/
mutant/SS shooting chains, dog jump/bite chains, and player damage/death. The
VDP remains a write-only snapshot renderer throughout. It receives only dirty
actor render records and derives the view-relative frame for every rendered
camera pose; gameplay visibility and hit decisions never depend on render
completion.

Emulator and physical-hardware acceptance both pass with the real medium E1L1
population and full wall, static, actor, sprite, and HUD workload. Hardware is
eminently playable, with a small amount of perceived latency retained as a
performance note. The ESP32-PICO-D4 build uses 83.0% flash and 13.6% RAM. All
four uploaded regions passed esptool hash verification and the board reset and
re-enumerated normally. The accepted 1,087,792-byte firmware image has SHA-256
`4093eeb054b9494f169c02d576b2d0fd0a7699e09996fc8984eb0d5d80384463`.

The synchronized `ordinary-combat-hardware-pass` tag in this repository and
the sibling `Wolf3dOrig` repository identifies the matching VDP/client pair.
