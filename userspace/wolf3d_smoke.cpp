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
	auto hasCompletion = [](const std::vector<std::uint8_t>& bytes) {
		const std::uint8_t prefix[] = {0x81, 10, 'W', '3', 'D', 'R'};
		for (std::size_t i = 0; i + sizeof(prefix) <= bytes.size(); ++i) {
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

	// VDU 23,0,&A0,0;&4A,0 -- hello-world dispatch smoke test.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 0});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// VDU 23,0,&A0,0;&4A,41,1,0x5A,0xC3; -- enable render-done notify.
	// Nothing triggers a render yet, so no W3DR packet is expected here;
	// this only proves the subcommand doesn't crash or desync the stream.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 41, 1, 0x5A, 0xC3});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	auto afterEnable = receiveBytes(std::chrono::milliseconds(10));
	if (hasCompletion(afterEnable)) {
		std::fprintf(stderr,
			"unexpected W3DR completion with no render pipeline\n");
		shutdown();
		return 1;
	}

	// VDU 23,0,&A0,0;&4A,41,0,0; -- disable again.
	sendBytes({23, 0, 0xA0, 0, 0, 0x4A, 41, 0, 0, 0});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	shutdown();
	std::printf("wolf3d_smoke: ABI symbols resolved, dispatch did not "
		"crash or desync (hello + notify register/unregister)\n");
	return 0;
}
