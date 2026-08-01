#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <thread>
#include <vector>

template<typename T>
T loadSymbol(void *handle, const char *name) {
	dlerror();
	auto symbol = reinterpret_cast<T>(dlsym(handle, name));
	if (const char *error = dlerror()) {
		std::fprintf(stderr, "missing ABI symbol %s: %s\n", name, error);
		std::exit(2);
	}
	return symbol;
}

int main(int argc, char **argv) {
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	if (argc != 2) {
		std::fprintf(stderr, "usage: %s VDP_SO\n", argv[0]);
		return 2;
	}

	auto handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return 2;
	}

	const char *fabAbiSymbols[] = {
		"vdp_setup",
		"vdp_loop",
		"signal_vblank",
		"copyVgaFramebuffer",
		"set_startup_screen_mode",
		"z80_uart0_is_cts",
		"z80_send_to_vdp",
		"z80_recv_from_vdp",
		"sendVKeyEventToFabgl",
		"sendPS2KbEventToFabgl",
		"sendHostMouseEventToFabgl",
		"setVdpDebugLogging",
		"getAudioSamples",
		"dump_vdp_mem_stats",
		"vdp_shutdown",
	};
	for (const auto *name : fabAbiSymbols) {
		loadSymbol<void *>(handle, name);
	}

	auto send = loadSymbol<void (*)(std::uint8_t)>(
		handle, "z80_send_to_vdp");
	auto receive = loadSymbol<bool (*)(std::uint8_t *)>(
		handle, "z80_recv_from_vdp");
	auto isCts = loadSymbol<bool (*)()>(handle, "z80_uart0_is_cts");
	auto setup = loadSymbol<void (*)()>(handle, "vdp_setup");
	auto setDebug = loadSymbol<void (*)(bool)>(
		handle, "setVdpDebugLogging");
	auto signalVblank = loadSymbol<void (*)()>(handle, "signal_vblank");
	auto copyFramebuffer = loadSymbol<void (*)(int *, int *, void *, float *)>(
		handle, "copyVgaFramebuffer");
	auto shutdown = loadSymbol<void (*)()>(handle, "vdp_shutdown");

	auto sendBytes = [&](const std::vector<std::uint8_t>& bytes) {
		for (auto byte : bytes) {
			while (!isCts()) {
				std::this_thread::sleep_for(
					std::chrono::microseconds(50));
			}
			send(byte);
		}
	};
	auto receiveBytes = [&](std::chrono::milliseconds quietPeriod) {
		std::vector<std::uint8_t> bytes;
		auto quietSince = std::chrono::steady_clock::now();
		while (std::chrono::steady_clock::now() - quietSince < quietPeriod) {
			std::uint8_t byte;
			if (receive(&byte)) {
				bytes.push_back(byte);
				quietSince = std::chrono::steady_clock::now();
			} else {
				std::this_thread::sleep_for(
					std::chrono::microseconds(100));
			}
		}
		return bytes;
	};
	auto hasCompletion = [](const std::vector<std::uint8_t>& bytes,
	                        std::uint8_t event, std::uint16_t token,
	                        int sequence = -1) {
		const std::uint8_t prefix[] = {0x81, 10, 'W', '3', 'D', 'R'};
		for (std::size_t i = 0; i + 12 <= bytes.size(); ++i) {
			bool match = true;
			for (std::size_t j = 0; j < sizeof(prefix); ++j) {
				if (bytes[i + j] != prefix[j]) {
					match = false;
					break;
				}
			}
			if (match && bytes[i + 6] == 1 && bytes[i + 7] == event
			    && bytes[i + 8] == (token & 0xFF)
			    && bytes[i + 9] == (token >> 8)
			    && (sequence < 0
			        || (bytes[i + 10] == (sequence & 0xFF)
			            && bytes[i + 11] == ((sequence >> 8) & 0xFF)))) {
				return true;
			}
		}
		return false;
	};
	auto waitForCompletion = [&](std::uint8_t event, std::uint16_t token,
	                             int sequence,
	                             std::chrono::milliseconds timeout) {
		std::vector<std::uint8_t> bytes;
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (std::chrono::steady_clock::now() < deadline) {
			std::uint8_t byte;
			if (receive(&byte)) {
				bytes.push_back(byte);
				if (hasCompletion(bytes, event, token, sequence)) return true;
			} else {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
		}
		return false;
	};

	setDebug(true);
	setup();

	// Let the VDP proceed past its eZ80 general-poll handshake.
	sendBytes({23, 0, 0x80, 1});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	receiveBytes(std::chrono::milliseconds(10));

	// VDU 23,0,&A0,0;&4A,0 -- hello-world dispatch smoke test.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 0});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Initialize an empty render snapshot, then exercise the actor ABI. The
	// notification command follows set_actor immediately: if the old 20-byte
	// parser (or any other over-read) consumes bytes beyond the exact 15-byte
	// payload, it destroys the only notification registration and no matching
	// completion can arrive. Missing tile/art buffers deliberately fail safe
	// during render.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 1, 2, 0});
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 2,
	           0, 128, 2, 0, 0, 128, 4, 0, 0, 0});
	// actor 3: base shape 50, (5.5,3.25), facing 180 degrees, 8 rotations.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 4,
	           3, 0, 50, 0,
	           0, 128, 5, 0, 0, 64, 3, 0,
	           180, 0, 8});

	// VDU 23,0,&A0,0;&4A,41,1,0x5A,0xC3; -- enable render-done notify.
	// Nothing triggers a render yet, so no W3DR packet is expected here.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 41, 1, 0x5A, 0xC3});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	auto afterEnable = receiveBytes(std::chrono::milliseconds(10));
	if (hasCompletion(afterEnable, 1, 0xC35A)) {
		std::fprintf(stderr,
			"unexpected W3DR completion before render request\n");
		shutdown();
		return 1;
	}

	// remove_actor and set_view_weapon immediately precede render_frame,
	// covering both payload boundaries. The 0xFFFF shape is the persistent
	// overlay-suppressed sentinel. A token-matched event-1 completion proves
	// the full stream remained aligned.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 5, 3, 0});
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 8, 0xFF, 0xFF});
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 7});
	if (!waitForCompletion(1, 0xC35A, 1, std::chrono::seconds(3))) {
		std::fprintf(stderr,
			"no W3DR completion after set_actor/remove_actor ABI stream\n");
		shutdown();
		return 1;
	}

	// Exercise the actual presentation handler in mode 8+128. Userspace has
	// no physical VGA timing, so a small pump supplies the vertical-blank
	// signals consumed by each swap. Event 2 must echo its command token and
	// use the independent presentation sequence. A following event-1 render
	// must be exactly the next render sequence, proving fizzle did not perturb
	// that counter.
	std::atomic<bool> pumpVblank { true };
	std::thread vblankThread([&]() {
		while (pumpVblank.load(std::memory_order_relaxed)) {
			signalVblank();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});
	auto stopVblank = [&]() {
		pumpVblank.store(false, std::memory_order_relaxed);
		vblankThread.join();
	};
	sendBytes({22, 136});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// Match the real client's presentation precondition: hide the text cursor,
	// render the same final view into each back buffer, and present each one.
	// Fizzle then begins with byte-identical play surround and HUD surfaces.
	sendBytes({23, 1, 0});
	for (int sequence = 2; sequence <= 3; sequence++) {
		sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 7});
		if (!waitForCompletion(1, 0xC35A, sequence,
		                       std::chrono::seconds(3))) {
			std::fprintf(stderr,
				"failed to initialize both fizzle surfaces at render %d\n",
				sequence);
			stopVblank();
			shutdown();
			return 1;
		}
		sendBytes({23, 0, 0xC3});
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	std::vector<std::uint8_t> beforeFizzle(1024 * 768 * 3);
	int beforeWidth = 0;
	int beforeHeight = 0;
	float frameRate = 0;
	copyFramebuffer(&beforeWidth, &beforeHeight, beforeFizzle.data(), &frameRate);
	if (beforeWidth != 320 || beforeHeight != 240) {
		std::fprintf(stderr,
			"mode 136 framebuffer is %dx%d instead of 320x240\n",
			beforeWidth, beforeHeight);
		stopVblank();
		shutdown();
		return 1;
	}
	struct LayoutPixel {
		int x;
		int y;
		std::uint8_t red;
		std::uint8_t green;
		std::uint8_t blue;
		const char *name;
	};
	// Pin the mode-8 composition before the fizzle obscures the view. The
	// empty-world smoke render still exercises the real surround, four-sided
	// bevel, centered viewport, and unextended 40-row status-bar fallback.
	const LayoutPixel layoutPixels[] = {
		{0, 0, 0, 85, 85, "top-left play surround"},
		{319, 199, 0, 85, 85, "bottom-right play surround"},
		{32, 20, 170, 170, 170, "viewport origin"},
		{31, 20, 0, 0, 0, "left bevel"},
		{288, 20, 0, 170, 170, "right bevel"},
		{32, 19, 0, 0, 0, "top bevel"},
		{32, 180, 0, 170, 170, "bottom bevel"},
		{31, 19, 0, 0, 0, "top-left bevel corner"},
		{288, 19, 0, 170, 170, "top-right bevel corner"},
		{31, 180, 0, 0, 0, "bottom-left bevel corner"},
		{288, 180, 0, 170, 170, "bottom-right bevel corner"},
		{0, 200, 85, 85, 85, "status-bar first row"},
		{319, 239, 85, 85, 85, "status-bar final row"},
	};
	for (const auto& expected : layoutPixels) {
		const std::size_t offset =
			((std::size_t)expected.y * beforeWidth + expected.x) * 3;
		if (beforeFizzle[offset] != expected.red
		    || beforeFizzle[offset + 1] != expected.green
		    || beforeFizzle[offset + 2] != expected.blue) {
			std::fprintf(stderr,
				"layout mismatch at %s (%d,%d): expected %u,%u,%u; got %u,%u,%u\n",
				expected.name, expected.x, expected.y,
				expected.red, expected.green, expected.blue,
				beforeFizzle[offset], beforeFizzle[offset + 1],
				beforeFizzle[offset + 2]);
			stopVblank();
			shutdown();
			return 1;
		}
	}
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 9, 0xEF, 0xBE});
	if (!waitForCompletion(2, 0xBEEF, 1, std::chrono::seconds(3))) {
		std::fprintf(stderr,
			"no token-matched event-2 W3DR fizzle completion\n");
		stopVblank();
		shutdown();
		return 1;
	}
	std::vector<std::uint8_t> afterFizzle(1024 * 768 * 3);
	int afterWidth = 0;
	int afterHeight = 0;
	copyFramebuffer(&afterWidth, &afterHeight, afterFizzle.data(), &frameRate);
	if (afterWidth != beforeWidth || afterHeight != beforeHeight) {
		std::fprintf(stderr, "framebuffer dimensions changed during fizzle\n");
		stopVblank();
		shutdown();
		return 1;
	}
	for (int y = 0; y < afterHeight; y++) {
		for (int x = 0; x < afterWidth; x++) {
			const std::size_t offset = ((std::size_t)y * afterWidth + x) * 3;
			const bool inViewport = x >= 32 && x < 288 && y >= 20 && y < 180;
			if (inViewport) {
				if (afterFizzle[offset] != 170 || afterFizzle[offset + 1] != 0
				    || afterFizzle[offset + 2] != 0) {
					std::fprintf(stderr,
						"fizzle left non-red viewport pixel at (%d,%d)\n", x, y);
					stopVblank();
					shutdown();
					return 1;
				}
			} else if (afterFizzle[offset] != beforeFizzle[offset]
			           || afterFizzle[offset + 1] != beforeFizzle[offset + 1]
			           || afterFizzle[offset + 2] != beforeFizzle[offset + 2]) {
				std::fprintf(stderr,
					"fizzle modified surround/HUD pixel at (%d,%d)\n", x, y);
				stopVblank();
				shutdown();
				return 1;
			}
		}
	}
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 7});
	if (!waitForCompletion(1, 0xC35A, 4, std::chrono::seconds(3))) {
		std::fprintf(stderr,
			"render sequence changed or stalled after fizzle completion\n");
		stopVblank();
		shutdown();
		return 1;
	}
	stopVblank();

	// VDU 23,0,&A0,0;&4A,41,0,0; -- disable again.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 41, 0, 0, 0});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	shutdown();
	std::printf("wolf3d_smoke: ABI symbols resolved; actor set/remove and "
		"view-weapon payloads remained aligned through token-matched "
		"event-1 completion; the mode-8 surround/bevel/HUD layout matched; "
		"event-2 fizzle filled only (32,20)-(287,179), "
		"completed with an independent sequence, and preserved render "
		"sequence 4\n");
	return 0;
}
