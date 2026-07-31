#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <vector>

// wolf3d_draw.h only needs buffers.find(), a first stream, size(), and
// getBuffer() from the production buffered-command store.  Supplying that
// narrow interface here keeps this geometry regression independent of the
// FabGL userspace runtime while exercising the production renderer itself.
class RendererTestBuffer {
public:
	explicit RendererTestBuffer(std::size_t size) : bytes(size) {}

	std::size_t size() const { return bytes.size(); }
	std::uint8_t* getBuffer() { return bytes.data(); }
	const std::uint8_t* getBuffer() const { return bytes.data(); }

	std::vector<std::uint8_t> bytes;
};

std::unordered_map<std::uint16_t,
	std::vector<std::shared_ptr<RendererTestBuffer>>> buffers;

// The production buffer header defines the same global name plus the entire
// buffered-command implementation.  Its include guard lets this unit test
// substitute only the renderer-facing interface above.
#define BUFFERS_H
#include "video/wolf3d/render/wolf3d_draw.h"

namespace {

constexpr std::uint16_t kTilemapBufferId = 0x1234;
constexpr int kCenterColumn = WOLF3D_DEFAULT_VIEWWIDTH / 2 - 1;

struct Fixture {
	Wolf3dWorldState world{};
	std::shared_ptr<RendererTestBuffer> tilemap;
	Wolf3dRenderer renderer;

	Fixture() : tilemap(std::make_shared<RendererTestBuffer>(
		WOLF3D_MAPSIZE * WOLF3D_MAPSIZE)), renderer(world) {
		buffers.clear();
		buffers[kTilemapBufferId].push_back(tilemap);
		world.init_level(kTilemapBufferId);
		for (int i = 0; i < WOLF3D_MAPSIZE; ++i) {
			setTile(i, 0, 1);
			setTile(i, WOLF3D_MAPSIZE - 1, 1);
			setTile(0, i, 1);
			setTile(WOLF3D_MAPSIZE - 1, i, 1);
		}
	}

	void setTile(int x, int y, std::uint8_t tile) {
		tilemap->bytes[y * WOLF3D_MAPSIZE + x] = tile;
	}

	void setDoor(int x, int y, bool vertical, std::uint16_t position) {
		setTile(x, y, WOLF3D_TILE_DOOR_FLAG);
		world.set_door(0, x, y, vertical, 0,
			Wolf3dDoor::action_opening, position, 121);
	}

	void cast(wolf3d_fixed_t x, wolf3d_fixed_t y, int angle) {
		world.set_player_pose(x, y, angle);
		renderer.SetupView();
		renderer.WallRefresh();
	}
};

[[noreturn]] void fail(const char* test, const char* field, int expected,
	int actual) {
	std::fprintf(stderr, "%s: expected %s=%d, got %d\n",
		test, field, expected, actual);
	std::exit(1);
}

void expect(const char* test, const char* field, int expected, int actual) {
	if (expected != actual) fail(test, field, expected, actual);
}

void expectPixel(const char* test, const std::vector<std::uint8_t>& pixels,
	int width, int x, int y, std::uint8_t expected) {
	expect(test, "pixel", expected, pixels[(std::size_t)y * width + x]);
}

void testVerticalDoorTextureTracksPanel() {
	Fixture f;
	f.setDoor(4, 4, true, 0x4000);
	f.cast(2 * WOLF3D_TILEGLOBAL + 0x8000,
	       4 * WOLF3D_TILEGLOBAL + 0xC000, 0);

	expect("vertical moving door", "tile", WOLF3D_TILE_DOOR_FLAG,
	       f.renderer.WallTiles()[kCenterColumn]);
	expect("vertical moving door", "side", 0,
	       f.renderer.WallSides()[kCenterColumn]);
	expect("vertical moving door", "texture U", 32,
	       f.renderer.WallTexU()[kCenterColumn]);
}

void testHorizontalDoorTextureTracksPanel() {
	Fixture f;
	f.setDoor(4, 4, false, 0x5000);
	f.cast(4 * WOLF3D_TILEGLOBAL + 0xD000,
	       2 * WOLF3D_TILEGLOBAL + 0x8000, 270);

	expect("horizontal moving door", "tile", WOLF3D_TILE_DOOR_FLAG,
	       f.renderer.WallTiles()[kCenterColumn]);
	expect("horizontal moving door", "side", 1,
	       f.renderer.WallSides()[kCenterColumn]);
	expect("horizontal moving door", "texture U", 32,
	       f.renderer.WallTexU()[kCenterColumn]);
}

void testDoorPassesAtExactOpenBoundary() {
	Fixture f;
	f.setDoor(4, 4, true, 0xC000);
	f.setTile(6, 4, 7);
	f.cast(2 * WOLF3D_TILEGLOBAL + 0x8000,
	       4 * WOLF3D_TILEGLOBAL + 0xC000, 0);

	expect("door exact pass boundary", "tile behind door", 7,
	       f.renderer.WallTiles()[kCenterColumn]);
}

void testVerticalDoorPerpendicularJamb() {
	Fixture f;
	f.setDoor(4, 4, true, 0xFFFF);
	f.setTile(4, 5, 9);
	f.cast(3 * WOLF3D_TILEGLOBAL + 0x4000,
	       3 * WOLF3D_TILEGLOBAL + 0xC000, 315);

	expect("vertical door jamb", "jamb texture",
	       WOLF3D_DOOR_JAMB_TEXTURE_ID,
	       f.renderer.WallTiles()[kCenterColumn]);
	expect("vertical door jamb", "wall side", 1,
	       f.renderer.WallSides()[kCenterColumn]);
}

void testHorizontalDoorPerpendicularJamb() {
	Fixture f;
	f.setDoor(4, 4, false, 0xFFFF);
	f.setTile(5, 4, 9);
	f.cast(3 * WOLF3D_TILEGLOBAL + 0xC000,
	       3 * WOLF3D_TILEGLOBAL + 0x4000, 315);

	expect("horizontal door jamb", "jamb texture",
	       WOLF3D_DOOR_JAMB_TEXTURE_ID,
	       f.renderer.WallTiles()[kCenterColumn]);
	expect("horizontal door jamb", "wall side", 0,
	       f.renderer.WallSides()[kCenterColumn]);
}

void testDoorParallelWallsStayOrdinary() {
	{
		Fixture f;
		f.setDoor(4, 4, true, 0xFFFF);
		f.setTile(5, 4, 9);
		f.cast(2 * WOLF3D_TILEGLOBAL + 0x8000,
		       4 * WOLF3D_TILEGLOBAL + 0xC000, 0);
		expect("vertical door parallel wall", "wall texture", 9,
		       f.renderer.WallTiles()[kCenterColumn]);
	}

	{
		Fixture f;
		f.setDoor(4, 4, false, 0xFFFF);
		f.setTile(4, 5, 10);
		f.cast(4 * WOLF3D_TILEGLOBAL + 0xD000,
		       2 * WOLF3D_TILEGLOBAL + 0x8000, 270);
		expect("horizontal door parallel wall", "wall texture", 10,
		       f.renderer.WallTiles()[kCenterColumn]);
	}
}

void testActorSlotsNormalizeRenderStateAndRemove() {
	Fixture f;
	f.world.set_actor(149, 50, 0x00058000, 0x00034000, 540, 8);
	const auto& last = f.world.actors[149];
	expect("last actor slot", "shape", 50, last.shapenum);
	expect("last actor slot", "x", 0x00058000, last.x);
	expect("last actor slot", "y", 0x00034000, last.y);
	expect("last actor slot", "normalized facing", 180, last.facingAngle);
	expect("last actor slot", "rotations", 8, last.rotations);

	// An out-of-range id must not alias the final legal slot.
	f.world.set_actor(150, 99, 1, 2, 3, 2);
	expect("out-of-range actor", "last shape retained", 50, last.shapenum);

	f.world.set_actor(7, 61, 0x00048000, 0x00048000, -90, 3);
	expect("negative actor facing", "normalized facing", 270,
	       f.world.actors[7].facingAngle);
	expect("invalid rotation count", "fixed shape fallback", 0,
	       f.world.actors[7].rotations);
	f.world.remove_actor(7);
	expect("remove actor", "inactive sentinel", -1, f.world.actors[7].shapenum);

	// The long-form alias remains idempotent for clients that already have a
	// complete dirty record in hand when an actor becomes non-rendering.
	f.world.set_actor(8, -1, 0, 0, 0, 0);
	expect("set actor removal alias", "inactive sentinel", -1,
	       f.world.actors[8].shapenum);
}

void testActorRotationBoundaries() {
	Fixture f;
	f.world.set_actor(1, 50, 0, 0, 180, 8);
	auto& actor = f.world.actors[1];
	actor.viewx = kCenterColumn; // remove the original off-center correction

	f.world.playerAngle = 22;
	expect("eight-way below boundary", "offset", 0,
	       f.renderer.CalcRotate(actor));
	f.world.playerAngle = 23;
	expect("eight-way above boundary", "offset", 1,
	       f.renderer.CalcRotate(actor));
	f.world.playerAngle = 337;
	expect("eight-way wrap below", "offset", 7,
	       f.renderer.CalcRotate(actor));
	f.world.playerAngle = 338;
	expect("eight-way wrap above", "offset", 0,
	       f.renderer.CalcRotate(actor));

	actor.rotations = 2;
	f.world.playerAngle = 157;
	expect("two-way below boundary", "offset", 0,
	       f.renderer.CalcRotate(actor));
	f.world.playerAngle = 158;
	expect("two-way above boundary", "offset", 4,
	       f.renderer.CalcRotate(actor));

	actor.rotations = 0;
	f.world.playerAngle = 23;
	expect("fixed actor frame", "offset", 0, f.renderer.CalcRotate(actor));
}

void testActorsShareProjectionAndDepthSortWithStatics() {
	Fixture f;
	f.world.set_player_pose(0x00028000, 0x00048000, 0);
	f.world.set_actor(4, 60, 0x00058000, 0x00048000, 180, 8);
	f.world.set_static(9, 7, 4, 3, 0);
	f.renderer.SetupView();
	f.renderer.DrawScaleds();

	expect("actor/static projection", "visible count", 2,
	       f.renderer.VisSpriteCount());
	// Far-to-near painter order: the tile-centered static at x=7.5 precedes
	// the actor at x=5.5. The actor's eight-way frame is base+0 here.
	expect("actor/static projection", "far static shape", 3,
	       f.renderer.VisSprites()[0].shapenum);
	expect("actor/static projection", "near actor shape", 60,
	       f.renderer.VisSprites()[1].shapenum);

	f.world.remove_actor(4);
	f.renderer.DrawScaleds();
	expect("removed actor projection", "visible count", 1,
	       f.renderer.VisSpriteCount());
	expect("removed actor projection", "remaining static", 3,
	       f.renderer.VisSprites()[0].shapenum);

	f.world.set_static(9, 7, 4, -1, 0);
	f.world.set_actor(4, 60, 0x00018000, 0x00048000, 180, 0);
	f.renderer.DrawScaleds();
	expect("behind-camera actor", "visible count", 0,
	       f.renderer.VisSpriteCount());
}

void testSpriteMaskDepthBoundary() {
	constexpr int width = 3;
	constexpr int height = 2;
	const int wallHeights[width] = {63, 64, 65};
	std::vector<std::uint8_t> pixels(width * height, 0xFF);

	Wolf3dRenderer::MaskSpriteColumnsBehindWalls(
		pixels.data(), width, height, 0, wallHeights, width, 64);

	for (int y = 0; y < height; y++) {
		expectPixel("sprite farther wall", pixels, width, 0, y, 0xFF);
		expectPixel("sprite equality boundary", pixels, width, 1, y, 0x00);
		expectPixel("sprite nearer wall", pixels, width, 2, y, 0x00);
	}
}

void testSpriteMaskUsesClippedViewOffset() {
	constexpr int viewWidth = 7;
	constexpr int destWidth = 3;
	constexpr int destHeight = 2;
	const int wallHeights[viewWidth] = {90, 90, 10, 80, 20, 90, 90};
	std::vector<std::uint8_t> pixels(destWidth * destHeight, 0xD5);

	// This scratch bitmap begins at viewport X=2 after horizontal clipping.
	// Its local columns must therefore consult wall columns 2, 3, and 4.
	Wolf3dRenderer::MaskSpriteColumnsBehindWalls(
		pixels.data(), destWidth, destHeight, 2, wallHeights, viewWidth, 50);

	for (int y = 0; y < destHeight; y++) {
		expectPixel("sprite clipped offset left", pixels, destWidth, 0, y, 0xD5);
		expectPixel("sprite clipped offset middle", pixels, destWidth, 1, y, 0x00);
		expectPixel("sprite clipped offset right", pixels, destWidth, 2, y, 0xD5);
	}
}

void testSpriteMaskPreservesVisibleTransparency() {
	constexpr int width = 2;
	constexpr int height = 3;
	const int wallHeights[width] = {1, 100};
	std::vector<std::uint8_t> pixels = {
		0x00, 0xC1,
		0xE2, 0x00,
		0x7F, 0xF3,
	};

	Wolf3dRenderer::MaskSpriteColumnsBehindWalls(
		pixels.data(), width, height, 0, wallHeights, width, 50);

	expectPixel("visible transparent retained", pixels, width, 0, 0, 0x00);
	expectPixel("visible opaque retained row 1", pixels, width, 0, 1, 0xE2);
	expectPixel("visible opaque retained row 2", pixels, width, 0, 2, 0x7F);
	for (int y = 0; y < height; y++) {
		expectPixel("occluded column transparent", pixels, width, 1, y, 0x00);
	}
}

} // namespace

int main() {
	testVerticalDoorTextureTracksPanel();
	testHorizontalDoorTextureTracksPanel();
	testDoorPassesAtExactOpenBoundary();
	testVerticalDoorPerpendicularJamb();
	testHorizontalDoorPerpendicularJamb();
	testDoorParallelWallsStayOrdinary();
	testActorSlotsNormalizeRenderStateAndRemove();
	testActorRotationBoundaries();
	testActorsShareProjectionAndDepthSortWithStatics();
	testSpriteMaskDepthBoundary();
	testSpriteMaskUsesClippedViewOffset();
	testSpriteMaskPreservesVisibleTransparency();
	std::puts("wolf3d_renderer_test: door, actor rotation/lifetime, depth sort, and sprite occlusion invariants pass");
	return 0;
}
