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
