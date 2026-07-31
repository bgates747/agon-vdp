#ifndef WOLF3D_STATUS_H
#define WOLF3D_STATUS_H

#include <stdint.h>

// Original shareware-v1.4 graphics-chunk IDs. The bundled data is a later
// revision than the checked-in 1992 GFXV_WL1.H table, whose values are eight
// chunks earlier; keep these decoded-data IDs as the asset contract.
#define WOLF3D_HUD_BUFFER_BASE       0x4000
#define WOLF3D_HUD_STATUS_PANEL_ID   0x40F0
#define WOLF3D_HUD_STATUSBAR_CHUNK   98
#define WOLF3D_HUD_KNIFE_CHUNK       103
#define WOLF3D_HUD_NOKEY_CHUNK       107
#define WOLF3D_HUD_GOLDKEY_CHUNK     108
#define WOLF3D_HUD_SILVERKEY_CHUNK   109
#define WOLF3D_HUD_BLANK_CHUNK       110
#define WOLF3D_HUD_DIGIT0_CHUNK      111
#define WOLF3D_HUD_FACE1A_CHUNK      121
#define WOLF3D_HUD_FACE_DEAD_CHUNK   142

#define WOLF3D_SCREEN_WIDTH          320
#define WOLF3D_PLAY_AREA_HEIGHT      160
#define WOLF3D_STATUS_Y              160
#define WOLF3D_STATUS_HEIGHT         80

static inline uint16_t Wolf3dHudBufferId(uint16_t chunkId) {
	return (uint16_t)(WOLF3D_HUD_BUFFER_BASE + chunkId);
}

// Persistent eZ80-supplied HUD state. The Draw* names mirror WL_AGENT.C's
// granular update API, but drawing happens once inside render_frame so both
// hardware back buffers always receive a complete play screen.
class Wolf3dStatusBar {
public:
	Wolf3dStatusBar() = default;

	void DrawHealth(uint8_t health)   { m_health = health; }
	void DrawAmmo(uint8_t ammo)       { m_ammo = ammo; }
	void DrawKeys(uint8_t keyFlags)   { m_keyFlags = keyFlags; }
	void DrawWeapon(uint8_t weapon)   { m_weapon = weapon; }
	void DrawScore(uint32_t score)    { m_score = score; }
	void DrawLevel(uint8_t level)     { m_level = level; }
	void DrawFace(uint8_t faceFrame)  { m_faceFrame = faceFrame > 2 ? 2 : faceFrame; }
	void DrawLives(uint8_t lives)     { m_lives = lives; }

	uint8_t Health() const   { return m_health; }
	uint8_t Ammo() const     { return m_ammo; }
	uint8_t KeyFlags() const { return m_keyFlags; }
	uint8_t Weapon() const   { return m_weapon; }
	uint32_t Score() const   { return m_score; }
	uint8_t Level() const    { return m_level; }
	uint8_t FaceFrame() const { return m_faceFrame; }
	uint8_t Lives() const    { return m_lives; }

private:
	uint8_t  m_health = 100;
	uint8_t  m_ammo = 8;
	uint8_t  m_keyFlags = 0;
	uint8_t  m_weapon = 1;       // pistol
	uint32_t m_score = 0;
	uint8_t  m_level = 1;
	uint8_t  m_faceFrame = 0;
	uint8_t  m_lives = 3;
};

#endif // WOLF3D_STATUS_H
