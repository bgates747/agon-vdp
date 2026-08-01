# Combined Pingo and Wolf3DOrig emulator profile

Status: mutable integration profile implemented; automated headless and
interactive Author visual/audio gates passed on 2026-07-31. Physical combined
heap/PSRAM lifecycle qualification remains separate.

The combined VDP has its own profile at:

```text
~/Agon/mystuff/agon-dev-env/emulators/wolf-pingo-v2.16
```

It does not replace or modify either enduring standalone profile:

```text
~/Agon/mystuff/pingoasm/emulator
~/Agon/mystuff/Wolf3dOrig/agonport/emulator
```

The profile lives outside both application trees that it maps, preventing
recursive SD-card exposure. It links the owned Fab executable and matching
MOS, the current mutable `vdp_combined.so` from this worktree, and the
canonical `pingoasm/apps` tree. It snapshots the exact nominated Wolf
executable and level-0 assets into its own profile so a concurrent Wolf rebuild
cannot change or remove the visual gate underneath a running integration task.
`profile-manifest.txt` records the resolved source identities and hashes each
time setup runs. `profile-checksums.sha256` defines the accepted launch set:
every launch fails before Fab starts if the VDP, Fab/MOS and its symbol map,
either selected Pingo fixture directory, the Wolf fixtures, or the Wolf assets
have drifted since setup. After that check, the launcher freezes the manifest,
Fab executable, VDP module, MOS image and symbol map, and every file in the
selected Pingo and Wolf fixture directories into its private run directory. It
rechecks every copy against the frozen manifest and launches only those copies,
closing the check-to-launch race with concurrent builds. Setup itself pins the
nominated Wolf visual artifacts to these accepted hashes:

```text
wolf3d.bin   2d845e2882ae22012f0c64c7705d47b7413dc6b6301bc07bcdfdfadbe635f1c7
tiles.agnb   9990d79e3ae47d0c940e072f70f821d505d9e39ce21f713b38f5d8c7237502c8
sprites.agnb 7bf0f04a6fad56d5a83b26b30f5c0ea3bbafb4c8727a3c5f9d266c30c0e058f9
hud.agnb     1848900559fe9f684dbfc8ae80b0416132db5414bc1ab41f9dad47a5ab26a74d
sfx.agnb     f294c1152bedca56e726e115ce1d1dc2f5aa6191e340a96a79b1e845296f1cd4
```

This is a mutable development profile, not an immutable qualified VDP
snapshot; rerunning setup is an explicit acceptance of the then-current
mutable Pingo and combined-module artifacts, while the nominated Wolf gate
remains pinned. Setup prefers the canonical Wolf target and can recover the
same accepted artifacts from the standalone profile or an existing combined
profile snapshot when concurrent work has removed or rebuilt the canonical
ignored outputs. Every fallback is read-only and must match the pinned hash
before it is copied.

## Provision or repair

Build and smoke-test the native module first:

```bash
make -C ~/Agon/mystuff/agon-vdp-wolf-pingo/userspace \
  FAB_ROOT=~/Agon/mystuff/fab-agon-emulator smoke
```

Then create or repair only the combined profile:

```bash
~/Agon/mystuff/agon-vdp/.venv/bin/python \
  ~/Agon/mystuff/agon-vdp-wolf-pingo/scripts/setup_combined_emulator.py
```

Before making any filesystem change, setup rejects a profile path that is a
symlink, equals or falls beneath either standalone emulator, or overlaps in
either direction with the combined source, Fab, `pingoasm`, or Wolf3DOrig
trees. The two-way overlap check also rejects a broad parent such as
`~/Agon/mystuff`, which would recursively contain mapped source trees. Setup
then refuses to replace unexpected regular files with symlinks and preserves
an existing `autoexec.txt`. To explicitly restore the documented visual gate:

```bash
~/Agon/mystuff/agon-vdp/.venv/bin/python \
  ~/Agon/mystuff/agon-vdp-wolf-pingo/scripts/setup_combined_emulator.py \
  --refresh-autoexec
```

The generated MOS file uses CRLF endings. Its sequence is:

1. Run `earth-party-flat.bin` from the canonical Pingo application tree.
2. When the Author presses Escape, load and run the canonical playable
   audio-enabled `wolf3d.bin` with its level-0 and sound-effect assets.
3. A second Escape returns cleanly to MOS.

Launch it from anywhere:

```bash
~/Agon/mystuff/agon-vdp-wolf-pingo/scripts/run_combined_emulator.sh
```

The launcher resolves every artifact to an absolute path and runs from a
temporary directory without a `firmware` fallback tree. Both interactive and
headless launches use a disposable SD view containing frozen copies of the
selected Pingo/Wolf fixtures; the interactive `autoexec.txt` is copied with a
before/after identity check. A missing or drifting combined module therefore
fails before Fab can boot a stock VDP. Interactive runs let SDL select the
system audio driver so the Wolf sound-effects gate is audible. Set
`SDL_AUDIODRIVER=dummy` explicitly when a silent interactive run is desired.
Headless smoke always uses dummy audio and therefore never plays the boot beep.

## Silent headless smoke

The smoke mode creates a disposable SD view and does not edit the profile's
user-controlled `autoexec.txt`. It runs finite fixtures in one fresh process:

1. the finite 36-frame RGBA2222 Cube benchmark through Pingo opcode `0x49`;
2. the integration-owned Wolf dispatch/exit fixture through opcode `0x4A`.

The latter writes zero to Fab's documented debug output port only after it has
emitted the Wolf command, giving the fresh process a deterministic clean exit.
That tiny assembly fixture is emulator-only and is regenerated during profile
setup.

It requires an exit status of zero, explicit Pingo initialization, exactly 36
`PINGO_RENDER` records, and the Wolf hello diagnostic:

```bash
~/Agon/mystuff/agon-vdp-wolf-pingo/scripts/run_combined_emulator.sh \
  --headless-smoke
```

Successful temporary state is removed. A timeout, emulator failure, or missing
subsystem record preserves the temporary directory and log and prints its
path. This is a profile/runtime smoke test; the native cross-subsystem suite
remains the stronger protocol and lifecycle regression.

## Interactive qualification record

On 2026-07-31 the Author passed the complete sequential profile using the
unchanged combined VDP module. Earth Party Flat rendered correctly; Escape
advanced to the current hardware-accepted Wolf application; Wolf gameplay,
rendering, and sound effects worked; and Escape returned to MOS. The qualified
Wolf application is commit `3695b7d` with these additional pinned artifacts:

```text
wolf3d.bin 2d845e2882ae22012f0c64c7705d47b7413dc6b6301bc07bcdfdfadbe635f1c7
sfx.agnb   f294c1152bedca56e726e115ce1d1dc2f5aa6191e340a96a79b1e845296f1cd4
```

The application uses standard Enhanced Audio and ordinary buffered commands;
it requires no newer Wolf firmware command. Interactive launch leaves
`SDL_AUDIODRIVER` unset unless the caller supplies it, while headless smoke
continues to force dummy audio.

Set `COMBINED_EMULATOR_SMOKE_LOG` when a durable copy of the complete emulator
log is needed for an exact-output oracle or review evidence:

```bash
COMBINED_EMULATOR_SMOKE_LOG=/tmp/wolf-pingo-smoke.log \
  ~/Agon/mystuff/agon-vdp-wolf-pingo/scripts/run_combined_emulator.sh \
  --headless-smoke
```

The destination must not already exist or be a symlink, and its parent must be
an existing writable non-symlink directory. Publication uses a temporary file
and an atomic no-overwrite hard link, so a concurrent file cannot be replaced.
If copying or publishing fails, the launcher retains the complete private run
directory and reports the surviving original log path.

## Files and ownership

Tracked combined-worktree additions:

```text
scripts/setup_combined_emulator.py
scripts/run_combined_emulator.sh
userspace/combined_emulator_exit.asm
docs/wolf-pingo-emulator.md
```

Generated ignored state:

```text
~/Agon/mystuff/agon-dev-env/emulators/wolf-pingo-v2.16/
  snapshots/wolf3d/                 exact nominated visual artifacts
```

No file beneath either standalone Pingo or Wolf emulator profile is written,
relinked, or used as a deployment target. Setup may read exact-hash Wolf
artifacts already accepted in the standalone or combined profile solely as
recovery sources when canonical ignored outputs are absent or have drifted; it
never runs or changes those source profiles. Pingo applications remain live
links to their canonical owning project at rest; each launch runs a verified
private copy of the two selected fixture directories instead of those links.

Per canonical Agon workflow, emulator-facing changes are not committed or
pushed until the Author has launched the interactive profile, reviewed both
stages, and explicitly approved them. That gate passed for the current
audio-enabled profile on 2026-07-31.
