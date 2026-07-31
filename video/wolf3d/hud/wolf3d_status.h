#ifndef WOLF3D_STATUS_H
#define WOLF3D_STATUS_H

#include <stdint.h>

// Status bar (HUD) drawing, mirroring WL_AGENT.C's per-field Draw*()
// functions 1:1 by name -- the original redraws each stat independently
// when the eZ80-side game state changes it (e.g. GiveAmmo() only calls
// DrawAmmo()), so the BUFFERED_WOLF3D dispatch mirrors that same
// granularity instead of one big "update everything" packet.
//
// Status: scaffolded only -- bodies are stubs until the status-bar bitmap
// buffer + StatusDrawPic()-equivalent blit primitive exist (mirrors
// WL_AGENT.C's StatusDrawPic(), which blits a numbered pic at a fixed
// status-bar coordinate; see agonport/PORT_NOTES.md's "Status bar"
// terminology/layout notes).

class Wolf3dStatusBar {
public:
	// Mirrors WL_AGENT.C's DrawHealth().
	void DrawHealth(uint8_t health) {
		// TODO: StatusDrawPic-equivalent blit of the health digits.
	}

	// Mirrors WL_AGENT.C's DrawAmmo().
	void DrawAmmo(uint8_t ammo) {
		// TODO: StatusDrawPic-equivalent blit of the ammo digits.
	}

	// Mirrors WL_AGENT.C's DrawKeys() (gold/silver key indicators).
	void DrawKeys(uint8_t keyFlags) {
		// TODO: StatusDrawPic-equivalent blit of the key icons.
	}

	// Mirrors WL_AGENT.C's DrawWeapon().
	void DrawWeapon(uint8_t weapon) {
		// TODO: StatusDrawPic-equivalent blit of the weapon icon.
	}

	// Mirrors WL_AGENT.C's DrawScore().
	void DrawScore(uint32_t score) {
		// TODO: StatusDrawPic-equivalent blit of the score digits.
	}

	// Mirrors WL_AGENT.C's DrawLevel() (floor number).
	void DrawLevel(uint8_t level) {
		// TODO: StatusDrawPic-equivalent blit of the level digits.
	}

	// Mirrors WL_AGENT.C's DrawFace()/UpdateFace().
	void DrawFace(uint8_t faceFrame) {
		// TODO: StatusDrawPic-equivalent blit of the player face.
	}

	// Mirrors WL_AGENT.C's DrawLives().
	void DrawLives(uint8_t lives) {
		// TODO: StatusDrawPic-equivalent blit of the lives digits.
	}
};

#endif // WOLF3D_STATUS_H
