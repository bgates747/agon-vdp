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
	                        std::uint16_t token) {
		const std::uint8_t prefix[] = {0x81, 10, 'W', '3', 'D', 'R'};
		for (std::size_t i = 0; i + 12 <= bytes.size(); ++i) {
			bool match = true;
			for (std::size_t j = 0; j < sizeof(prefix); ++j) {
				if (bytes[i + j] != prefix[j]) {
					match = false;
					break;
				}
			}
			if (match && bytes[i + 6] == 1 && bytes[i + 7] == 1
			    && bytes[i + 8] == (token & 0xFF)
			    && bytes[i + 9] == (token >> 8)) {
				return true;
			}
		}
		return false;
	};
	auto waitForCompletion = [&](std::uint16_t token,
	                             std::chrono::milliseconds timeout) {
		std::vector<std::uint8_t> bytes;
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (std::chrono::steady_clock::now() < deadline) {
			std::uint8_t byte;
			if (receive(&byte)) {
				bytes.push_back(byte);
				if (hasCompletion(bytes, token)) return true;
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
	if (hasCompletion(afterEnable, 0xC35A)) {
		std::fprintf(stderr,
			"unexpected W3DR completion before render request\n");
		shutdown();
		return 1;
	}

	// remove_actor immediately precedes render_frame, covering that payload's
	// byte boundary as well. A token-matched completion proves the full stream
	// remained aligned.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 5, 3, 0});
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 7});
	if (!waitForCompletion(0xC35A, std::chrono::seconds(3))) {
		std::fprintf(stderr,
			"no W3DR completion after set_actor/remove_actor ABI stream\n");
		shutdown();
		return 1;
	}

	// VDU 23,0,&A0,0;&4A,41,0,0; -- disable again.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 41, 0, 0, 0});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	shutdown();
	std::printf("wolf3d_smoke: ABI symbols resolved; actor set/remove payloads "
		"remained aligned through token-matched render completion\n");
	return 0;
}
