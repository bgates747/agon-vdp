#ifndef WOLF3D_H
#define WOLF3D_H

#include <stdint.h>

#include "agon.h"
#include "vdu_stream_processor.h"

// Wolf3D VDP extension glue. Mirrors video/pingo_3d.h's structure: this file
// is the dispatch/bridge code living directly under video/, with the
// renderer implementation itself (once it exists) under video/wolf3d/ --
// see video/wolf3d/README.md for the layout rationale.
//
// Render-completion transport is a deliberate parity mirror of Pingo's
// subcommand 41 (video/pingo_3d.h's set_render_notification/
// send_render_complete): same subcommand number, same enable/disable +
// token shape, same stock-keyboard-packet delivery. Only the wire magic
// differs ("W3DR" vs Pingo's "P3DR") so a client watching for both
// extensions' completion packets on the same UART can tell them apart.
#define WOLF3D_RENDER_NOTIFY_DISABLED 0
#define WOLF3D_RENDER_NOTIFY_KEYCODE  1
#define WOLF3D_RENDER_NOTIFY_VERSION  1
#define WOLF3D_RENDER_NOTIFY_COMPLETE 1

typedef struct tag_Wolf3dControl {
	uint8_t             m_render_notify_mode;   // Opt-in render-completion transport
	uint16_t            m_render_notify_token;  // Caller-supplied completion token
	uint32_t            m_render_sequence;      // Diagnostic sequence for render timing records

	// VDU 23, 0, &A0, bufferId; &4A, 0: dispatch smoke test, no scene/render
	// state exists yet -- this just proves the opcode/subcommand plumbing.
	void hello_world(VDUStreamProcessor& processor) {
		debug_log("Wolf3D: Hello from Castle Wolfenstein 3D!\n\r");
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

	// Not yet called from anywhere: no render pipeline exists yet (see
	// video/wolf3d/render/README.md). Wire this up the same way Pingo calls
	// send_render_complete() from render_to_bitmap() once the column
	// renderer lands.
	void send_render_complete(VDUStreamProcessor& processor, uint32_t sequence) {
		if (m_render_notify_mode != WOLF3D_RENDER_NOTIFY_KEYCODE) {
			return;
		}

		uint8_t packet[10] = {
			'W', '3', 'D', 'R',
			WOLF3D_RENDER_NOTIFY_VERSION,
			WOLF3D_RENDER_NOTIFY_COMPLETE,
			(uint8_t)(m_render_notify_token & 0xFF),
			(uint8_t)(m_render_notify_token >> 8),
			(uint8_t)(sequence & 0xFF),
			(uint8_t)((sequence >> 8) & 0xFF),
		};
		processor.send_packet(PACKET_KEYCODE, sizeof(packet), packet);
	}

	void handle_subcommand(VDUStreamProcessor& processor, uint8_t subcmd) {
		switch (subcmd) {
			case 0:  hello_world(processor); break;
			case 41: set_render_notification(processor); break;
		}
	}
} Wolf3dControl;

#endif // WOLF3D_H
