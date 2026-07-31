#ifndef WOLF3D_WORLD_H
#define WOLF3D_WORLD_H

#include <stdint.h>

// VDP-side mirror of the eZ80-authoritative game world.
//
// Naming/shape is deliberately preserved from id Software's original
// WL_DEF.H (educational-use-only source, see agonport/doc/HANDOFF.md's
// licensing note -- this is a clean-room reimplementation, not a copy) so
// anyone cross-referencing the two stays oriented. Field/struct names below
// map directly onto WL_DEF.H's objtype/doorobj_t/statobj_t/dirtype.
//
// Architecture (settled -- see video/wolf3d/README.md): the eZ80 owns world
// state, collision and AI raycasting (sight/hearing/pathfinding, the
// WL_STATE.C-equivalent logic). The VDP does NOT run gameplay logic here --
// this struct is just a render-facing snapshot, refreshed by BUFFERED_WOLF3D
// subcommands each frame, that video/wolf3d/render/wolf3d_draw.h consumes to
// do its own per-column + per-actor rendering (the WL_DRAW.C-equivalent
// logic), independent of the eZ80.

#define WOLF3D_MAPSIZE    64   // WL_DEF.H MAPSIZE: 64x64 tile grid, plane 0
#define WOLF3D_MAXACTORS  150  // WL_DEF.H MAXACTORS
#define WOLF3D_MAXDOORS   64   // WL_DEF.H MAXDOORS
#define WOLF3D_MAXSTATS   400  // WL_DEF.H MAXSTATS

// Tilemap buffer encoding (one byte per tile, row-major, tiley*MAPSIZE+tilex).
// The outer ring (tilex/tiley == 0 or MAPSIZE-1) must always be solid, same
// invariant as the original's level format -- lets both the eZ80 DDA trace
// and the VDP renderer skip explicit map-bounds checks.
#define WOLF3D_TILE_OPEN       0x00        // no wall, walkable
#define WOLF3D_TILE_WALL_MIN   0x01        // 1-127: wall, value == wall texture id
#define WOLF3D_TILE_WALL_MAX   0x7F
#define WOLF3D_TILE_DOOR_FLAG  0x80        // 128-191: door, low 7 bits == door index
#define WOLF3D_TILE_DOOR_MAX   0xBF        // last valid door tile (63 == WOLF3D_MAXDOORS-1)
#define WOLF3D_TILE_DOOR_MASK  0x7F
#define WOLF3D_TILE_RESERVED_MIN 0xC0      // 192-255: reserved, block conservatively (not a door)

// Door art uses fixed texture IDs outside the shareware level's ordinary
// wall range. The jamb is rendered on the perpendicular wall face beside
// every door, independent of that door's lock/face texture.
#define WOLF3D_DOOR_JAMB_TEXTURE_ID 122

// Wall/door and actor/static sprite textures are NOT packed into one shared
// atlas buffer -- real texture usage per level is sparse (most of the
// 1-127 possible wall texture ids are never referenced by a given map), so
// a dense grid/strip atlas would waste space and need per-id sub-rect math
// for no benefit. Instead every texture is its own separate VDP bitmap
// buffer (mirrors the parallel AgonWolf3D project's one-buffer-per-texture
// approach), at a buffer id computed with a fixed offset -- no atlas
// layout and no lookup table needed on the wire, since the VDP buffer
// store (video/buffers.h) is a sparse map already free of per-id waste.
// The eZ80 uploads each texture (e.g. via an .agnb-style container, see
// agon-utils/examples/agnb) at exactly the buffer id these formulas give;
// the VDP renderer recomputes the same id at render time to select it.
#define WOLF3D_WALL_BUFFER_BASE   0x1000 // wall/door texture ids 1-127 -> 0x1001-0x107F
#define WOLF3D_SPRITE_BUFFER_BASE 0x2000 // sprite shapenum 0-8191   -> 0x2000-0x3FFF

// textureId is a tile byte's wall/door texture id (WOLF3D_TILE_WALL_MIN..MAX).
inline uint16_t Wolf3dWallBufferId(uint8_t textureId) {
	return WOLF3D_WALL_BUFFER_BASE + textureId;
}

// shapenum is Wolf3dActor::shapenum / Wolf3dStatic::shapenum (must be >= 0).
inline uint16_t Wolf3dSpriteBufferId(int16_t shapenum) {
	return WOLF3D_SPRITE_BUFFER_BASE + (uint16_t)shapenum;
}

// 16.16 fixed point, mirrors WL_DEF.H's `typedef long fixed;`
typedef int32_t wolf3d_fixed_t;

// Mirrors WL_DEF.H's dirtype exactly (same member order/values -- this is
// the wire contract with the eZ80 side, not just a local convenience enum).
enum Wolf3dDir {
	wolf3d_dir_east,
	wolf3d_dir_northeast,
	wolf3d_dir_north,
	wolf3d_dir_northwest,
	wolf3d_dir_west,
	wolf3d_dir_southwest,
	wolf3d_dir_south,
	wolf3d_dir_southeast,
	wolf3d_dir_nodir
};

// Mirrors WL_DEF.H's doorobj_t. `position` is a fractional 0 (closed) to
// 0xFFFF (fully open) value, replacing the original's `doorposition[]`
// fixed-point fraction with a plain 16-bit scale for wire simplicity.
// Blocking rule: a sight/render ray crossing this tile passes iff its
// tile-relative crossing fraction <= position. `vertical` doors (crossed by
// rays traveling along X) use the Y-fraction, 0x0000 = north edge, 0xFFFF =
// south edge; horizontal doors use the X-fraction, 0x0000 = west edge,
// 0xFFFF = east edge. Neither orientation mirrors the fraction.
struct Wolf3dDoor {
	uint8_t tilex, tiley;
	bool    vertical;
	uint8_t lock;
	enum { action_closed, action_opening, action_open, action_closing } action;
	uint16_t position;
	// Wall/door texture id (WOLF3D_TILE_WALL_MIN..MAX), resolved via
	// Wolf3dWallBufferId(). A door's tile byte only has room for the
	// WOLF3D_TILE_DOOR_FLAG bit + a 7-bit door index (see WOLF3D_TILE_DOOR_MASK
	// below), not a texture id too -- so unlike plain walls (whose tile byte
	// *is* the texture id), a door's texture id has to live here instead.
	uint8_t textureId;
};

// Mirrors WL_DEF.H's statobj_t (pickups/decorations). shapenum == -1 means
// "removed", exactly the original's own convention -- see WL_AGENT.C's
// GetBonus().
struct Wolf3dStatic {
	uint8_t tilex, tiley;
	int16_t shapenum;
	uint8_t flags;
};

// Mirrors the render-relevant subset of WL_DEF.H's objtype (the full
// thinking-actor struct). Fields the eZ80 owns and pushes are listed first;
// the transx/transy/viewx/viewheight fields are scratch space filled in
// per-frame by Wolf3dRenderer::TransformActor(), same role as in the
// original -- not sent over the wire.
struct Wolf3dActor {
	uint16_t id;
	int16_t  shapenum;  // -1 == inactive/removed slot
	wolf3d_fixed_t x, y;
	uint8_t  tilex, tiley;
	Wolf3dDir dir;
	int16_t  angle;     // 0-359, mirrors objtype::angle
	int16_t  hitpoints;
	uint8_t  flags;      // mirrors objtype::flags (FL_SHOOTABLE etc, WL_DEF.H)

	// Render-only scratch, recomputed every frame -- mirrors objtype's own
	// transx/transy/viewx/viewheight fields exactly.
	wolf3d_fixed_t transx, transy;
	int      viewx;
	unsigned viewheight;
};

// The full per-level/per-frame snapshot pushed from the eZ80. One instance
// lives inside Wolf3dControl (video/wolf3d.h).
struct Wolf3dWorldState {
	// Buffer ID referencing the tilemap asset uploaded via the stock
	// buffered-command API (see agonport/doc/vdp-3d-pipeline-reuse.md) --
	// init_level only associates it, it doesn't carry the raw bytes itself.
	// Wall/sprite textures don't get their own field here -- see
	// Wolf3dWallBufferId()/Wolf3dSpriteBufferId() above.
	uint16_t tilemapBufferId;       // 64x64 plane-0 grid (walls/doors/areas)

	wolf3d_fixed_t playerX, playerY;
	int16_t        playerAngle;

	Wolf3dDoor   doors[WOLF3D_MAXDOORS];
	Wolf3dActor  actors[WOLF3D_MAXACTORS];
	Wolf3dStatic statics[WOLF3D_MAXSTATS];

	void init_level(uint16_t tilemapBufferId_) {
		tilemapBufferId = tilemapBufferId_;
		for (auto& door : doors) door.position = 0;
		for (auto& actor : actors) actor.shapenum = -1;
		for (auto& stat : statics) stat.shapenum = -1;
	}

	void set_player_pose(wolf3d_fixed_t x, wolf3d_fixed_t y, int16_t angle) {
		playerX = x;
		playerY = y;
		playerAngle = angle;
	}

	void set_door(uint8_t doornum, uint8_t tilex, uint8_t tiley, bool vertical, uint8_t lock, uint8_t action, uint16_t position, uint8_t textureId) {
		if (doornum >= WOLF3D_MAXDOORS) return;
		Wolf3dDoor& door = doors[doornum];
		door.tilex = tilex;
		door.tiley = tiley;
		door.vertical = vertical;
		door.lock = lock;
		door.action = (decltype(door.action))action;
		door.position = position;
		door.textureId = textureId;
	}

	void set_actor(uint16_t actorId, int16_t shapenum, wolf3d_fixed_t x, wolf3d_fixed_t y, uint8_t tilex, uint8_t tiley, Wolf3dDir dir, int16_t angle, int16_t hitpoints, uint8_t flags) {
		if (actorId >= WOLF3D_MAXACTORS) return;
		Wolf3dActor& actor = actors[actorId];
		actor.id = actorId;
		actor.shapenum = shapenum;
		actor.x = x;
		actor.y = y;
		actor.tilex = tilex;
		actor.tiley = tiley;
		actor.dir = dir;
		actor.angle = angle;
		actor.hitpoints = hitpoints;
		actor.flags = flags;
	}

	void remove_actor(uint16_t actorId) {
		if (actorId >= WOLF3D_MAXACTORS) return;
		actors[actorId].shapenum = -1;
	}

	void set_static(uint16_t index, uint8_t tilex, uint8_t tiley, int16_t shapenum, uint8_t flags) {
		if (index >= WOLF3D_MAXSTATS) return;
		Wolf3dStatic& stat = statics[index];
		stat.tilex = tilex;
		stat.tiley = tiley;
		stat.shapenum = shapenum;
		stat.flags = flags;
	}
};

#endif // WOLF3D_WORLD_H
