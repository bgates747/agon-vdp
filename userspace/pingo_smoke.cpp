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
	auto copyFramebuffer =
		loadSymbol<void (*)(int *, int *, void *, float *)>(
			handle, "copyVgaFramebuffer");
	auto setDebug = loadSymbol<void (*)(bool)>(
		handle, "setVdpDebugLogging");
	auto shutdown = loadSymbol<void (*)()>(handle, "vdp_shutdown");
	auto meshShadingMode = loadSymbol<bool (*)(
		std::uint16_t, std::uint16_t, std::uint8_t *)>(
		handle, "pingo_userspace_get_mesh_shading_mode");
	auto flatPatternState = loadSymbol<bool (*)(
		std::uint16_t, std::uint16_t *, std::uint8_t *,
		std::uint8_t *, std::uint8_t *, std::uint8_t *)>(
		handle, "pingo_userspace_get_flat_pattern_state");
	loadSymbol<void (*)()>(handle, "rendererRender");

	auto sendBytes = [&](const std::vector<std::uint8_t>& bytes) {
		for (auto byte : bytes) {
			while (!isCts()) {
				std::this_thread::sleep_for(
					std::chrono::microseconds(50));
			}
			send(byte);
		}
	};
	auto appendWord = [](
			std::vector<std::uint8_t>& bytes,
			std::uint16_t value) {
		bytes.push_back(static_cast<std::uint8_t>(value));
		bytes.push_back(static_cast<std::uint8_t>(value >> 8));
	};
	auto sendPingo = [&](std::uint8_t subcommand,
			const std::vector<std::uint16_t>& words) {
		std::vector<std::uint8_t> bytes = {
			23, 0, 0xA0, 0xE8, 0x03, 0x49, subcommand,
		};
		for (auto word : words) {
			appendWord(bytes, word);
		}
		sendBytes(bytes);
	};
	auto uploadConsolidatedBuffer = [&](
			std::uint16_t buffer,
			const std::vector<std::uint8_t>& payload) {
		std::vector<std::uint8_t> bytes = {
			23, 0, 0xA0,
			static_cast<std::uint8_t>(buffer),
			static_cast<std::uint8_t>(buffer >> 8),
			0,
		};
		appendWord(bytes, static_cast<std::uint16_t>(payload.size()));
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		sendBytes(bytes);
		sendBytes({
			23, 0, 0xA0,
			static_cast<std::uint8_t>(buffer),
			static_cast<std::uint8_t>(buffer >> 8),
			14,
		});
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
	auto findCompletion = [](
			const std::vector<std::uint8_t>& bytes,
			std::uint16_t token,
			std::uint16_t sequence) {
		const std::uint8_t expected[] = {
			0x81, 10,
			'P', '3', 'D', 'R', 1, 1,
			static_cast<std::uint8_t>(token),
			static_cast<std::uint8_t>(token >> 8),
			static_cast<std::uint8_t>(sequence),
			static_cast<std::uint8_t>(sequence >> 8),
		};
		for (std::size_t i = 0;
				i + sizeof(expected) <= bytes.size(); ++i) {
			bool match = true;
			for (std::size_t j = 0; j < sizeof(expected); ++j) {
				if (bytes[i + j] != expected[j]) {
					match = false;
					break;
				}
			}
			if (match) {
				return true;
			}
		}
		return false;
	};
	auto hasAnyCompletion = [](
			const std::vector<std::uint8_t>& bytes) {
		const std::uint8_t prefix[] = {
			0x81, 10, 'P', '3', 'D', 'R', 1, 1,
		};
		for (std::size_t i = 0;
				i + sizeof(prefix) <= bytes.size(); ++i) {
			bool match = true;
			for (std::size_t j = 0; j < sizeof(prefix); ++j) {
				if (bytes[i + j] != prefix[j]) {
					match = false;
					break;
				}
			}
			if (match) {
				return true;
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

	// Create native RGBA2222 target 257.
	sendBytes({23, 27, 0x20, 1, 1});
	sendBytes({23, 27, 0x22, 64, 0, 64, 0, 0xF3});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Also create legacy RGBA8888 target 258.
	sendBytes({23, 27, 0x20, 2, 1});
	sendBytes({
		23, 27, 2,
		64, 0,
		64, 0,
		0xFF, 0x00, 0xFF, 0xFF,
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Create control buffer 1000 and render an empty scene to both formats.
	sendBytes({23, 0, 0xA0, 0xE8, 0x03, 0x49, 0, 64, 0, 64, 0});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	sendBytes({23, 0, 0xA0, 0xE8, 0x03, 0x49, 38, 1, 1});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	auto disabledBytes = receiveBytes(std::chrono::milliseconds(10));
	if (hasAnyCompletion(disabledBytes)) {
		std::fprintf(stderr, "render notification was not disabled by default\n");
		shutdown();
		return 1;
	}

	// Opt in with token 0xC35A, then verify both output formats signal only
	// after their complete render path has returned.
	sendBytes({
		23, 0, 0xA0, 0xE8, 0x03, 0x49, 41, 1, 0x5A, 0xC3,
	});
	sendBytes({23, 0, 0xA0, 0xE8, 0x03, 0x49, 38, 2, 1});
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	auto enabledBytes = receiveBytes(std::chrono::milliseconds(10));
	if (!findCompletion(enabledBytes, 0xC35A, 1)) {
		std::fprintf(stderr, "missing RGBA8888 render completion packet\n");
		shutdown();
		return 1;
	}

	sendBytes({
		23, 0, 0xA0, 0xE8, 0x03, 0x49, 41, 0, 0, 0,
	});
	sendBytes({23, 0, 0xA0, 0xE8, 0x03, 0x49, 38, 1, 1});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	auto reDisabledBytes = receiveBytes(std::chrono::milliseconds(10));
	if (hasAnyCompletion(reDisabledBytes)) {
		std::fprintf(stderr, "render notification did not disable\n");
		shutdown();
		return 1;
	}

	/*
	 * Exercise malformed mesh ingestion through the actual VDU bridge. A
	 * rejected non-triplet replacement must consume its entire payload, and
	 * an out-of-range replacement must make the object non-renderable rather
	 * than dereferencing it. A later valid component set must recover without
	 * recreating the control structure.
	 */
	sendPingo(1, {
		7, 3,
		0xC000, 0xC000, 0xC000,
		0x4000, 0xC000, 0xC000,
		0x0000, 0x4000, 0xC000,
	});
	sendPingo(2, {7, 4, 0, 1, 2, 0});
	sendPingo(2, {7, 3, 0, 1, 9});
	sendPingo(4, {7, 3, 0, 1, 2});
	sendPingo(5, {7, 7, 257});

	sendBytes({
		23, 0, 0xA0, 0xE8, 0x03, 0x49, 41, 1, 0xA7, 0xC3,
	});
	sendBytes({23, 0, 0xA0, 0xE8, 0x03, 0x49, 38, 1, 1});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	auto invalidMeshBytes = receiveBytes(std::chrono::milliseconds(10));
	if (!findCompletion(invalidMeshBytes, 0xC3A7, 3)) {
		std::fprintf(
			stderr,
			"malformed mesh upload desynchronized or crashed the VDU bridge\n");
		shutdown();
		return 1;
	}

	sendPingo(2, {7, 3, 0, 1, 2});
	sendPingo(3, {
		7, 3,
		0x0000, 0x0000,
		0xFFFF, 0x0000,
		0x0000, 0xFFFF,
	});
	sendPingo(4, {7, 3, 0, 1, 2});
	sendBytes({23, 0, 0xA0, 0xE8, 0x03, 0x49, 38, 1, 1});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	auto recoveredMeshBytes = receiveBytes(std::chrono::milliseconds(10));
	if (!findCompletion(recoveredMeshBytes, 0xC3A7, 4)) {
		std::fprintf(
			stderr,
			"valid replacement did not recover malformed mesh state\n");
		shutdown();
		return 1;
	}

	/*
	 * Exercise the experimental flat-pattern path through generic-buffer
	 * upload, consolidation, resource binding, mesh policy, and a completed
	 * native render. The selector bitmap's pixels are deliberately irrelevant:
	 * UV texel position zero is the compact material ID.
	 */
	constexpr std::uint16_t selectorBitmap = 259;
	constexpr std::uint16_t patternBuffer = 410;
	constexpr std::uint16_t lookupBuffer = 411;
	std::vector<std::uint8_t> pattern(16);
	for (std::uint8_t phase = 0; phase < pattern.size(); phase++) {
		pattern[phase] = static_cast<std::uint8_t>(0xC0u | phase);
	}
	uploadConsolidatedBuffer(patternBuffer, pattern);
	uploadConsolidatedBuffer(lookupBuffer, {0, 0});
	sendBytes({23, 27, 0x20, 3, 1});
	sendBytes({23, 27, 0x22, 4, 0, 4, 0, 0xC0});
	sendPingo(5, {7, 7, selectorBitmap});

	std::vector<std::uint8_t> bind = {
		23, 0, 0xA0, 0xE8, 0x03, 0x49, 49,
	};
	appendWord(bind, patternBuffer);
	appendWord(bind, lookupBuffer);
	appendWord(bind, 1);
	bind.push_back(1);
	bind.push_back(2);
	sendBytes(bind);
	std::vector<std::uint8_t> patternMode = {
		23, 0, 0xA0, 0xE8, 0x03, 0x49, 47,
	};
	appendWord(patternMode, 7);
	patternMode.push_back(2);
	sendBytes(patternMode);
	sendBytes({23, 0, 0xA0, 0xE8, 0x03, 0x49, 38, 1, 1});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	auto patternedMeshBytes = receiveBytes(std::chrono::milliseconds(10));
	if (!findCompletion(patternedMeshBytes, 0xC3A7, 5)) {
		std::fprintf(
			stderr,
			"flat-pattern upload/bind/render did not complete\n");
		shutdown();
		return 1;
	}

	std::uint8_t shadingMode = 0;
	std::uint16_t patternCount = 0;
	std::uint8_t materialCount = 0;
	std::uint8_t bandCount = 0;
	std::uint8_t firstPatternPixel = 0;
	std::uint8_t firstLookupId = 0xFF;
	if (!meshShadingMode(1000, 7, &shadingMode) || shadingMode != 2 ||
		!flatPatternState(
			1000, &patternCount, &materialCount, &bandCount,
			&firstPatternPixel, &firstLookupId) ||
		patternCount != 1 || materialCount != 1 || bandCount != 2 ||
		firstPatternPixel != 0xC0 || firstLookupId != 0) {
		std::fprintf(
			stderr,
			"flat-pattern protocol state did not match uploaded resources\n");
		shutdown();
		return 1;
	}

	std::vector<std::uint8_t> framebuffer(1024 * 768 * 3);
	int width = 0;
	int height = 0;
	float frameRate = 0;
	copyFramebuffer(
		&width, &height, framebuffer.data(), &frameRate);

	if (width <= 0 || height <= 0 || frameRate <= 0) {
		std::fprintf(
			stderr,
			"invalid native VDP state: %dx%d at %.2f Hz\n",
			width,
			height,
			frameRate);
		shutdown();
		return 1;
	}

	std::printf(
		"Pingo dual-target native smoke passed: %dx%d at %.2f Hz\n",
		width,
		height,
		frameRate);
	shutdown();
	return 0;
}
