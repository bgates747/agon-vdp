#ifndef WOLF3D_FIZZLE_H
#define WOLF3D_FIZZLE_H

#include <stdint.h>

// The original VGA fizzle runs for 70 ticks at 70 Hz. Agon's display swap
// waits for a 60 Hz vertical blank, so sixty cumulative batches preserve the
// intended one-second duration without introducing a second timing source.
static constexpr uint16_t WOLF3D_FIZZLE_DISPLAY_FRAMES = 60;

struct Wolf3dFizzlePixel {
	uint16_t x;
	uint16_t y;
};

// Pure iterator for ID_VH.C::FizzleFade's 17-bit maximal-length LFSR. The
// original routine derives coordinates from the current state, then advances
// it; retain that order so the visible pattern is deterministic and directly
// comparable with the DOS source.
class Wolf3dFizzle {
public:
	Wolf3dFizzle(uint16_t width, uint16_t height)
		: m_width(width), m_height(height), m_total((uint32_t)width * height) {}

	static uint32_t AdvanceState(uint32_t state) {
		const bool lowBit = (state & 1U) != 0;
		state >>= 1;
		if (lowBit) state ^= 0x12000U;
		return state;
	}

	// Return the next in-bounds pixel. Out-of-viewport LFSR coordinates are
	// consumed but never emitted. Bounds are deliberately strict: the DOS
	// source's x>width/y>height test admits one extra row and column.
	bool Next(Wolf3dFizzlePixel& pixel) {
		if (m_complete || m_total == 0) {
			m_complete = true;
			return false;
		}

		while (!m_complete) {
			const uint32_t state = m_state;
			const uint16_t y = (uint16_t)(((state & 0xFFU) - 1U) & 0xFFU);
			const uint16_t x = (uint16_t)((state >> 8) & 0x1FFU);

			m_state = AdvanceState(state);
			if (m_state == 1U) m_complete = true;

			if (x < m_width && y < m_height) {
				pixel = { x, y };
				m_emitted++;
				return true;
			}
		}
		return false;
	}

	uint32_t State() const { return m_state; }
	uint32_t EmittedPixels() const { return m_emitted; }
	uint32_t TotalPixels() const { return m_total; }
	bool Complete() const { return m_complete; }

	// Cumulative pixel target for one display batch. The 64-bit product keeps
	// the helper valid for any future viewport dimensions.
	static uint32_t TargetForFrame(uint32_t totalPixels, uint16_t frame,
	                               uint16_t frameCount) {
		if (frameCount == 0) return totalPixels;
		if (frame >= frameCount) return totalPixels;
		return (uint32_t)(((uint64_t)totalPixels * frame) / frameCount);
	}

private:
	uint16_t m_width;
	uint16_t m_height;
	uint32_t m_total;
	uint32_t m_state = 1;
	uint32_t m_emitted = 0;
	bool m_complete = false;
};

#endif // WOLF3D_FIZZLE_H
