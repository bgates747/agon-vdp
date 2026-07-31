#ifndef WOLF3D_DRAW_H
#define WOLF3D_DRAW_H

#include <math.h>
#include <stdint.h>
#include <algorithm>

#include "../wolf3d_world.h"
#include "../../buffers.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Reimplementation (not a port -- see agonport/doc/HANDOFF.md's licensing
// note) of id Software's column raycaster, mirroring WL_DRAW.C/WL_MAIN.C's
// naming so the two stay easy to cross-reference. Operates entirely against
// a Wolf3dWorldState snapshot (video/wolf3d/wolf3d_world.h) pushed from the
// eZ80 -- this is the VDP-side half of the settled eZ80/VDP split: the eZ80
// owns world state/AI/collision, the VDP does its own per-column render.
//
// Status: view-space math (CalcHeight/TransformActor/CalcRotate/
// CalcProjection), WallRefresh's per-column DDA (against the tilemap
// buffer, see GetTile()), and DrawScaleds' projection/depth-sort are all
// implemented for real. Neither fills the screen by itself -- both only
// produce per-column/per-sprite scratch output (WallHeights()/WallTiles()/
// WallTexU()/WallSides(), VisSprites()/VisSpriteCount()); the pixel
// sampling helpers below (SampleWallColumn/SampleSprite) do the nearest-
// neighbor scaling math, and the actual VDP blit orchestration (scratch
// buffer -> bitmap -> Canvas::drawBitmap, the approved method -- see
// video/wolf3d/render/README.md) lives in Wolf3dControl::RenderWalls()/
// RenderSprites() (video/wolf3d.h), since that's where the
// VDUStreamProcessor friend access to bufferCreate()/createBitmapFromBuffer()
// already lives.

#define WOLF3D_TILESHIFT   16
#define WOLF3D_TILEGLOBAL  (1L << WOLF3D_TILESHIFT)       // WL_DEF.H GLOBAL1/TILEGLOBAL
#define WOLF3D_MINDIST     0x5800L                        // WL_DEF.H MINDIST
#define WOLF3D_ACTORSIZE   0x4000L                        // WL_DRAW.C ACTORSIZE
#define WOLF3D_ANGLES      360                             // WL_DEF.H ANGLES
#define WOLF3D_FINEANGLES  3600                            // WL_DEF.H FINEANGLES
#define WOLF3D_MAXVIEWWIDTH 320                            // WL_DEF.H MAXVIEWWIDTH (upper bound for column arrays)
#define WOLF3D_TEX_SIZE     64                              // WL_DEF.H TEXTURESIZE: wall/sprite source bitmaps are 64x64
#define WOLF3D_DEFAULT_VIEWWIDTH  256                       // Original gameplay viewport; configurable projection hook
#define WOLF3D_DEFAULT_VIEWHEIGHT 160                       // Original gameplay viewport; configurable projection hook
#define WOLF3D_DEFAULT_FOCALLENGTH 0x5700L                  // Original WL_MAIN.C focal length

class Wolf3dRenderer {
public:
	// FOCALLENGTH/VIEWWIDTH mirror WL_MAIN.C's own defaults -- CalcProjection
	// can be re-invoked later once the real Agon screen mode/view window is
	// decided (see video/wolf3d/render/README.md).
	Wolf3dRenderer(Wolf3dWorldState& world) : m_world(world) {
		BuildTables();
		CalcProjection(WOLF3D_DEFAULT_VIEWWIDTH, WOLF3D_DEFAULT_VIEWHEIGHT,
			WOLF3D_DEFAULT_FOCALLENGTH);
	}

	// Mirrors WL_MAIN.C's CalcProjection(): derives scale/heightnumerator/
	// centerx from the view window size, once per level/mode change. Also
	// precomputes each column's fine-angle offset from the view angle
	// (mirrors the original's pixelangle[] table, built here via atan2
	// instead of the original's fixed-point tangent-table search).
	void CalcProjection(int viewwidth, int viewheight, long focal) {
		if (viewwidth > WOLF3D_MAXVIEWWIDTH) viewwidth = WOLF3D_MAXVIEWWIDTH;
		m_focallength = focal;
		long facedist = focal + WOLF3D_MINDIST;
		int halfview = viewwidth / 2;
		long viewglobal = WOLF3D_TILEGLOBAL * 2;      // WL_DEF.H VIEWGLOBAL
		m_scale = (int)(halfview * facedist / (viewglobal / 2));
		// Wolf3D stores projected heights in quarter-pixel units; the
		// original scaler converts them with height >> 2. Fold that
		// conversion into the cached numerator so walls and sprites are
		// pixel-sized without adding work to the per-column render loop.
		m_heightNumerator = ((long)WOLF3D_TILEGLOBAL * m_scale) >> 8;
		m_centerx = viewwidth / 2 - 1;
		m_viewwidth = viewwidth;
		m_viewheight = viewheight;
		m_centery = viewheight / 2;

		for (int col = 0; col < viewwidth; col++) {
			double radians = atan2((double)(m_centerx - col), (double)m_scale);
			int fine = (int)lround(radians * WOLF3D_FINEANGLES / (2.0 * M_PI));
			m_pixelAngleFine[col] = fine;
		}
	}

	// Mirrors WL_DRAW.C's WallRefresh(): sets up the per-frame view
	// transform globals (viewx/viewy/viewsin/viewcos) from the world
	// state's player pose.
	void SetupView() {
		int viewangle = m_world.playerAngle;
		int fineangle = NormalizeFineAngle(viewangle * (WOLF3D_FINEANGLES / WOLF3D_ANGLES));
		m_viewsin = m_sintable[fineangle];
		m_viewcos = m_costable[fineangle];
		m_viewx = m_world.playerX - FixedByFrac(m_focallength, m_viewcos);
		m_viewy = m_world.playerY + FixedByFrac(m_focallength, m_viewsin);
	}

	// Mirrors WL_DRAW.C's CalcHeight(): perspective distance -> column
	// height, for a ray's tile-grid intercept point.
	int CalcHeight(wolf3d_fixed_t intercept_x, wolf3d_fixed_t intercept_y) const {
		wolf3d_fixed_t gx = intercept_x - m_viewx;
		wolf3d_fixed_t gxt = FixedByFrac(gx, m_viewcos);
		wolf3d_fixed_t gy = intercept_y - m_viewy;
		wolf3d_fixed_t gyt = FixedByFrac(gy, m_viewsin);
		wolf3d_fixed_t nx = gxt - gyt;
		if (nx < WOLF3D_MINDIST) nx = WOLF3D_MINDIST;
		return (int)(m_heightNumerator / (nx >> 8));
	}

	// Mirrors WL_DRAW.C's TransformActor(): projects an actor into view
	// space, filling in its transx/transy/viewx/viewheight scratch fields.
	void TransformActor(Wolf3dActor& ob) const {
		Transformed t = TransformPoint(ob.x, ob.y);
		ob.transx = t.transx;
		ob.transy = t.transy;
		ob.viewx = t.viewx;
		ob.viewheight = t.viewheight;
	}

	// Mirrors WL_DRAW.C's CalcRotate(): select a view-relative frame from the
	// current state's contiguous two- or eight-rotation art. The eZ80 has
	// already resolved AI dir/projectile angle into one effective world-facing
	// angle, avoiding gameplay class/state tables on the write-only VDP.
	int CalcRotate(const Wolf3dActor& ob) const {
		if (ob.rotations != 2 && ob.rotations != 8) return 0;
		int viewangle = m_world.playerAngle + (m_centerx - ob.viewx) / 8;
		int angle = (viewangle - 180) - ob.facingAngle;

		angle += WOLF3D_ANGLES / 16;
		while (angle >= WOLF3D_ANGLES) angle -= WOLF3D_ANGLES;
		while (angle < 0) angle += WOLF3D_ANGLES;

		if (ob.rotations == 2) return 4 * (angle / (WOLF3D_ANGLES / 2));
		return angle / (WOLF3D_ANGLES / 8);
	}

	// Mirrors WL_DRAW.C's WallRefresh()/AsmRefresh(): the per-column grid-DDA
	// raycast against m_world's tilemap buffer. Fills in m_wallHeight/
	// m_wallTile/m_wallTexU/m_wallSide per column (fed by CalcHeight());
	// the blit itself is done by the caller (Wolf3dControl::RenderWalls(),
	// video/wolf3d.h) using SampleWallColumn() above.
	void WallRefresh() {
		int fineViewAngle = m_world.playerAngle * (WOLF3D_FINEANGLES / WOLF3D_ANGLES);

		for (int col = 0; col < m_viewwidth; col++) {
			int fineangle = NormalizeFineAngle(fineViewAngle + m_pixelAngleFine[col]);
			wolf3d_fixed_t rayDirX = m_costable[fineangle];
			wolf3d_fixed_t rayDirY = -m_sintable[fineangle];   // dy=-sin, matches world's south-is-+y convention

			wolf3d_fixed_t px = m_viewx, py = m_viewy;   // rays are cast from the eye point, same origin CalcHeight subtracts
			int mapX = (int)(px >> 16), mapY = (int)(py >> 16);

			int stepX, stepY;
			wolf3d_fixed_t deltaDistX, deltaDistY, sideDistX, sideDistY;

			if (rayDirX == 0) {
				stepX = 0; deltaDistX = INT32_MAX; sideDistX = INT32_MAX;
			} else if (rayDirX < 0) {
				stepX = -1; deltaDistX = FixedDiv(WOLF3D_TILEGLOBAL, -rayDirX);
				sideDistX = FixedByFrac(deltaDistX, px - (mapX << 16));
			} else {
				stepX = 1; deltaDistX = FixedDiv(WOLF3D_TILEGLOBAL, rayDirX);
				sideDistX = FixedByFrac(deltaDistX, ((mapX + 1) << 16) - px);
			}

			if (rayDirY == 0) {
				stepY = 0; deltaDistY = INT32_MAX; sideDistY = INT32_MAX;
			} else if (rayDirY < 0) {
				stepY = -1; deltaDistY = FixedDiv(WOLF3D_TILEGLOBAL, -rayDirY);
				sideDistY = FixedByFrac(deltaDistY, py - (mapY << 16));
			} else {
				stepY = 1; deltaDistY = FixedDiv(WOLF3D_TILEGLOBAL, rayDirY);
				sideDistY = FixedByFrac(deltaDistY, ((mapY + 1) << 16) - py);
			}

			bool hit = false;
			int side = 0;
			uint8_t tileVal = WOLF3D_TILE_OPEN;
			wolf3d_fixed_t interceptX = px, interceptY = py;

			// Bounded by 2x map size: the mandatory solid outer ring
			// (wolf3d_world.h) guarantees a hit well before this.
			for (int step = 0; step < WOLF3D_MAPSIZE * 2; step++) {
				wolf3d_fixed_t traveled;
				if (sideDistX < sideDistY) {
					traveled = sideDistX;
					sideDistX += deltaDistX;
					mapX += stepX;
					side = 0;
				} else {
					traveled = sideDistY;
					sideDistY += deltaDistY;
					mapY += stepY;
					side = 1;
				}

				if (mapX < 0 || mapX >= WOLF3D_MAPSIZE || mapY < 0 || mapY >= WOLF3D_MAPSIZE) {
					// Shouldn't happen given the solid-border invariant --
					// stop rather than read out of bounds.
					tileVal = WOLF3D_TILE_WALL_MIN;
					hit = true;
					break;
				}

				interceptX = px + FixedByFrac(rayDirX, traveled);
				interceptY = py + FixedByFrac(rayDirY, traveled);

				uint8_t tile = GetTile(mapX, mapY);
				if (tile == WOLF3D_TILE_OPEN) continue;

				if (tile >= WOLF3D_TILE_DOOR_FLAG && tile <= WOLF3D_TILE_DOOR_MAX) {
					uint8_t doorIdx = tile & WOLF3D_TILE_DOOR_MASK;
					const Wolf3dDoor& door = m_world.doors[doorIdx];

					// Exact center-plane intersection, not the DDA's entry
					// gridline -- mirrors the eZ80's own door-only exact
					// calculation (agonport/doc/handoff_ez80_ai_raycaster.md):
					// a vertical door is split by the plane x=tilex+0.5 and
					// gated by the Y fraction there; a horizontal door is
					// split by y=tiley+0.5 and gated by the X fraction.
					bool resolved = false;
					uint16_t fraction = 0;
					if (door.vertical) {
						if (rayDirX != 0) {
							wolf3d_fixed_t doorPlaneX = ((wolf3d_fixed_t)door.tilex << WOLF3D_TILESHIFT) + WOLF3D_TILEGLOBAL / 2;
							wolf3d_fixed_t t = FixedDiv(doorPlaneX - px, rayDirX);
							wolf3d_fixed_t planeY = py + FixedByFrac(rayDirY, t);
							if ((uint32_t)(planeY >> WOLF3D_TILESHIFT) == door.tiley) {
								fraction = (uint16_t)(planeY & 0xFFFF);
								interceptX = doorPlaneX;
								interceptY = planeY;
								resolved = true;
							}
						}
					} else if (rayDirY != 0) {
						wolf3d_fixed_t doorPlaneY = ((wolf3d_fixed_t)door.tiley << WOLF3D_TILESHIFT) + WOLF3D_TILEGLOBAL / 2;
						wolf3d_fixed_t t = FixedDiv(doorPlaneY - py, rayDirY);
						wolf3d_fixed_t planeX = px + FixedByFrac(rayDirX, t);
						if ((uint32_t)(planeX >> WOLF3D_TILESHIFT) == door.tilex) {
							fraction = (uint16_t)(planeX & 0xFFFF);
							interceptX = planeX;
							interceptY = doorPlaneY;
							resolved = true;
						}
					}

					// A door occupies only its half-cell center plane, not
					// the gridline where the DDA entered its tile. If that
					// plane falls outside this tile, keep tracing: the next
					// crossing may be the perpendicular wall face/jamb. This
					// is the original renderer's continuevert/continuehoriz
					// behavior; treating an unresolved plane as solid creates
					// a false door-textured panel on the entry gridline.
					if (!resolved) continue;
					if (fraction <= door.position) continue; // open enough -- ray passes through

					// A resolved, closed portion of the center plane blocks.
					// `side` picks the texture-U axis below; the door's own
					// orientation decides it rather than whichever axis the
					// DDA happened to step on entry.
					side = door.vertical ? 0 : 1;
					tileVal = tile;
					hit = true;
				} else {
					// Ordinary walls and reserved 0xC0-0xFF tiles both
					// render as solid at the DDA's entry gridline. A plain
					// wall face entered directly from a door tile is that
					// door's perpendicular jamb, so retain the wall geometry
					// but select the dedicated frame texture. Checking the
					// door orientation reproduces SpawnDoor's original choice
					// of which two neighboring wall cells were side-marked.
					tileVal = tile;
					if (tile >= WOLF3D_TILE_WALL_MIN && tile <= WOLF3D_TILE_WALL_MAX) {
						int previousX = mapX - (side == 0 ? stepX : 0);
						int previousY = mapY - (side == 1 ? stepY : 0);
						uint8_t previousTile = GetTile(previousX, previousY);
						if (previousTile >= WOLF3D_TILE_DOOR_FLAG && previousTile <= WOLF3D_TILE_DOOR_MAX) {
							const Wolf3dDoor& previousDoor = m_world.doors[previousTile & WOLF3D_TILE_DOOR_MASK];
							bool perpendicularJamb = (side == 1 && previousDoor.vertical)
							                     || (side == 0 && !previousDoor.vertical);
							if (perpendicularJamb) tileVal = WOLF3D_DOOR_JAMB_TEXTURE_ID;
						}
					}
					hit = true;
				}
				break;
			}

			if (!hit) {
				m_wallHeight[col] = 0;
				continue;
			}

			m_wallHeight[col] = CalcHeight(interceptX, interceptY);
			m_wallTile[col] = tileVal;
			m_wallSide[col] = (uint8_t)side;
			// Texture column: fractional tile-crossing position on the
			// non-stepped axis, scaled from a 16-bit fraction down to a
			// 0-63 texture column (64px-wide wall textures). A moving door's
			// art translates with the panel: the original HitVertDoor/
			// HitHorizDoor path subtracts doorposition before selecting U.
			// Jambs are ordinary wall hits and deliberately remain unshifted.
			uint16_t textureFraction = (side == 0)
				? (uint16_t)interceptY
				: (uint16_t)interceptX;
			if (tileVal >= WOLF3D_TILE_DOOR_FLAG && tileVal <= WOLF3D_TILE_DOOR_MAX) {
				const Wolf3dDoor& door = m_world.doors[tileVal & WOLF3D_TILE_DOOR_MASK];
				textureFraction = (uint16_t)(textureFraction - door.position);
			}
			m_wallTexU[col] = (uint8_t)(textureFraction >> 10);
		}
	}

	// Mirrors WL_DRAW.C's DrawScaleds(): projects and distance-sorts every
	// visible actor/static into m_visSprites (far to near, painter's-
	// algorithm order). The blit itself is done by the caller
	// (Wolf3dControl::RenderSprites(), video/wolf3d.h) using SampleSprite().
	// Actor base shapes are resolved to their camera-relative 2/8-way frame
	// here; statics remain fixed shapes.
	void DrawScaleds() {
		m_visCount = 0;

		for (auto& actor : m_world.actors) {
			if (actor.shapenum < 0) continue;
			TransformActor(actor);
			int16_t shapenum = (int16_t)(actor.shapenum + CalcRotate(actor));
			AddVisSprite(actor.viewx, actor.viewheight, shapenum, actor.transx);
		}

		for (auto& stat : m_world.statics) {
			if (stat.shapenum < 0) continue;
			// Statics are tile-centered, matching the original's placement.
			wolf3d_fixed_t x = ((wolf3d_fixed_t)stat.tilex << WOLF3D_TILESHIFT) + WOLF3D_TILEGLOBAL / 2;
			wolf3d_fixed_t y = ((wolf3d_fixed_t)stat.tiley << WOLF3D_TILESHIFT) + WOLF3D_TILEGLOBAL / 2;
			Transformed t = TransformPoint(x, y);
			AddVisSprite(t.viewx, t.viewheight, stat.shapenum, t.transx);
		}

		// Painter's algorithm: draw far to near, so nearer sprites overdraw
		// farther ones and wall occlusion (drawn first, by the caller) stays
		// correct.
		std::sort(m_visSprites, m_visSprites + m_visCount,
		          [](const Wolf3dVisSprite& a, const Wolf3dVisSprite& b) { return a.depth > b.depth; });
	}

	// Mirrors WL_DRAW.C's ThreeDRefresh(): one full frame, same order as
	// the original (walls, then scaled actors/statics, then present).
	void ThreeDRefresh() {
		SetupView();
		WallRefresh();
		DrawScaleds();
	}

	// Per-column DDA output, filled in by WallRefresh() -- read by whatever
	// blit stage lands next.
	const int*     WallHeights() const { return m_wallHeight; }
	const uint8_t* WallTiles()   const { return m_wallTile; }
	const uint8_t* WallTexU()    const { return m_wallTexU; }
	const uint8_t* WallSides()   const { return m_wallSide; }
	int            ViewWidth()  const { return m_viewwidth; }

	// One projected, depth-sorted billboard -- filled in by DrawScaleds().
	struct Wolf3dVisSprite {
		int            viewx;
		unsigned       viewheight;
		int16_t        shapenum;
		wolf3d_fixed_t depth;   // sort key only (transx); not a real distance
	};

	const Wolf3dVisSprite* VisSprites()     const { return m_visSprites; }
	int                    VisSpriteCount() const { return m_visCount; }

	int ViewHeight() const { return m_viewheight; }
	int CenterY()    const { return m_centery; }

	// Mirrors WL_SCALE.C's per-column scaler job: nearest-neighbor vertical
	// resample of one texture column into `dest` (destHeight pixels, 1 byte
	// each -- RGBA2222). Pure pixel math against caller-owned buffers, no VDP
	// buffer/bitmap/canvas calls here -- see Wolf3dControl::RenderWalls() in
	// wolf3d.h for the actual blit (scratch buffer -> bitmap ->
	// Canvas::drawBitmap), which is the approved method (per-pixel
	// Canvas::setPixel was ruled out as non-performant during Pingo's own
	// development; the eZ80 never issues per-column draw commands over the
	// wire either -- rendering is entirely VDP-internal).
	//
	// `fullHeight` is the column's *unclipped* projected height (so the
	// texel step size matches the true projection even when the visible
	// strip is clipped to the screen); `skipRows` is how many rows of that
	// unclipped strip were clipped off the top before `dest` starts.
	static void SampleWallColumn(const uint8_t* srcBase, int srcStride, int fullHeight, int skipRows,
	                             uint8_t* dest, int destHeight) {
		if (fullHeight <= 0 || destHeight <= 0) return;
		long step = ((long)WOLF3D_TEX_SIZE << 16) / fullHeight;
		long frac = step * (long)skipRows;
		for (int y = 0; y < destHeight; y++) {
			int sy = (int)(frac >> 16);
			if (sy >= WOLF3D_TEX_SIZE) sy = WOLF3D_TEX_SIZE - 1;
			dest[y] = srcBase[sy * srcStride];
			frac += step;
		}
	}

	// Mirrors WL_SCALE.C's ScaleShape/SimpleScaleShape job: nearest-neighbor
	// 2D resample of a whole sprite bitmap into `dest` (destWidth*destHeight
	// pixels, row-major, RGBA2222). Same pure-pixel-math/no-VDP-calls split as
	// SampleWallColumn() above -- see Wolf3dControl::RenderSprites().
	//
	// `destFullWidth`/`destFullHeight` are the sprite's *unclipped* projected
	// size (so the texel step size matches the true projection even when the
	// visible rect is clipped to the screen); `skipX`/`skipY` are how many
	// columns/rows of that unclipped rect were clipped off the left/top
	// before `dest` starts (mirrors SampleWallColumn's `skipRows`).
	static void SampleSprite(const uint8_t* src, int srcWidth, int srcHeight,
	                         int destFullWidth, int destFullHeight, int skipX, int skipY,
	                         uint8_t* dest, int destWidth, int destHeight) {
		if (srcWidth <= 0 || srcHeight <= 0 || destFullWidth <= 0 || destFullHeight <= 0
		    || destWidth <= 0 || destHeight <= 0) return;
		long xStep = ((long)srcWidth << 16) / destFullWidth;
		long yStep = ((long)srcHeight << 16) / destFullHeight;
		long yFrac = (long)skipY * yStep;
		for (int y = 0; y < destHeight; y++) {
			int sy = (int)(yFrac >> 16);
			if (sy >= srcHeight) sy = srcHeight - 1;
			const uint8_t* srcRow = src + (size_t)sy * srcWidth;
			uint8_t* destRow = dest + (size_t)y * destWidth;
			long xFrac = (long)skipX * xStep;
			for (int x = 0; x < destWidth; x++) {
				int sx = (int)(xFrac >> 16);
				if (sx >= srcWidth) sx = srcWidth - 1;
				destRow[x] = srcRow[sx];
				xFrac += xStep;
			}
			yFrac += yStep;
		}
	}

	// Apply the original ScaleShape wall-occlusion rule to an already-scaled
	// sprite bitmap. `viewLeft` is the first *visible/clipped* destination
	// column's X coordinate in the renderer viewport, so dest column zero maps
	// directly to wallHeights[viewLeft] even when the sprite's unclipped left
	// edge was off-screen. A sprite column is visible only when the wall's
	// projected height is strictly less than the sprite's projected height;
	// equality belongs to the wall. Hidden columns become transparent RGBA2222
	// (byte zero), retaining one whole-bitmap Canvas draw per sprite.
	static void MaskSpriteColumnsBehindWalls(uint8_t* dest, int destWidth,
	                                        int destHeight, int viewLeft,
	                                        const int* wallHeights, int viewWidth,
	                                        int spriteHeight) {
		if (!dest || !wallHeights || destWidth <= 0 || destHeight <= 0
		    || viewWidth <= 0 || spriteHeight <= 0) return;

		for (int destX = 0; destX < destWidth; destX++) {
			int viewX = viewLeft + destX;
			bool visible = viewX >= 0 && viewX < viewWidth
			            && wallHeights[viewX] < spriteHeight;
			if (visible) continue;

			for (int y = 0; y < destHeight; y++) {
				dest[(size_t)y * destWidth + destX] = 0;
			}
		}
	}

private:
	// Mirrors WL_DRAW.C's FixedByFrac(): 16.16 fixed-point multiply.
	static wolf3d_fixed_t FixedByFrac(wolf3d_fixed_t a, wolf3d_fixed_t b) {
		return (wolf3d_fixed_t)(((int64_t)a * (int64_t)b) >> 16);
	}

	// 16.16 fixed-point divide: a/b expressed as a fixed-point ratio itself
	// scaled by 1<<16 (used for the DDA's deltaDist -- see WallRefresh()).
	static wolf3d_fixed_t FixedDiv(wolf3d_fixed_t a, wolf3d_fixed_t b) {
		return (wolf3d_fixed_t)(((int64_t)a << 16) / b);
	}

	static int NormalizeFineAngle(int fineangle) {
		fineangle %= WOLF3D_FINEANGLES;
		if (fineangle < 0) fineangle += WOLF3D_FINEANGLES;
		return fineangle;
	}

	// Builds the internal fixed-point sin/cos tables (indexed by fine
	// angle, 0..FINEANGLES-1). Reimplementation of WL_MAIN.C's
	// BuildTables(), using floating point at setup time instead of the
	// original's incremental fixed-point generation -- runs once.
	void BuildTables() {
		for (int i = 0; i < WOLF3D_FINEANGLES; i++) {
			double radians = i * (2.0 * M_PI / WOLF3D_FINEANGLES);
			m_sintable[i] = (wolf3d_fixed_t)lround(sin(radians) * 65536.0);
			m_costable[i] = (wolf3d_fixed_t)lround(cos(radians) * 65536.0);
		}
	}

	// Reads one tile byte from the buffer referenced by
	// m_world.tilemapBufferId (see video/buffers.h). Missing/undersized
	// buffers fail safe as solid wall rather than reading out of bounds.
	uint8_t GetTile(int tilex, int tiley) const {
		auto it = buffers.find(m_world.tilemapBufferId);
		if (it == buffers.end() || it->second.empty()) return WOLF3D_TILE_WALL_MIN;
		auto& stream = it->second[0];
		uint32_t idx = (uint32_t)tiley * WOLF3D_MAPSIZE + (uint32_t)tilex;
		if (idx >= stream->size()) return WOLF3D_TILE_WALL_MIN;
		return stream->getBuffer()[idx];
	}

	// Shared projection math behind TransformActor() -- also used by
	// DrawScaleds() for statics, which have no Wolf3dActor to write into.
	struct Transformed { wolf3d_fixed_t transx, transy; int viewx; unsigned viewheight; };
	Transformed TransformPoint(wolf3d_fixed_t x, wolf3d_fixed_t y) const {
		wolf3d_fixed_t gx = x - m_viewx;
		wolf3d_fixed_t gy = y - m_viewy;

		wolf3d_fixed_t gxt = FixedByFrac(gx, m_viewcos);
		wolf3d_fixed_t gyt = FixedByFrac(gy, m_viewsin);
		wolf3d_fixed_t nx = gxt - gyt - WOLF3D_ACTORSIZE;

		gxt = FixedByFrac(gx, m_viewsin);
		gyt = FixedByFrac(gy, m_viewcos);
		wolf3d_fixed_t ny = gyt + gxt;

		Transformed t;
		t.transx = nx;
		t.transy = ny;

		if (nx < WOLF3D_MINDIST) {
			t.viewx = m_centerx;
			t.viewheight = 0;
			return t;
		}

		t.viewx = m_centerx + (int)(((int64_t)ny * m_scale) / nx);
		t.viewheight = (unsigned)(m_heightNumerator / (nx >> 8));
		return t;
	}

	// Appends to m_visSprites if on-screen and not behind MINDIST; silently
	// drops sprites past WOLF3D_MAX_VIS_SPRITES (can't happen today -- sized
	// for every actor+static slot at once).
	void AddVisSprite(int viewx, unsigned viewheight, int16_t shapenum, wolf3d_fixed_t depth) {
		if (viewheight == 0) return;
		if (viewx < -m_centerx || viewx > m_viewwidth + m_centerx) return;
		if (m_visCount >= WOLF3D_MAX_VIS_SPRITES) return;
		m_visSprites[m_visCount++] = { viewx, viewheight, shapenum, depth };
	}

	Wolf3dWorldState& m_world;

	long m_focallength = 0;
	int  m_scale = 0;
	long m_heightNumerator = 0;
	int  m_centerx = 0;
	int  m_viewwidth = 0;
	int  m_viewheight = 0;
	int  m_centery = 0;

	wolf3d_fixed_t m_viewx = 0, m_viewy = 0;
	wolf3d_fixed_t m_viewsin = 0, m_viewcos = 0;

	wolf3d_fixed_t m_sintable[WOLF3D_FINEANGLES];
	wolf3d_fixed_t m_costable[WOLF3D_FINEANGLES];
	int m_pixelAngleFine[WOLF3D_MAXVIEWWIDTH];

	int     m_wallHeight[WOLF3D_MAXVIEWWIDTH];
	uint8_t m_wallTile[WOLF3D_MAXVIEWWIDTH];
	uint8_t m_wallTexU[WOLF3D_MAXVIEWWIDTH];
	uint8_t m_wallSide[WOLF3D_MAXVIEWWIDTH];

	static constexpr int WOLF3D_MAX_VIS_SPRITES = WOLF3D_MAXACTORS + WOLF3D_MAXSTATS;
	Wolf3dVisSprite m_visSprites[WOLF3D_MAX_VIS_SPRITES];
	int m_visCount = 0;
};

#endif // WOLF3D_DRAW_H
