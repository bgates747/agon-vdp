#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "video/wolf3d/presentation/wolf3d_fizzle.h"
#include "video/wolf3d/hud/wolf3d_status.h"

static_assert(WOLF3D_SCREEN_WIDTH == 320, "mode-8 layout width changed");
static_assert(WOLF3D_PLAY_AREA_HEIGHT == 200,
	"mode-8 play surround must occupy rows 0..199");
static_assert(WOLF3D_STATUS_Y == 200,
	"original status strip must begin at mode-8 row 200");
static_assert(WOLF3D_STATUS_HEIGHT == 40,
	"status strip must retain the original 40-pixel height");

namespace {

[[noreturn]] void Fail(const char *message) {
	std::fprintf(stderr, "wolf3d_fizzle_test: %s\n", message);
	std::exit(1);
}

void Require(bool condition, const char *message) {
	if (!condition) Fail(message);
}

void PlotUntil(Wolf3dFizzle& fizzle, uint32_t target,
	             std::vector<uint8_t>& surface,
	             int surfaceWidth, int originX, int originY, uint8_t red) {
	Wolf3dFizzlePixel pixel;
	while (fizzle.EmittedPixels() < target && fizzle.Next(pixel)) {
		surface[(originY + pixel.y) * surfaceWidth + originX + pixel.x] = red;
	}
}

uint32_t CountViewport(const std::vector<uint8_t>& surface,
	                   int surfaceWidth, int originX, int originY,
	                   int viewWidth, int viewHeight, uint8_t value) {
	uint32_t count = 0;
	for (int y = 0; y < viewHeight; y++) {
		for (int x = 0; x < viewWidth; x++) {
			if (surface[(originY + y) * surfaceWidth + originX + x] == value) count++;
		}
	}
	return count;
}

} // namespace

int main() {
	Require(Wolf3dFizzle::AdvanceState(1) == 0x12000,
	        "first LFSR transition differs from ID_VH.C");

	uint32_t state = 1;
	uint32_t period = 0;
	do {
		state = Wolf3dFizzle::AdvanceState(state);
		period++;
		Require(state != 0, "LFSR fell into the zero state");
	} while (state != 1 && period <= 131071);
	Require(period == 131071, "LFSR period is not the expected 2^17-1");

	constexpr int viewWidth = 256;
	constexpr int viewHeight = 160;
	Wolf3dFizzle fizzle(viewWidth, viewHeight);
	const std::array<Wolf3dFizzlePixel, 16> expectedFirst = {{
		{0, 0}, {4, 127}, {2, 63}, {1, 31},
		{0, 143}, {0, 71}, {0, 35}, {0, 17},
		{0, 8}, {144, 1}, {72, 0}, {32, 127},
		{16, 63}, {8, 31}, {4, 15}, {2, 7},
	}};
	std::vector<uint8_t> seen(viewWidth * viewHeight, 0);
	Wolf3dFizzlePixel pixel;
	uint32_t emitted = 0;
	while (fizzle.Next(pixel)) {
		Require(pixel.x < viewWidth && pixel.y < viewHeight,
		        "iterator emitted a pixel outside strict viewport bounds");
		if (emitted < expectedFirst.size()) {
			Require(pixel.x == expectedFirst[emitted].x
			        && pixel.y == expectedFirst[emitted].y,
			        "initial deterministic coordinate sequence changed");
		}
		auto& visit = seen[pixel.y * viewWidth + pixel.x];
		Require(visit == 0, "iterator emitted a duplicate viewport pixel");
		visit = 1;
		emitted++;
	}
	Require(fizzle.Complete(), "iterator did not report a complete LFSR cycle");
	Require(emitted == (uint32_t)viewWidth * viewHeight,
	        "iterator did not cover every viewport pixel exactly once");
	Require(fizzle.EmittedPixels() == emitted,
	        "iterator's emitted-pixel accounting disagrees with traversal");
	Require(std::all_of(seen.begin(), seen.end(), [](uint8_t value) { return value == 1; }),
	        "viewport coverage contains holes");

	uint32_t previous = 0;
	uint32_t minBatch = UINT32_MAX;
	uint32_t maxBatch = 0;
	for (uint16_t frame = 1; frame <= WOLF3D_FIZZLE_DISPLAY_FRAMES; frame++) {
		uint32_t target = Wolf3dFizzle::TargetForFrame(
			viewWidth * viewHeight, frame, WOLF3D_FIZZLE_DISPLAY_FRAMES);
		Require(target >= previous, "display-batch targets are not monotonic");
		uint32_t batch = target - previous;
		minBatch = std::min(minBatch, batch);
		maxBatch = std::max(maxBatch, batch);
		previous = target;
	}
	Require(previous == (uint32_t)viewWidth * viewHeight,
	        "final display batch does not reach the full viewport");
	Require(minBatch == 682 && maxBatch == 683,
	        "60 Hz batches do not distribute pixels evenly");

	// Model the production hidden-buffer algorithm: advance one iterator into
	// the hidden surface, swap it visible, then replay an iterator snapshot
	// into the newly hidden surface. The two buffers must be cumulatively
	// identical after every vertical blank, and no play-border/HUD byte may
	// change.
	constexpr int screenWidth = WOLF3D_SCREEN_WIDTH;
	constexpr int screenHeight = WOLF3D_STATUS_Y + WOLF3D_STATUS_HEIGHT;
	constexpr int originX = (WOLF3D_SCREEN_WIDTH - viewWidth) / 2;
	constexpr int originY = (WOLF3D_PLAY_AREA_HEIGHT - viewHeight) / 2;
	static_assert(screenHeight == 240, "mode-8 composition must fill 320x240");
	static_assert(originX == 32 && originY == 20,
		"256x160 viewport must be centered at (32,20)");
	constexpr uint8_t untouched = 0x11;
	constexpr uint8_t red = 0x22;
	std::array<std::vector<uint8_t>, 2> surfaces = {{
		std::vector<uint8_t>(screenWidth * screenHeight, untouched),
		std::vector<uint8_t>(screenWidth * screenHeight, untouched),
	}};
	int hidden = 1;
	Wolf3dFizzle primary(viewWidth, viewHeight);
	for (uint16_t frame = 1; frame <= WOLF3D_FIZZLE_DISPLAY_FRAMES; frame++) {
		uint32_t target = Wolf3dFizzle::TargetForFrame(
			primary.TotalPixels(), frame, WOLF3D_FIZZLE_DISPLAY_FRAMES);
		Wolf3dFizzle replay = primary;
		PlotUntil(primary, target, surfaces[hidden], screenWidth,
		          originX, originY, red);
		hidden ^= 1; // swap: the surface just filled became visible
		PlotUntil(replay, target, surfaces[hidden], screenWidth,
		          originX, originY, red);
		Require(surfaces[0] == surfaces[1],
		        "mirrored double buffers diverged after a display batch");
		Require(CountViewport(surfaces[0], screenWidth, originX, originY,
		                      viewWidth, viewHeight, red) == target,
		        "cumulative visible pixel count differs from batch target");
	}

	for (int y = 0; y < screenHeight; y++) {
		for (int x = 0; x < screenWidth; x++) {
			bool inViewport = x >= originX && x < originX + viewWidth
			               && y >= originY && y < originY + viewHeight;
			uint8_t expected = inViewport ? red : untouched;
			Require(surfaces[0][y * screenWidth + x] == expected,
			        "fizzle modified the play border or HUD");
		}
	}

	std::printf("wolf3d_fizzle_test: period=%u, unique viewport pixels=%u, "
	            "60 mirrored batches=%u..%u: PASS\n",
	            period, emitted, minBatch, maxBatch);
	return 0;
}
