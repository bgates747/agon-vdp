# Wolf3D column renderer

Placeholder for the per-column rasterizer — the one piece of the original
game's rendering pipeline with a real analog on the VDP side.

Original id Software equivalent: `WL_DRAW.C`'s raycast column loop, and the
`WL_SCALE.C`/`CONTIGSC.C`/`OLDSCALE.C` family of compiled column scalers
(three CPU-tier-specific implementations of the same job in the original —
Agon only needs one modern implementation). Given a screen column, a
texture ID + source column, and a top/bottom height, this blits a scaled
textured vertical strip into the render target.

No code yet. What lives here depends on the still-open eZ80/VDP renderer-
boundary question (see `agonport/doc/vdp-3d-pipeline-reuse.md`'s open
questions and `agonport/ARCHITECTURE_PRECIS.md`): if the eZ80 keeps
computing per-column draw parameters and only sends "draw this column"
commands, this stays a pure blitter with no ray/wall-distance math of its
own.
