#ifndef WOLF3D_H
#define WOLF3D_H

#include <stdint.h>
#include <algorithm>

#include "agon.h"
#include "vdu_stream_processor.h"
#include "sprites.h"
#include "wolf3d/wolf3d_world.h"
#include "wolf3d/render/wolf3d_draw.h"
#include "wolf3d/hud/wolf3d_status.h"
#include "wolf3d/presentation/wolf3d_fizzle.h"

// Wolf3D VDP extension glue. Mirrors video/pingo_3d.h's structure: this file
// is the dispatch/bridge code living directly under video/, with the
// renderer implementation itself (once it exists) under video/wolf3d/ --
// see video/wolf3d/README.md for the layout rationale.
//
// Completion transport is a deliberate parity mirror of Pingo's
// subcommand 41 (video/pingo_3d.h's set_render_notification/
// send_render_complete): same subcommand number, same enable/disable +
// token shape, same stock-keyboard-packet delivery. Only the wire magic
// differs ("W3DR" vs Pingo's "P3DR") so a client watching for both
// extensions' completion packets on the same UART can tell them apart.
#define WOLF3D_RENDER_NOTIFY_DISABLED 0
#define WOLF3D_RENDER_NOTIFY_KEYCODE  1
#define WOLF3D_RENDER_NOTIFY_VERSION  1
#define WOLF3D_NOTIFY_RENDER_COMPLETE 1
#define WOLF3D_NOTIFY_FIZZLE_COMPLETE 2

// Internal VDP-side scratch buffer ids used by RenderWalls()/RenderSprites()/
// RenderViewWeapon() below to stage one resampled image before it's converted to a
// bitmap and drawn. These never cross the wire and are never referenced by
// the eZ80 -- picked well clear of the wall/sprite asset id ranges
// (Wolf3dWallBufferId()/Wolf3dSpriteBufferId() in wolf3d_world.h use
// 0x1000-0x107F/0x2000-0x3FFF) and of any eZ80-supplied tilemapBufferId.
#define WOLF3D_SCRATCH_WALL_BUFFER_ID   0xFFFE
#define WOLF3D_SCRATCH_SPRITE_BUFFER_ID 0xFFFD
#define WOLF3D_SCRATCH_WEAPON_BUFFER_ID 0xFFFC

typedef struct tag_Wolf3dControl {
	uint8_t             m_render_notify_mode = WOLF3D_RENDER_NOTIFY_DISABLED; // Opt-in completion transport
	uint16_t            m_render_notify_token = 0;
	uint32_t            m_render_sequence = 0;
	uint32_t            m_presentation_sequence = 0;
	int16_t             m_view_weapon_shapenum = -1;
	Wolf3dWorldState    m_world;                // eZ80-authoritative snapshot mirror
	Wolf3dRenderer       m_renderer { m_world }; // per-column + billboard renderer
	Wolf3dStatusBar      m_statusBar;            // HUD/status-bar drawing

	// VDU 23, 0, &A0, bufferId; &4A, 0: dispatch smoke test, no scene/render
	// state exists yet -- this just proves the opcode/subcommand plumbing.
	void hello_world(VDUStreamProcessor& processor) {
		debug_log("Wolf3D: Hello from Castle Wolfenstein 3D!\n\r");
	}

	// Composes a 32-bit value from two wire words (low word first), since
	// VDUStreamProcessor has no native 32-bit reader.
	static int32_t read_long(VDUStreamProcessor& processor) {
		int32_t lo = processor.readWord_t();
		int32_t hi = processor.readWord_t();
		return (int32_t)(((uint32_t)hi << 16) | (uint16_t)lo);
	}

	// VDU ... &4A, 1, tilemapBufferId; -- wall/sprite textures are per-texture
	// buffers, not associated here, see Wolf3dWallBufferId()/
	// Wolf3dSpriteBufferId() in wolf3d_world.h.
	void init_level(VDUStreamProcessor& processor) {
		auto tilemapBufferId = processor.readWord_t();
		m_world.init_level(tilemapBufferId);
	}

	// VDU ... &4A, 2, x; x; y; y; angle;  (x/y are 32-bit fixed, low word first)
	void set_player_pose(VDUStreamProcessor& processor) {
		auto x = read_long(processor);
		auto y = read_long(processor);
		auto angle = processor.readWord_t();
		m_world.set_player_pose(x, y, (int16_t)angle);
	}

	// VDU ... &4A, 3, doornum, tilex, tiley, vertical, lock, action, position; position; textureId
	// textureId is new (appended, doesn't disturb the existing field order):
	// a door tile byte only has room for WOLF3D_TILE_DOOR_FLAG + a 7-bit door
	// index, not a texture id too, so unlike plain walls (tile byte == texture
	// id) the door's wall texture id has to be carried here instead -- see
	// Wolf3dDoor::textureId in wolf3d_world.h.
	void set_door(VDUStreamProcessor& processor) {
		auto doornum = processor.readByte_t();
		auto tilex = processor.readByte_t();
		auto tiley = processor.readByte_t();
		auto vertical = processor.readByte_t();
		auto lock = processor.readByte_t();
		auto action = processor.readByte_t();
		auto position = processor.readWord_t();
		auto textureId = processor.readByte_t();
		m_world.set_door(doornum, tilex, tiley, vertical != 0, lock, action, position, textureId);
	}

	// VDU ... &4A, 4, actorId; shapenum; x; x; y; y; facingAngle; rotations
	// actorId/shapenum/facingAngle are words; x/y are 32-bit fixed, low word
	// first; rotations is the exact frame count 0, 2, or 8. The eZ80 resolves
	// gameplay state to this render-only record and sends it only when dirty.
	void set_actor(VDUStreamProcessor& processor) {
		auto actorId = processor.readWord_t();
		auto shapenum = processor.readWord_t();
		auto x = read_long(processor);
		auto y = read_long(processor);
		auto facingAngle = processor.readWord_t();
		auto rotations = processor.readByte_t();
		m_world.set_actor((uint16_t)actorId, (int16_t)shapenum, x, y,
			(int16_t)facingAngle, rotations);
	}

	// VDU ... &4A, 5, actorId; actorId;
	void remove_actor(VDUStreamProcessor& processor) {
		auto actorId = processor.readWord_t();
		m_world.remove_actor((uint16_t)actorId);
	}

	// VDU ... &4A, 6, index; index; tilex, tiley, shapenum; shapenum; flags;
	void set_static(VDUStreamProcessor& processor) {
		auto index = processor.readWord_t();
		auto tilex = processor.readByte_t();
		auto tiley = processor.readByte_t();
		auto shapenum = processor.readWord_t();
		auto flags = processor.readByte_t();
		m_world.set_static((uint16_t)index, tilex, tiley, (int16_t)shapenum, flags);
	}

	// VDU ... &4A, 8, shapenum; -- persistent first-person view shape.
	// A wire value of 0xFFFF casts to the -1 sentinel and suppresses the
	// overlay, independently of the HUD weapon icon set by subcommand 13.
	void set_view_weapon(VDUStreamProcessor& processor) {
		auto shapenum = processor.readWord_t();
		if (shapenum < 0) return;
		m_view_weapon_shapenum = (int16_t)(uint16_t)shapenum;
	}

	// VDU ... &4A, 7: renders synchronously into the hidden mode-8 buffer;
	// completion is reported through subcommand 41 only after all queued draws
	// have drained. The eZ80 then presents that buffer and may submit the next
	// newest-state snapshot.
	void render_frame(VDUStreamProcessor& processor) {
		debug_log("Wolf3D render_frame: begin\n\r");
		// Clear the complete mode-8 320x200 play surround, then place the active
		// view window at its centered Wolf3D origin. Bounds continue to follow
		// CalcProjection(), preserving the viewport-scaling hook.
		int viewwidth = m_renderer.ViewWidth();
		int viewheight = m_renderer.ViewHeight();
		int horizon = m_renderer.CenterY();
		int viewX = ViewOriginX();
		int viewY = ViewOriginY();
		// Wolf3D's DrawPlayBorder fills the area around a reduced view with
		// VGA palette index 127.  Its nearest Agon64 colour is dark cyan;
		// retaining that surround makes the centered 256-pixel view read as
		// the original play window instead of a shifted full-screen render.
		canvas->setBrushColor(0, 85, 85);
		canvas->fillRectangle(0, 0, WOLF3D_SCREEN_WIDTH - 1,
			WOLF3D_PLAY_AREA_HEIGHT - 1);
		canvas->setBrushColor(170, 170, 170);
		canvas->fillRectangle(viewX, viewY, viewX + viewwidth - 1,
			viewY + horizon - 1);
		canvas->setBrushColor(85, 85, 85);
		canvas->fillRectangle(viewX, viewY + horizon, viewX + viewwidth - 1,
			viewY + viewheight - 1);
		// Preserve DrawPlayBorder's one-pixel bevel around all four sides of
		// the centered window. With the mode-8 layout the 160-pixel view sits
		// at y=20 inside a full 200-pixel play surround.
		const int borderLeft = std::max(viewX - 1, 0);
		const int borderRight = std::min(viewX + viewwidth,
			WOLF3D_SCREEN_WIDTH - 1);
		const int borderTop = std::max(viewY - 1, 0);
		const int borderBottom = std::min(viewY + viewheight,
			WOLF3D_PLAY_AREA_HEIGHT - 1);
		if (viewY > 0) {
			canvas->setBrushColor(0, 0, 0);
			canvas->fillRectangle(borderLeft, borderTop,
				borderRight, borderTop);
		}
		if (viewY + viewheight < WOLF3D_PLAY_AREA_HEIGHT) {
			// Mode 8 quantizes RGB888 components in 64-value bands. 113 would
			// collapse to the surround's same 0,85,85 output; use component 2
			// so the bottom/right highlight remains visibly distinct.
			canvas->setBrushColor(0, 170, 170);
			canvas->fillRectangle(borderLeft, borderBottom,
				borderRight, borderBottom);
		}
		if (viewX > 0) {
			canvas->setBrushColor(0, 0, 0);
			canvas->fillRectangle(borderLeft, borderTop, borderLeft,
				borderBottom);
		}
		if (viewX + viewwidth < WOLF3D_SCREEN_WIDTH) {
			canvas->setBrushColor(0, 170, 170);
			canvas->fillRectangle(borderRight, borderTop,
				borderRight, borderBottom);
		}
		waitPlotCompletion(false);
		m_renderer.ThreeDRefresh();
		RenderWalls(processor);
		RenderSprites(processor);
		RenderViewWeapon(processor);
		RenderStatusBar();
		// render_frame is synchronous at the wire boundary. Keep that
		// contract explicit even if a future blit path queues work without
		// its own scratch-lifetime drain.
		waitPlotCompletion(false);
		m_render_sequence++;
		send_render_complete(processor, m_render_sequence);
		debug_log("Wolf3D render_frame: complete, sequence %u\n\r",
			m_render_sequence);
	}

	// Actual wall-column blit: for each screen column with a hit
	// (WallHeights()[col] > 0), resamples the hit wall/door texture's column
	// into a scratch buffer (Wolf3dRenderer::SampleWallColumn(), nearest-
	// neighbor vertical scale), wraps that scratch buffer as a 1xN RGBA2222
	// bitmap (createBitmapFromBuffer()), and draws it with
	// Canvas::drawBitmap() -- the approved method (buffer -> bitmap -> bitmap
	// plot), not per-pixel Canvas::setPixel() or any per-column wire command
	// (rendering is entirely VDP-internal -- the eZ80 never issues draw
	// commands over the wire).
	void RenderWalls(VDUStreamProcessor& processor) {
		auto heights = m_renderer.WallHeights();
		auto tiles = m_renderer.WallTiles();
		auto texU = m_renderer.WallTexU();
		int viewheight = m_renderer.ViewHeight();
		int centery = m_renderer.CenterY();
		int viewX = ViewOriginX();
		int viewY = ViewOriginY();

		for (int col = 0; col < m_renderer.ViewWidth(); col++) {
			int fullHeight = heights[col];
			if (fullHeight <= 0) continue;

			// Door tiles can't carry a texture id in their tile byte (only
			// WOLF3D_TILE_DOOR_FLAG + a 7-bit door index fit in 8 bits), so
			// their texture id comes from the door's own struct field instead
			// -- see Wolf3dDoor::textureId in wolf3d_world.h. Only
			// WOLF3D_TILE_DOOR_FLAG..WOLF3D_TILE_DOOR_MAX (0x80-0xBF) are
			// real doors -- 0xC0-0xFF are reserved and would otherwise index
			// past m_world.doors[WOLF3D_MAXDOORS].
			uint8_t tile = tiles[col];
			uint8_t textureId = (tile >= WOLF3D_TILE_DOOR_FLAG && tile <= WOLF3D_TILE_DOOR_MAX)
				? m_world.doors[tile & WOLF3D_TILE_DOOR_MASK].textureId
				: tile;
			auto srcBitmap = getBitmap(Wolf3dWallBufferId(textureId));
			if (!srcBitmap || srcBitmap->format != PixelFormat::RGBA2222) continue;

			int top = centery - fullHeight / 2;
			int clippedTop = std::max(top, 0);
			int clippedBottom = std::min(top + fullHeight, viewheight);
			int destHeight = clippedBottom - clippedTop;
			if (destHeight <= 0) continue;

			processor.bufferClear(WOLF3D_SCRATCH_WALL_BUFFER_ID);
			auto scratch = processor.bufferCreate(WOLF3D_SCRATCH_WALL_BUFFER_ID, destHeight);
			if (!scratch) continue;

			const uint8_t* srcColumn = srcBitmap->data + texU[col];
			Wolf3dRenderer::SampleWallColumn(srcColumn, srcBitmap->width, fullHeight,
			                                 clippedTop - top, scratch->getBuffer(), destHeight);

			processor.createBitmapFromBuffer(WOLF3D_SCRATCH_WALL_BUFFER_ID, 1 /* RGBA2222 */, 1, destHeight);
			auto columnBitmap = getBitmap(WOLF3D_SCRATCH_WALL_BUFFER_ID);
			if (columnBitmap) {
				canvas->drawBitmap(viewX + col, viewY + clippedTop, columnBitmap.get());
				// drawBitmap queues a raw Bitmap pointer. Drain it while
				// this bitmap and its scratch buffer are still alive;
				// the next column clears and recreates both.
				waitPlotCompletion(false);
			}
		}
	}

	// Actual sprite blit: for each visible actor/static (VisSprites(),
	// already depth-sorted far-to-near by DrawScaleds()), resamples the
	// whole sprite bitmap to its projected on-screen size
	// (Wolf3dRenderer::SampleSprite(), nearest-neighbor 2D scale) into a
	// scratch buffer, wraps it as a bitmap, and draws it -- same
	// buffer -> bitmap -> Canvas::drawBitmap approach as RenderWalls() above.
	//
	// The scaled scratch bitmap is then masked column-by-column against the
	// saved wall heights using the original ScaleShape depth rule. Fully
	// occluded columns become transparent before the single bitmap draw, so
	// correct wall clipping does not require one Canvas operation per column.
	void RenderSprites(VDUStreamProcessor& processor) {
		int viewwidth = m_renderer.ViewWidth();
		int viewheight = m_renderer.ViewHeight();
		int viewX = ViewOriginX();
		int viewY = ViewOriginY();
		const int* wallHeights = m_renderer.WallHeights();

		for (int i = 0; i < m_renderer.VisSpriteCount(); i++) {
			const auto& vis = m_renderer.VisSprites()[i];
			auto srcBitmap = getBitmap(Wolf3dSpriteBufferId(vis.shapenum));
			if (!srcBitmap || srcBitmap->format != PixelFormat::RGBA2222) continue;

			// Sprites are scaled uniformly on both axes to vis.viewheight,
			// mirroring the original's ScaleShape (same scale factor
			// horizontally and vertically, regardless of source aspect).
			int destSize = (int)vis.viewheight;
			if (destSize <= 0) continue;
			int left = vis.viewx - destSize / 2;
			int top = viewheight / 2 - destSize / 2;

			int clippedLeft = std::max(left, 0);
			int clippedRight = std::min(left + destSize, viewwidth);
			int clippedTop = std::max(top, 0);
			int clippedBottom = std::min(top + destSize, viewheight);
			int destWidth = clippedRight - clippedLeft;
			int destHeight = clippedBottom - clippedTop;
			if (destWidth <= 0 || destHeight <= 0) continue;

			processor.bufferClear(WOLF3D_SCRATCH_SPRITE_BUFFER_ID);
			auto scratch = processor.bufferCreate(WOLF3D_SCRATCH_SPRITE_BUFFER_ID, (uint32_t)destWidth * destHeight);
			if (!scratch) continue;

			Wolf3dRenderer::SampleSprite(srcBitmap->data, srcBitmap->width, srcBitmap->height,
			                             destSize, destSize, clippedLeft - left, clippedTop - top,
			                             scratch->getBuffer(), destWidth, destHeight);
			Wolf3dRenderer::MaskSpriteColumnsBehindWalls(
				scratch->getBuffer(), destWidth, destHeight, clippedLeft,
				wallHeights, viewwidth, destSize);

			processor.createBitmapFromBuffer(WOLF3D_SCRATCH_SPRITE_BUFFER_ID, 1 /* RGBA2222 */, destWidth, destHeight);
			auto spriteBitmap = getBitmap(WOLF3D_SCRATCH_SPRITE_BUFFER_ID);
			if (spriteBitmap) {
				canvas->drawBitmap(viewX + clippedLeft, viewY + clippedTop, spriteBitmap.get());
				// Preserve the queued bitmap and backing bytes until
				// FabGL has consumed them, before scratch reuse.
				waitPlotCompletion(false);
			}
		}
	}

	// Draw the first-person weapon last in the 3D viewport, matching
	// WL_DRAW.C::DrawPlayerWeapon. It is an unoccluded screen-space overlay:
	// world wall heights must not mask it. SimpleScaleShape uses viewheight+1
	// as the uniform scale and centers the shape at viewwidth/2; C++ integer
	// division also leaves the single excess row clipped at the bottom.
	void RenderViewWeapon(VDUStreamProcessor& processor) {
		if (m_view_weapon_shapenum < 0) return;

		auto srcBitmap = getBitmap(Wolf3dSpriteBufferId(m_view_weapon_shapenum));
		if (!srcBitmap || srcBitmap->format != PixelFormat::RGBA2222) return;

		const int viewwidth = m_renderer.ViewWidth();
		const int viewheight = m_renderer.ViewHeight();
		const int destSize = viewheight + 1;
		const int left = viewwidth / 2 - destSize / 2;
		const int top = (viewheight - destSize) / 2;
		const int clippedLeft = std::max(left, 0);
		const int clippedRight = std::min(left + destSize, viewwidth);
		const int clippedTop = std::max(top, 0);
		const int clippedBottom = std::min(top + destSize, viewheight);
		const int destWidth = clippedRight - clippedLeft;
		const int destHeight = clippedBottom - clippedTop;
		if (destWidth <= 0 || destHeight <= 0) return;

		processor.bufferClear(WOLF3D_SCRATCH_WEAPON_BUFFER_ID);
		auto scratch = processor.bufferCreate(WOLF3D_SCRATCH_WEAPON_BUFFER_ID,
			(uint32_t)destWidth * destHeight);
		if (!scratch) return;

		Wolf3dRenderer::SampleSprite(srcBitmap->data, srcBitmap->width,
			srcBitmap->height, destSize, destSize, clippedLeft - left,
			clippedTop - top, scratch->getBuffer(), destWidth, destHeight);
		processor.createBitmapFromBuffer(WOLF3D_SCRATCH_WEAPON_BUFFER_ID,
			1 /* RGBA2222 */, destWidth, destHeight);
		auto weaponBitmap = getBitmap(WOLF3D_SCRATCH_WEAPON_BUFFER_ID);
		if (weaponBitmap) {
			canvas->drawBitmap(ViewOriginX() + clippedLeft,
				ViewOriginY() + clippedTop, weaponBitmap.get());
			waitPlotCompletion(false);
		}
	}

	int ViewOriginX() const {
		return (WOLF3D_SCREEN_WIDTH - m_renderer.ViewWidth()) / 2;
	}

	int ViewOriginY() const {
		return (WOLF3D_PLAY_AREA_HEIGHT - m_renderer.ViewHeight()) / 2;
	}

	void DrawHudPic(uint16_t chunkId, int x, int y) {
		auto bitmap = getBitmap(Wolf3dHudBufferId(chunkId));
		if (bitmap) canvas->drawBitmap(x, WOLF3D_STATUS_Y + y, bitmap.get());
	}

	void DrawHudNumber(int x, int y, int width, uint32_t value) {
		uint32_t modulus = 1;
		for (int i = 0; i < width; i++) modulus *= 10;
		value %= modulus;
		uint32_t divisor = modulus / 10;
		bool started = false;
		for (int i = 0; i < width; i++) {
			uint8_t digit = (uint8_t)(value / divisor);
			value %= divisor;
			uint16_t chunk = WOLF3D_HUD_DIGIT0_CHUNK + digit;
			if (!started && digit == 0 && i != width - 1) {
				chunk = WOLF3D_HUD_BLANK_CHUNK;
			} else {
				started = true;
			}
			DrawHudPic(chunk, x + i * 8, y);
			if (divisor > 1) divisor /= 10;
		}
	}

	// Compose the original 320x40 status panel at mode-8 rows 200..239 on
	// every rendered back buffer. The extra mode-8 height belongs to the play
	// surround above; there is no synthetic lower-panel extension.
	void RenderStatusBar() {
		canvas->setBrushColor(85, 85, 85);
		canvas->fillRectangle(0, WOLF3D_STATUS_Y,
			WOLF3D_SCREEN_WIDTH - 1, WOLF3D_STATUS_Y + WOLF3D_STATUS_HEIGHT - 1);
		DrawHudPic(WOLF3D_HUD_STATUSBAR_CHUNK, 0, 0);

		DrawHudNumber(16, 16, 2, m_statusBar.Level());
		DrawHudNumber(48, 16, 6, m_statusBar.Score());
		DrawHudNumber(112, 16, 1, m_statusBar.Lives());

		uint16_t faceChunk = WOLF3D_HUD_FACE_DEAD_CHUNK;
		if (m_statusBar.Health() != 0) {
			uint8_t damageBand = (uint8_t)((100 - std::min<uint8_t>(m_statusBar.Health(), 100)) / 16);
			faceChunk = WOLF3D_HUD_FACE1A_CHUNK + damageBand * 3 + m_statusBar.FaceFrame();
		}
		DrawHudPic(faceChunk, 136, 4);
		DrawHudNumber(168, 16, 3, m_statusBar.Health());
		DrawHudNumber(216, 16, 2, m_statusBar.Ammo());
		DrawHudPic((m_statusBar.KeyFlags() & 1) ? WOLF3D_HUD_GOLDKEY_CHUNK
			: WOLF3D_HUD_NOKEY_CHUNK, 240, 4);
		DrawHudPic((m_statusBar.KeyFlags() & 2) ? WOLF3D_HUD_SILVERKEY_CHUNK
			: WOLF3D_HUD_NOKEY_CHUNK, 240, 20);
		DrawHudPic(WOLF3D_HUD_KNIFE_CHUNK + std::min<uint8_t>(m_statusBar.Weapon(), 3),
			256, 8);
	}

	// VDU ... &4A, 10..17: HUD/status-bar field updates, one per WL_AGENT.C
	// Draw*() analog (see wolf3d/hud/wolf3d_status.h).
	void draw_health(VDUStreamProcessor& processor) { m_statusBar.DrawHealth(processor.readByte_t()); }
	void draw_ammo(VDUStreamProcessor& processor)   { m_statusBar.DrawAmmo(processor.readByte_t()); }
	void draw_keys(VDUStreamProcessor& processor)   { m_statusBar.DrawKeys(processor.readByte_t()); }
	void draw_weapon(VDUStreamProcessor& processor) { m_statusBar.DrawWeapon(processor.readByte_t()); }
	void draw_score(VDUStreamProcessor& processor)  { m_statusBar.DrawScore((uint32_t)read_long(processor)); }
	void draw_level(VDUStreamProcessor& processor)  { m_statusBar.DrawLevel(processor.readByte_t()); }
	void draw_face(VDUStreamProcessor& processor)   { m_statusBar.DrawFace(processor.readByte_t()); }
	void draw_lives(VDUStreamProcessor& processor)  { m_statusBar.DrawLives(processor.readByte_t()); }

	void PlotFizzleUntil(Wolf3dFizzle& fizzle, uint32_t target,
	                     uint8_t rawRed) {
		Wolf3dFizzlePixel pixel;
		const int originX = ViewOriginX();
		const int originY = ViewOriginY();
		while (fizzle.EmittedPixels() < target && fizzle.Next(pixel)) {
			uint8_t *scanline = _VGAController->getScanline(originY + pixel.y);
			// FabGL's raw scanline bytes are dword-packed in 2,3,0,1 order.
			scanline[(originX + pixel.x) ^ 2] = rawRed;
		}
	}

	// VDU ... &4A, 9, token; -- nonabortable, viewport-only red fizzle.
	// This handler deliberately owns presentation until the one-second effect
	// completes. The client must not submit a render while its token is
	// outstanding. Each cumulative batch is drawn into the hidden buffer,
	// presented at vertical blank, then replayed into the newly hidden buffer;
	// consequently both mode-8 surfaces finish byte-identical without C3.
	void fizzle_to_red(VDUStreamProcessor& processor) {
		auto token = processor.readWord_t();
		if (token < 0) return;

		waitPlotCompletion(false);
		const int originX = ViewOriginX();
		const int originY = ViewOriginY();
		const int viewwidth = m_renderer.ViewWidth();
		const int viewheight = m_renderer.ViewHeight();
		const bool validSurface = _VGAController
			&& originX >= 0 && originY >= 0
			&& originX + viewwidth <= canvasW
			&& originY + viewheight <= canvasH;

		if (validSurface) {
			Wolf3dFizzle fizzle((uint16_t)viewwidth, (uint16_t)viewheight);
			const uint8_t rawRed = _VGAController->createRawPixel(RGB222(2, 0, 0));
			for (uint16_t frame = 1;
			     frame <= WOLF3D_FIZZLE_DISPLAY_FRAMES; frame++) {
				const uint32_t target = Wolf3dFizzle::TargetForFrame(
					fizzle.TotalPixels(), frame, WOLF3D_FIZZLE_DISPLAY_FRAMES);
				Wolf3dFizzle replay = fizzle;
				PlotFizzleUntil(fizzle, target, rawRed);
				switchBuffer();
				PlotFizzleUntil(replay, target, rawRed);

				// VDU parsing stays paused by contract, but hardware input packets
				// must remain responsive throughout this blocking presentation.
				processor.handleKeyboardAndMouse();
				processor.processEventQueue();
			}
		} else {
			debug_log("Wolf3D fizzle: unsupported viewport/surface bounds\n\r");
		}

		m_presentation_sequence++;
		send_completion(processor, WOLF3D_NOTIFY_FIZZLE_COMPLETE,
			(uint16_t)token, m_presentation_sequence);
	}

	// VDU 23, 0, &A0, bufferId; &4A, 41, mode, token;
	// mode 0 disables notification; mode 1 emits a stock MOS keyboard packet.
	// Exact mirror of Pingo's set_render_notification (video/pingo_3d.h).
	void set_render_notification(VDUStreamProcessor& processor) {
		auto mode = processor.readByte_t();
		auto token = processor.readWord_t();
		if (mode < 0 || token < 0) {
			return;
		}
		m_render_notify_mode =
			mode == WOLF3D_RENDER_NOTIFY_KEYCODE
				? WOLF3D_RENDER_NOTIFY_KEYCODE
				: WOLF3D_RENDER_NOTIFY_DISABLED;
		m_render_notify_token = (uint16_t)token;
	}

	void send_completion(VDUStreamProcessor& processor, uint8_t event,
	                     uint16_t token, uint32_t sequence) {
		if (m_render_notify_mode != WOLF3D_RENDER_NOTIFY_KEYCODE) {
			return;
		}

		uint8_t packet[10] = {
			'W', '3', 'D', 'R',
			WOLF3D_RENDER_NOTIFY_VERSION,
			event,
			(uint8_t)(token & 0xFF),
			(uint8_t)(token >> 8),
			(uint8_t)(sequence & 0xFF),
			(uint8_t)((sequence >> 8) & 0xFF),
		};
		processor.send_packet(PACKET_KEYCODE, sizeof(packet), packet);
	}

	// Publish the completed hidden-buffer sequence after the renderer and HUD
	// draw queue have drained. The eZ80 callback only records this packet; its
	// foreground loop owns presentation and submission of the next frame.
	void send_render_complete(VDUStreamProcessor& processor, uint32_t sequence) {
		send_completion(processor, WOLF3D_NOTIFY_RENDER_COMPLETE,
			m_render_notify_token, sequence);
	}

	void handle_subcommand(VDUStreamProcessor& processor, uint8_t subcmd) {
		switch (subcmd) {
			case 0:  hello_world(processor); break;
			case 1:  init_level(processor); break;
			case 2:  set_player_pose(processor); break;
			case 3:  set_door(processor); break;
			case 4:  set_actor(processor); break;
			case 5:  remove_actor(processor); break;
			case 6:  set_static(processor); break;
			case 7:  render_frame(processor); break;
			case 8:  set_view_weapon(processor); break;
			case 9:  fizzle_to_red(processor); break;
			case 10: draw_health(processor); break;
			case 11: draw_ammo(processor); break;
			case 12: draw_keys(processor); break;
			case 13: draw_weapon(processor); break;
			case 14: draw_score(processor); break;
			case 15: draw_level(processor); break;
			case 16: draw_face(processor); break;
			case 17: draw_lives(processor); break;
			case 41: set_render_notification(processor); break;
		}
	}
} Wolf3dControl;

#endif // WOLF3D_H
