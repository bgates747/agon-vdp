#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <initializer_list>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::uint8_t kPingoOpcode = 0x49;
constexpr std::uint8_t kWolfOpcode = 0x4A;

template<typename T>
T loadSymbol(void * handle, const char * name) {
	dlerror();
	auto symbol = reinterpret_cast<T>(dlsym(handle, name));
	if (const char * error = dlerror()) {
		std::fprintf(stderr, "missing ABI symbol %s: %s\n", name, error);
		std::exit(2);
	}
	return symbol;
}

struct Completion {
	std::array<char, 4> magic;
	std::uint8_t version;
	std::uint8_t event;
	std::uint16_t token;
	std::uint16_t sequence;
};

struct ExpectedCompletion {
	const char * magic;
	std::uint8_t event;
	std::uint16_t token;
	std::uint16_t sequence;
};

class Harness {
public:
	explicit Harness(const char * library)
		: m_handle(dlopen(library, RTLD_NOW | RTLD_LOCAL)),
		  m_nextEcho(0x40) {
		if (!m_handle) {
			std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
			std::exit(2);
		}

		m_send = loadSymbol<void (*)(std::uint8_t)>(
			m_handle, "z80_send_to_vdp");
		m_receive = loadSymbol<bool (*)(std::uint8_t *)>(
			m_handle, "z80_recv_from_vdp");
		m_isCts = loadSymbol<bool (*)()>(m_handle, "z80_uart0_is_cts");
		m_setup = loadSymbol<void (*)()>(m_handle, "vdp_setup");
		m_shutdown = loadSymbol<void (*)()>(m_handle, "vdp_shutdown");
		m_setDebug = loadSymbol<void (*)(bool)>(
			m_handle, "setVdpDebugLogging");
		m_pingoControlExists = loadSymbol<bool (*)(std::uint16_t)>(
			m_handle, "pingo_userspace_control_exists");
		m_pingoOwnedAllocations = loadSymbol<std::uint32_t (*)()>(
			m_handle, "pingo_userspace_get_owned_allocation_count");
		m_failPingoAllocationAfter = loadSymbol<void (*)(std::int32_t)>(
			m_handle, "pingo_userspace_fail_allocation_after");
		m_wolfControlExists = loadSymbol<bool (*)(std::uint16_t)>(
			m_handle, "wolf3d_userspace_control_exists");
		m_wolfControlCount = loadSymbol<std::uint32_t (*)()>(
			m_handle, "wolf3d_userspace_control_count");
		m_wolfGetTile = loadSymbol<std::int32_t (*)(std::uint16_t, int, int)>(
			m_handle, "wolf3d_userspace_get_tile");
		m_bufferBlockCount = loadSymbol<std::uint32_t (*)(std::uint16_t)>(
			m_handle, "buffer_userspace_block_count");
		m_bufferBlockSize = loadSymbol<std::int32_t (*)(std::uint16_t, std::uint32_t)>(
			m_handle, "buffer_userspace_block_size");
		m_bufferBlockByte = loadSymbol<std::int32_t (*)(
			std::uint16_t, std::uint32_t, std::uint32_t)>(
			m_handle, "buffer_userspace_block_byte");
	}

	~Harness() {
		if (m_handle) {
			dlclose(m_handle);
		}
	}

	void setup() {
		m_setDebug(false);
		m_setup();
		drain();
		auto bytes = barrier();
		expectCompletions(bytes, {});
	}

	void shutdown() {
		if (m_handle) {
			m_shutdown();
			dlclose(m_handle);
			m_handle = nullptr;
		}
	}

	void sendBytes(const std::vector<std::uint8_t>& bytes) {
		for (auto byte : bytes) {
			while (!m_isCts()) {
				std::this_thread::sleep_for(
					std::chrono::microseconds(50));
			}
			m_send(byte);
		}
	}

	void sendBytes(std::initializer_list<std::uint8_t> bytes) {
		sendBytes(std::vector<std::uint8_t>(bytes));
	}

	static void appendWord(
			std::vector<std::uint8_t>& bytes, std::uint16_t value) {
		bytes.push_back(static_cast<std::uint8_t>(value));
		bytes.push_back(static_cast<std::uint8_t>(value >> 8));
	}

	static void appendLong(
			std::vector<std::uint8_t>& bytes, std::uint32_t value) {
		appendWord(bytes, static_cast<std::uint16_t>(value));
		appendWord(bytes, static_cast<std::uint16_t>(value >> 16));
	}

	void sendPingo(
			std::uint16_t control, std::uint8_t subcommand,
			const std::vector<std::uint16_t>& words = {}) {
		auto bytes = extensionPrefix(control, kPingoOpcode, subcommand);
		for (auto word : words) {
			appendWord(bytes, word);
		}
		sendBytes(bytes);
	}

	void setPingoNotification(
			std::uint16_t control, std::uint16_t token) {
		auto bytes = extensionPrefix(control, kPingoOpcode, 41);
		bytes.push_back(1);
		appendWord(bytes, token);
		sendBytes(bytes);
	}

	void sendWolf(
			std::uint16_t control, std::uint8_t subcommand,
			const std::vector<std::uint8_t>& payload = {}) {
		auto bytes = extensionPrefix(control, kWolfOpcode, subcommand);
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		sendBytes(bytes);
	}

	void initializeWolf(
			std::uint16_t control, std::uint16_t tilemap) {
		std::vector<std::uint8_t> payload;
		appendWord(payload, tilemap);
		sendWolf(control, 1, payload);

		payload.clear();
		appendLong(payload, 0x00028000); // x = 2.5 tiles
		appendLong(payload, 0x00048000); // y = 4.5 tiles
		appendWord(payload, 0);          // face +X
		sendWolf(control, 2, payload);

		payload.clear();
		appendWord(payload, 4);          // actor slot
		appendWord(payload, 60);         // base shape
		appendLong(payload, 0x00058000); // x = 5.5 tiles
		appendLong(payload, 0x00048000); // y = 4.5 tiles
		appendWord(payload, 180);
		payload.push_back(0);            // fixed sprite
		sendWolf(control, 4, payload);

		payload.clear();
		appendWord(payload, 61);         // first-person weapon shape
		sendWolf(control, 8, payload);
	}

	void setWolfNotification(
			std::uint16_t control, std::uint16_t token) {
		std::vector<std::uint8_t> payload = {1};
		appendWord(payload, token);
		sendWolf(control, 41, payload);
	}

	void writeBuffer(
			std::uint16_t buffer, const std::vector<std::uint8_t>& payload) {
		auto bytes = bufferedPrefix(buffer, 0);
		appendWord(bytes, static_cast<std::uint16_t>(payload.size()));
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		sendBytes(bytes);
	}

	void clearBuffer(std::uint16_t buffer) {
		sendBytes(bufferedPrefix(buffer, 2));
	}

	void debugBuffer(std::uint16_t buffer) {
		sendBytes(bufferedPrefix(buffer, 0x80));
	}

	void addCallback(std::uint16_t buffer, std::uint16_t type) {
		auto bytes = bufferedPrefix(buffer, 0x50);
		appendWord(bytes, type);
		sendBytes(bytes);
	}

	void removeCallback(std::uint16_t buffer, std::uint16_t type) {
		auto bytes = bufferedPrefix(buffer, 0x51);
		appendWord(bytes, type);
		sendBytes(bytes);
	}

	void writeClearCallback(
			std::uint16_t callbackBuffer, std::uint16_t targetBuffer) {
		writeBuffer(callbackBuffer, bufferedPrefix(targetBuffer, 2));
	}

	void createBitmap2222(
			std::uint16_t bitmap, std::uint16_t width,
			std::uint16_t height, std::uint8_t color) {
		sendBytes({
			23, 27, 0x20,
			static_cast<std::uint8_t>(bitmap),
			static_cast<std::uint8_t>(bitmap >> 8),
		});
		sendBytes({
			23, 27, 0x22,
			static_cast<std::uint8_t>(width),
			static_cast<std::uint8_t>(width >> 8),
			static_cast<std::uint8_t>(height),
			static_cast<std::uint8_t>(height >> 8),
			color,
		});
	}

	std::vector<std::uint8_t> beginAndBarrier(
			std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
		return barrier(timeout);
	}

	void beginAction() {
		drain();
	}

	bool pingoExists(std::uint16_t id) const {
		return m_pingoControlExists(id);
	}

	bool wolfExists(std::uint16_t id) const {
		return m_wolfControlExists(id);
	}

	std::uint32_t pingoOwnedAllocations() const {
		return m_pingoOwnedAllocations();
	}

	void failPingoAllocationAfter(std::int32_t successfulAllocations) {
		m_failPingoAllocationAfter(successfulAllocations);
	}

	std::uint32_t wolfControlCount() const {
		return m_wolfControlCount();
	}

	std::int32_t wolfGetTile(
			std::uint16_t control, int tilex, int tiley) const {
		return m_wolfGetTile(control, tilex, tiley);
	}

	std::uint32_t bufferBlockCount(std::uint16_t buffer) const {
		return m_bufferBlockCount(buffer);
	}

	std::int32_t bufferBlockSize(
			std::uint16_t buffer, std::uint32_t block) const {
		return m_bufferBlockSize(buffer, block);
	}

	std::int32_t bufferBlockByte(
			std::uint16_t buffer, std::uint32_t block,
			std::uint32_t offset) const {
		return m_bufferBlockByte(buffer, block, offset);
	}

	void expectCompletions(
			const std::vector<std::uint8_t>& bytes,
			std::initializer_list<ExpectedCompletion> expected) {
		auto actual = parseCompletions(bytes);
		if (actual.size() != expected.size()) {
			std::fprintf(
				stderr, "expected %zu completion packet(s), received %zu\n",
				expected.size(), actual.size());
			dumpCompletions(actual);
			fail("completion packet count mismatch");
		}

		std::size_t index = 0;
		for (const auto& item : expected) {
			const auto& packet = actual[index++];
			if (std::memcmp(packet.magic.data(), item.magic, 4) != 0 ||
					packet.version != 1 || packet.event != item.event ||
					packet.token != item.token ||
					packet.sequence != item.sequence) {
				std::fprintf(
					stderr,
					"completion mismatch at index %zu: expected %.4s "
					"event=%u token=%04x seq=%u\n",
					index - 1, item.magic, item.event, item.token,
					item.sequence);
				dumpCompletions(actual);
				fail("completion packet contents mismatch");
			}
		}
	}

	[[noreturn]] void fail(const char * message) {
		std::fprintf(stderr, "%s\n", message);
		shutdown();
		std::exit(1);
	}

private:
	static std::vector<std::uint8_t> bufferedPrefix(
			std::uint16_t buffer, std::uint8_t command) {
		return {
			23, 0, 0xA0,
			static_cast<std::uint8_t>(buffer),
			static_cast<std::uint8_t>(buffer >> 8),
			command,
		};
	}

	static std::vector<std::uint8_t> extensionPrefix(
			std::uint16_t control, std::uint8_t opcode,
			std::uint8_t subcommand) {
		auto bytes = bufferedPrefix(control, opcode);
		bytes.push_back(subcommand);
		return bytes;
	}

	void drain() {
		std::uint8_t byte;
		while (m_receive(&byte)) {
		}
	}

	std::vector<std::uint8_t> barrier(
			std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
		const auto echo = m_nextEcho++;
		const std::uint8_t expected[] = {0x80, 1, echo};
		sendBytes({23, 0, 0x80, echo});

		std::vector<std::uint8_t> received;
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (std::chrono::steady_clock::now() < deadline) {
			std::uint8_t byte;
			if (!m_receive(&byte)) {
				std::this_thread::sleep_for(
					std::chrono::microseconds(100));
				continue;
			}
			received.push_back(byte);
			if (received.size() < sizeof(expected)) {
				continue;
			}
			auto start = received.size() - sizeof(expected);
			if (std::memcmp(received.data() + start, expected,
					sizeof(expected)) == 0) {
				return received;
			}
		}
		fail("timed out waiting for general-poll barrier");
	}

	static std::vector<Completion> parseCompletions(
			const std::vector<std::uint8_t>& bytes) {
		std::vector<Completion> packets;
		for (std::size_t i = 0; i + 12 <= bytes.size(); ++i) {
			if (bytes[i] != 0x81 || bytes[i + 1] != 10) {
				continue;
			}
			const bool pingo = std::memcmp(
				bytes.data() + i + 2, "P3DR", 4) == 0;
			const bool wolf = std::memcmp(
				bytes.data() + i + 2, "W3DR", 4) == 0;
			if (!pingo && !wolf) {
				continue;
			}
			Completion packet {};
			std::memcpy(packet.magic.data(), bytes.data() + i + 2, 4);
			packet.version = bytes[i + 6];
			packet.event = bytes[i + 7];
			packet.token = static_cast<std::uint16_t>(
				bytes[i + 8] | (bytes[i + 9] << 8));
			packet.sequence = static_cast<std::uint16_t>(
				bytes[i + 10] | (bytes[i + 11] << 8));
			packets.push_back(packet);
			i += 11;
		}
		return packets;
	}

	static void dumpCompletions(const std::vector<Completion>& packets) {
		for (std::size_t i = 0; i < packets.size(); ++i) {
			const auto& packet = packets[i];
			std::fprintf(
				stderr, "  [%zu] %.4s version=%u event=%u token=%04x seq=%u\n",
				i, packet.magic.data(), packet.version, packet.event,
				packet.token, packet.sequence);
		}
	}

	void * m_handle;
	void (*m_send)(std::uint8_t);
	bool (*m_receive)(std::uint8_t *);
	bool (*m_isCts)();
	void (*m_setup)();
	void (*m_shutdown)();
	void (*m_setDebug)(bool);
	bool (*m_pingoControlExists)(std::uint16_t);
	std::uint32_t (*m_pingoOwnedAllocations)();
	void (*m_failPingoAllocationAfter)(std::int32_t);
	bool (*m_wolfControlExists)(std::uint16_t);
	std::uint32_t (*m_wolfControlCount)();
	std::int32_t (*m_wolfGetTile)(std::uint16_t, int, int);
	std::uint32_t (*m_bufferBlockCount)(std::uint16_t);
	std::int32_t (*m_bufferBlockSize)(std::uint16_t, std::uint32_t);
	std::int32_t (*m_bufferBlockByte)(
		std::uint16_t, std::uint32_t, std::uint32_t);
	std::uint8_t m_nextEcho;
};

void require(Harness& harness, bool condition, const char * message) {
	if (!condition) {
		harness.fail(message);
	}
}

void expectNoneAfter(Harness& harness) {
	harness.expectCompletions(harness.beginAndBarrier(), {});
}

void createWolfAssets(Harness& harness, std::uint16_t tilemap) {
	constexpr std::size_t mapSize = 64;
	std::vector<std::uint8_t> map(mapSize * mapSize, 0);
	for (std::size_t coordinate = 0; coordinate < mapSize; ++coordinate) {
		// Texture 2 is intentionally absent: it preserves the renderer's solid
		// outer-ring invariant without paying for 256 staged wall columns.
		map[coordinate] = 2;
		map[(mapSize - 1) * mapSize + coordinate] = 2;
		map[coordinate * mapSize] = 2;
		map[coordinate * mapSize + mapSize - 1] = 2;
	}
	// One in-view wall tile is sufficient to force the former 0xFFFE path.
	map[4 * mapSize + 10] = 1;
	harness.writeBuffer(tilemap, map);
	harness.createBitmap2222(0x1001, 64, 64, 0xF3); // wall texture 1
	harness.createBitmap2222(0x203C, 64, 64, 0xFC); // actor shape 60
	harness.createBitmap2222(0x203D, 64, 64, 0xCF); // weapon shape 61
}

void testDistinctIdsAndFormerScratchIsolation(
		Harness& harness, std::uint32_t baseline) {
	std::puts("cross-test: preparing populated distinct-ID fixture");
	constexpr std::uint16_t pingoTarget = 0x0601;
	constexpr std::uint16_t tilemap = 0x0602;
	constexpr std::uint16_t wolfControl = 0x0700;
	constexpr std::array<std::uint16_t, 3> pingoControls = {
		0xFFFC, 0xFFFD, 0xFFFE,
	};
	constexpr std::array<std::uint16_t, 3> pingoTokens = {
		0xC101, 0xC102, 0xC103,
	};
	constexpr std::uint16_t wolfToken = 0xD201;

	harness.beginAction();
	harness.createBitmap2222(pingoTarget, 32, 32, 0);
	createWolfAssets(harness, tilemap);
	for (std::size_t i = 0; i < pingoControls.size(); ++i) {
		harness.sendPingo(pingoControls[i], 0, {32, 32});
		harness.setPingoNotification(pingoControls[i], pingoTokens[i]);
	}
	harness.initializeWolf(wolfControl, tilemap);
	harness.setWolfNotification(wolfControl, wolfToken);
	expectNoneAfter(harness);

	for (auto control : pingoControls) {
		require(harness, harness.pingoExists(control),
			"failed to initialize former-scratch-ID Pingo control");
	}
	require(harness, harness.wolfExists(wolfControl),
		"failed to initialize distinct Wolf control");

	// This populated render forces wall, actor-sprite, and view-weapon paths.
	// Their staging must remain private and leave all three historical scratch
	// IDs, now valid application-selected Pingo controls, untouched.
	harness.beginAction();
	harness.sendWolf(wolfControl, 7);
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"W3DR", 1, wolfToken, 1}});
	std::puts("cross-test: populated Wolf render completed");
	for (auto control : pingoControls) {
		require(harness, harness.pingoExists(control),
			"Wolf render destroyed a former-scratch-ID Pingo control");
	}

	for (std::size_t i = 0; i < pingoControls.size(); ++i) {
		harness.beginAction();
		harness.sendPingo(pingoControls[i], 38, {pingoTarget});
		harness.expectCompletions(
			harness.beginAndBarrier(),
			{{"P3DR", 1, pingoTokens[i], 0}});
	}

	// The one populated render above has exercised all three historical
	// scratch paths. Remove its art before the remaining lifecycle renders so
	// this correctness harness does not spend seconds staging every column.
	harness.beginAction();
	harness.clearBuffer(tilemap);
	harness.clearBuffer(0x1001);
	harness.clearBuffer(0x203C);
	harness.clearBuffer(0x203D);
	expectNoneAfter(harness);

	harness.beginAction();
	harness.clearBuffer(pingoControls[0]);
	expectNoneAfter(harness);
	require(harness, !harness.pingoExists(pingoControls[0]),
		"targeted Pingo clear retained its control");
	require(harness, harness.pingoExists(pingoControls[1]) &&
		harness.pingoExists(pingoControls[2]),
		"targeted Pingo clear damaged a distinct Pingo control");
	require(harness, harness.wolfExists(wolfControl),
		"targeted Pingo clear damaged a distinct Wolf control");

	harness.beginAction();
	harness.sendWolf(wolfControl, 7);
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"W3DR", 1, wolfToken, 2}});

	harness.beginAction();
	harness.clearBuffer(wolfControl);
	expectNoneAfter(harness);
	require(harness, !harness.wolfExists(wolfControl),
		"targeted clear retained a Wolf-only control");
	require(harness, harness.pingoExists(pingoControls[1]) &&
		harness.pingoExists(pingoControls[2]),
		"targeted Wolf clear damaged a distinct Pingo control");

	for (std::size_t i = 1; i < pingoControls.size(); ++i) {
		harness.beginAction();
		harness.sendPingo(pingoControls[i], 38, {pingoTarget});
		harness.expectCompletions(
			harness.beginAndBarrier(),
			{{"P3DR", 1, pingoTokens[i], 1}});
	}

	require(harness, harness.pingoOwnedAllocations() > baseline,
		"live Pingo controls were not reflected in owned-allocation accounting");
	std::puts("cross-test: distinct-ID and private-scratch checks completed");
}

void testWolfTilemapTypeIsolation(Harness& harness) {
	std::puts("cross-test: checking Wolf tilemap/Pingo type isolation");
	constexpr std::uint16_t tilemap = 0x0717;
	constexpr std::uint16_t wolfControl = 0x0718;
	constexpr int tilex = 2;
	constexpr int tiley = 3;
	constexpr std::uint8_t ordinaryTile = 7;
	constexpr std::uint8_t safeWallTile = 1;

	std::vector<std::uint8_t> map(64 * 64, 0);
	map[tiley * 64 + tilex] = ordinaryTile;

	harness.beginAction();
	harness.writeBuffer(tilemap, map);
	std::vector<std::uint8_t> payload;
	Harness::appendWord(payload, tilemap);
	harness.sendWolf(wolfControl, 1, payload);
	expectNoneAfter(harness);
	require(harness, harness.wolfGetTile(wolfControl, tilex, tiley) == ordinaryTile,
		"Wolf could not read its ordinary tilemap buffer");

	// Retype only the referenced tilemap ID as Pingo. The Wolf control remains
	// live, but its tile lookup must fail closed instead of reading bytes from
	// the in-place Pingo3dControl object.
	harness.beginAction();
	harness.clearBuffer(tilemap);
	harness.sendPingo(tilemap, 0, {32, 32});
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(tilemap),
		"Pingo did not replace the referenced ordinary tilemap");
	require(harness, harness.wolfExists(wolfControl),
		"tilemap retyping unexpectedly destroyed the Wolf control");
	require(harness, harness.wolfGetTile(wolfControl, tilex, tiley) == safeWallTile,
		"Wolf interpreted a live Pingo control as tilemap bytes");

	// The guard is deliberately checked at use time. Replacing the same ID
	// with ordinary map bytes must make it usable without reinitializing Wolf.
	harness.beginAction();
	harness.clearBuffer(tilemap);
	harness.writeBuffer(tilemap, map);
	expectNoneAfter(harness);
	require(harness, !harness.pingoExists(tilemap),
		"ordinary tilemap replacement retained Pingo state");
	require(harness, harness.wolfGetTile(wolfControl, tilex, tiley) == ordinaryTile,
		"Wolf did not recover after ordinary tilemap recreation");

	harness.beginAction();
	harness.clearBuffer(wolfControl);
	harness.clearBuffer(tilemap);
	expectNoneAfter(harness);
	require(harness, !harness.wolfExists(wolfControl),
		"tilemap-isolation test retained its Wolf control");
}

void testRepeatedWolfCreateClearLifecycle(Harness& harness) {
	std::puts("cross-test: checking repeated Wolf create/clear lifecycle");
	constexpr std::uint16_t pingoControl = 0x0720;
	constexpr std::uint16_t ordinaryTilemap = 0x0721;
	constexpr std::uint16_t wolfControl = 0x0722;
	constexpr std::size_t cycleCount = 128;
	constexpr int tilex = 2;
	constexpr int tiley = 3;
	constexpr std::uint8_t markerTile = 7;

	const auto allocationsBeforeSetup = harness.pingoOwnedAllocations();
	std::vector<std::uint8_t> map(64 * 64, 0);
	map[tiley * 64 + tilex] = markerTile;

	harness.beginAction();
	harness.sendPingo(pingoControl, 0, {8, 8});
	harness.writeBuffer(ordinaryTilemap, map);
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(pingoControl),
		"failed to establish the lifecycle test's stable Pingo control");
	require(harness, harness.wolfControlCount() == 0,
		"Wolf registry was not empty before repeated lifecycle test");
	const auto stablePingoAllocations = harness.pingoOwnedAllocations();
	require(harness, stablePingoAllocations > allocationsBeforeSetup,
		"stable Pingo control was absent from owned-allocation accounting");

	std::vector<std::uint8_t> initializePayload;
	Harness::appendWord(initializePayload, ordinaryTilemap);
	for (std::size_t cycle = 0; cycle < cycleCount; ++cycle) {
		harness.beginAction();
		harness.sendWolf(wolfControl, 1, initializePayload);
		expectNoneAfter(harness);
		require(harness, harness.wolfExists(wolfControl) &&
			harness.wolfControlCount() == 1,
			"repeated Wolf create did not publish exactly one control");
		require(harness,
			harness.wolfGetTile(wolfControl, tilex, tiley) == markerTile,
			"repeated Wolf create lost or collided with ordinary state");
		require(harness, harness.pingoExists(pingoControl) &&
			harness.pingoOwnedAllocations() == stablePingoAllocations,
			"repeated Wolf create collided with stable Pingo state");

		harness.beginAction();
		harness.clearBuffer(wolfControl);
		expectNoneAfter(harness);
		require(harness, !harness.wolfExists(wolfControl) &&
			harness.wolfControlCount() == 0,
			"repeated Wolf clear retained registry/control state");
		require(harness, harness.pingoExists(pingoControl) &&
			harness.pingoOwnedAllocations() == stablePingoAllocations,
			"repeated Wolf clear damaged stable Pingo state");
	}

	// This is a deterministic native lifecycle regression. It demonstrates
	// prompt registry teardown and isolation, not real embedded PSRAM limits.
	harness.beginAction();
	harness.clearBuffer(pingoControl);
	harness.clearBuffer(ordinaryTilemap);
	expectNoneAfter(harness);
	require(harness, !harness.pingoExists(pingoControl) &&
		harness.pingoOwnedAllocations() == allocationsBeforeSetup,
		"repeated lifecycle fixture leaked its Pingo-owned allocations");
	require(harness, harness.wolfControlCount() == 0,
		"repeated lifecycle fixture left persistent Wolf registry state");
	std::puts("cross-test: repeated Wolf create/clear lifecycle completed");
}

void testTransactionalPingoCreation(Harness& harness) {
	std::puts("cross-test: checking transactional Pingo creation");
	constexpr std::uint16_t ordinary = 0x0730;
	constexpr std::uint16_t wolf = 0x0731;
	constexpr std::uint16_t tilemap = 0x0732;
	constexpr std::uint16_t pingoTarget = 0x0733;
	constexpr std::uint16_t pingoToken = 0xC501;
	const auto baselineAllocations = harness.pingoOwnedAllocations();
	const auto baselineWolfControls = harness.wolfControlCount();

	// Invalid Pingo dimensions must consume their complete payload while
	// preserving an ordinary occupant's exact multi-block layout.
	harness.beginAction();
	harness.writeBuffer(ordinary, {0x11, 0x22});
	harness.writeBuffer(ordinary, {0x33});
	expectNoneAfter(harness);
	require(harness, harness.bufferBlockCount(ordinary) == 2 &&
		harness.bufferBlockSize(ordinary, 0) == 2 &&
		harness.bufferBlockSize(ordinary, 1) == 1 &&
		harness.bufferBlockByte(ordinary, 0, 0) == 0x11 &&
		harness.bufferBlockByte(ordinary, 0, 1) == 0x22 &&
		harness.bufferBlockByte(ordinary, 1, 0) == 0x33,
		"failed to establish transactional ordinary-buffer fixture");

	harness.beginAction();
	harness.sendPingo(ordinary, 0, {0, 16});
	expectNoneAfter(harness);
	require(harness, !harness.pingoExists(ordinary) &&
		!harness.wolfExists(ordinary) &&
		harness.bufferBlockCount(ordinary) == 2 &&
		harness.bufferBlockSize(ordinary, 0) == 2 &&
		harness.bufferBlockSize(ordinary, 1) == 1 &&
		harness.bufferBlockByte(ordinary, 0, 0) == 0x11 &&
		harness.bufferBlockByte(ordinary, 0, 1) == 0x22 &&
		harness.bufferBlockByte(ordinary, 1, 0) == 0x33,
		"invalid Pingo create damaged ordinary buffer contents/layout");
	require(harness,
		harness.pingoOwnedAllocations() == baselineAllocations,
		"invalid Pingo create leaked owned allocations over ordinary state");

	// Exercise the deeper failure path after private object construction. The
	// deterministic allocator hook rejects initialize()'s first owned resource;
	// the staged object must unwind without disturbing the ordinary occupant.
	harness.beginAction();
	harness.failPingoAllocationAfter(0);
	harness.sendPingo(ordinary, 0, {16, 16});
	expectNoneAfter(harness);
	harness.failPingoAllocationAfter(-1);
	require(harness, !harness.pingoExists(ordinary) &&
		harness.bufferBlockCount(ordinary) == 2 &&
		harness.bufferBlockSize(ordinary, 0) == 2 &&
		harness.bufferBlockSize(ordinary, 1) == 1 &&
		harness.bufferBlockByte(ordinary, 0, 0) == 0x11 &&
		harness.bufferBlockByte(ordinary, 0, 1) == 0x22 &&
		harness.bufferBlockByte(ordinary, 1, 0) == 0x33 &&
		harness.pingoOwnedAllocations() == baselineAllocations,
		"allocation-failed Pingo create damaged ordinary buffer state");

	// A valid create must replace that ordinary buffer directly, without a
	// caller-issued clear, and publish exactly one Pingo backing block.
	harness.beginAction();
	harness.createBitmap2222(pingoTarget, 16, 16, 0);
	harness.sendPingo(ordinary, 0, {16, 16});
	harness.setPingoNotification(ordinary, pingoToken);
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(ordinary) &&
		!harness.wolfExists(ordinary) &&
		harness.bufferBlockCount(ordinary) == 1 &&
		harness.bufferBlockSize(ordinary, 0) > 3,
		"direct ordinary-to-Pingo replacement did not publish one control block");
	const auto livePingoAllocations = harness.pingoOwnedAllocations();
	require(harness, livePingoAllocations > baselineAllocations,
		"direct ordinary-to-Pingo replacement published no owned resources");

	harness.beginAction();
	harness.sendPingo(ordinary, 38, {pingoTarget});
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"P3DR", 1, pingoToken, 0}});

	// A rejected replacement over an existing Pingo must preserve not only its
	// allocation count but live state (notification token and render sequence).
	harness.beginAction();
	harness.sendPingo(ordinary, 0, {16, 0});
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(ordinary) &&
		harness.bufferBlockCount(ordinary) == 1 &&
		harness.pingoOwnedAllocations() == livePingoAllocations,
		"invalid Pingo recreation damaged an existing Pingo control");
	harness.beginAction();
	harness.sendPingo(ordinary, 38, {pingoTarget});
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"P3DR", 1, pingoToken, 1}});

	harness.beginAction();
	harness.clearBuffer(ordinary);
	expectNoneAfter(harness);
	require(harness,
		harness.pingoOwnedAllocations() == baselineAllocations,
		"transactional ordinary/Pingo checks leaked owned allocations");

	// The same fail-closed rule applies to a Wolf-only occupant. Dimensions
	// outside Pingo's signed bitmap range must leave the live Wolf state intact;
	// a later valid Pingo create then replaces it atomically.
	std::vector<std::uint8_t> map(64 * 64, 0);
	map[3 * 64 + 2] = 7;
	harness.beginAction();
	harness.writeBuffer(tilemap, map);
	harness.initializeWolf(wolf, tilemap);
	expectNoneAfter(harness);
	require(harness, harness.wolfExists(wolf) &&
		harness.wolfControlCount() == baselineWolfControls + 1 &&
		harness.wolfGetTile(wolf, 2, 3) == 7,
		"failed to establish transactional Wolf fixture");

	harness.beginAction();
	harness.sendPingo(wolf, 0, {0x8000, 16});
	expectNoneAfter(harness);
	require(harness, harness.wolfExists(wolf) &&
		!harness.pingoExists(wolf) &&
		harness.wolfControlCount() == baselineWolfControls + 1 &&
		harness.wolfGetTile(wolf, 2, 3) == 7,
		"invalid Pingo create damaged a live Wolf occupant");
	require(harness,
		harness.pingoOwnedAllocations() == baselineAllocations,
		"invalid Pingo create over Wolf leaked owned allocations");

	harness.beginAction();
	harness.failPingoAllocationAfter(0);
	harness.sendPingo(wolf, 0, {16, 16});
	expectNoneAfter(harness);
	harness.failPingoAllocationAfter(-1);
	require(harness, harness.wolfExists(wolf) &&
		!harness.pingoExists(wolf) &&
		harness.wolfControlCount() == baselineWolfControls + 1 &&
		harness.wolfGetTile(wolf, 2, 3) == 7 &&
		harness.pingoOwnedAllocations() == baselineAllocations,
		"allocation-failed Pingo create damaged a live Wolf occupant");

	harness.beginAction();
	harness.sendPingo(wolf, 0, {16, 16});
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(wolf) &&
		!harness.wolfExists(wolf) &&
		harness.wolfControlCount() == baselineWolfControls &&
		harness.bufferBlockCount(wolf) == 1 &&
		harness.bufferBlockSize(wolf, 0) > 3,
		"successful Wolf-to-Pingo replacement was not atomic");

	harness.beginAction();
	harness.clearBuffer(wolf);
	harness.clearBuffer(tilemap);
	harness.clearBuffer(pingoTarget);
	expectNoneAfter(harness);
	require(harness,
		harness.pingoOwnedAllocations() == baselineAllocations &&
		harness.wolfControlCount() == baselineWolfControls,
		"transactional Pingo fixture leaked typed state");
	std::puts("cross-test: transactional Pingo creation completed");
}

void testExclusiveReplacementRecreateAndClearAll(
		Harness& harness, std::uint32_t baseline) {
	std::puts("cross-test: checking exclusive replacement and lifecycle");
	const auto retainedPingoAllocations = harness.pingoOwnedAllocations();
	require(harness, retainedPingoAllocations > baseline,
		"distinct-ID setup did not retain its expected Pingo state");
	constexpr std::uint16_t shared = 0x0710;
	constexpr std::uint16_t unknown = 0x0711;
	constexpr std::uint16_t truncated = 0x0712;
	constexpr std::uint16_t clearAllWolf = 0x0713;
	constexpr std::uint16_t pingoTarget = 0x0601;
	constexpr std::uint16_t tilemap = 0x0602;
	constexpr std::uint16_t callbackScript = 0x0715;
	constexpr std::uint16_t callbackTarget = 0x0716;
	constexpr std::uint16_t ordinaryProbe = 0x0719;

	// General-poll completion executes this deliberately missing callback. Its
	// canonical failure path unregisters itself while callbacks are being
	// dispatched, so the dispatcher must iterate a stable snapshot.
	harness.beginAction();
	harness.addCallback(0x0714, 0x0180);
	expectNoneAfter(harness);
	expectNoneAfter(harness);

	// Pingo first owns the global ID. The first valid Wolf command replaces it.
	harness.beginAction();
	harness.sendPingo(shared, 0, {32, 32});
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(shared) && !harness.wolfExists(shared),
		"Pingo did not exclusively claim a fresh control ID");

	harness.beginAction();
	harness.initializeWolf(shared, tilemap);
	harness.setWolfNotification(shared, 0xD301);
	expectNoneAfter(harness);
	require(harness, !harness.pingoExists(shared) && harness.wolfExists(shared),
		"first valid Wolf command did not replace same-ID Pingo state");
	require(harness,
		harness.pingoOwnedAllocations() == retainedPingoAllocations,
		"same-ID Wolf replacement leaked the replaced Pingo control");

	harness.beginAction();
	harness.sendWolf(shared, 7);
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"W3DR", 1, 0xD301, 1}});

	// Read-only debug inspection must not materialize an empty ordinary buffer
	// alongside the Wolf-only control. Such a ghost occupant would prevent the
	// reciprocal Pingo replacement below from claiming this global ID.
	harness.beginAction();
	harness.debugBuffer(shared);
	expectNoneAfter(harness);
	require(harness, harness.wolfExists(shared),
		"debug inspection damaged the Wolf-only control");

	// Reinitializing Pingo reciprocally replaces Wolf and resets Pingo state.
	harness.beginAction();
	harness.sendPingo(shared, 0, {32, 32});
	harness.setPingoNotification(shared, 0xC301);
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(shared) && !harness.wolfExists(shared),
		"Pingo recreation did not replace same-ID Wolf state");

	harness.beginAction();
	harness.sendPingo(shared, 38, {pingoTarget});
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"P3DR", 1, 0xC301, 0}});

	// An ordinary write is the third exclusive occupant type and must release
	// typed state before storing its bytes. A valid Wolf command then replaces
	// that ordinary buffer in turn.
	harness.beginAction();
	harness.writeBuffer(shared, {0xA5});
	expectNoneAfter(harness);
	require(harness, !harness.pingoExists(shared) && !harness.wolfExists(shared),
		"ordinary write did not release same-ID typed state");
	require(harness,
		harness.pingoOwnedAllocations() == retainedPingoAllocations,
		"ordinary replacement leaked Pingo-owned allocations");
	require(harness, harness.bufferBlockCount(shared) == 1 &&
		harness.bufferBlockSize(shared, 0) == 1 &&
		harness.bufferBlockByte(shared, 0, 0) == 0xA5,
		"ordinary replacement did not remove Pingo's raw backing block");

	// Once the ID is ordinary again, the canonical write operation must retain
	// its documented multi-block append behavior.
	harness.beginAction();
	harness.writeBuffer(shared, {0x5A, 0xC3});
	expectNoneAfter(harness);
	require(harness, harness.bufferBlockCount(shared) == 2 &&
		harness.bufferBlockSize(shared, 0) == 1 &&
		harness.bufferBlockSize(shared, 1) == 2 &&
		harness.bufferBlockByte(shared, 0, 0) == 0xA5 &&
		harness.bufferBlockByte(shared, 1, 0) == 0x5A &&
		harness.bufferBlockByte(shared, 1, 1) == 0xC3,
		"typed replacement fix broke ordinary multi-block append semantics");

	// Wolf's userspace tile accessor gives us a production-path read of the
	// replacement buffer. This must see the caller's first byte, not the stale
	// in-place Pingo3dControl block that formerly occupied this ID.
	harness.beginAction();
	harness.initializeWolf(ordinaryProbe, shared);
	expectNoneAfter(harness);
	require(harness, harness.wolfGetTile(ordinaryProbe, 0, 0) == 0xA5,
		"ordinary replacement retained a stale Pingo backing block");
	harness.beginAction();
	harness.clearBuffer(ordinaryProbe);
	expectNoneAfter(harness);

	harness.beginAction();
	harness.initializeWolf(shared, tilemap);
	expectNoneAfter(harness);
	require(harness, harness.wolfExists(shared),
		"Wolf did not replace a same-ID ordinary buffer");

	// Generic clear must erase Wolf state even though Wolf has no ordinary
	// backing block. An unknown opcode-local subcommand must not recreate it.
	harness.beginAction();
	harness.clearBuffer(shared);
	expectNoneAfter(harness);
	require(harness, !harness.wolfExists(shared),
		"generic targeted clear retained Wolf state");
	require(harness, harness.wolfControlCount() == 0,
		"unexpected Wolf control survived targeted clear");

	harness.beginAction();
	harness.sendWolf(unknown, 0xFE);
	expectNoneAfter(harness);
	require(harness, !harness.wolfExists(unknown) &&
		harness.wolfControlCount() == 0,
		"unknown Wolf subcommand allocated control state");

	// A recognized command does not own the ID until its required payload has
	// been accepted. Let the deliberately missing tilemap word time out before
	// sending the barrier, so the poll bytes cannot satisfy the truncated read.
	harness.beginAction();
	harness.sendWolf(truncated, 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	expectNoneAfter(harness);
	require(harness, !harness.wolfExists(truncated) &&
		harness.wolfControlCount() == 0,
		"truncated known Wolf command published control state");

	// Recreate Wolf after clear. Its default callback must be disabled and its
	// render sequence reset. The silent render is sequence 1; after opting in,
	// the next exact completion must therefore be sequence 2.
	harness.beginAction();
	harness.initializeWolf(shared, tilemap);
	harness.sendWolf(shared, 7);
	expectNoneAfter(harness);

	harness.beginAction();
	harness.setWolfNotification(shared, 0xD302);
	harness.sendWolf(shared, 7);
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"W3DR", 1, 0xD302, 2}});

	// Recreate Pingo over Wolf. Its independently disabled callback and
	// zero-based sequence must likewise reset, with one silent render followed
	// by the callback-enabled sequence 1.
	harness.beginAction();
	harness.sendPingo(shared, 0, {32, 32});
	harness.sendPingo(shared, 38, {pingoTarget});
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(shared) && !harness.wolfExists(shared),
		"Pingo did not replace recreated Wolf state");

	harness.beginAction();
	harness.setPingoNotification(shared, 0xC302);
	harness.sendPingo(shared, 38, {pingoTarget});
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"P3DR", 1, 0xC302, 1}});

	// Completion packets synchronously execute CALLBACK_SENT_VDPP handlers.
	// Clear the active Pingo control from such a handler: notification must be
	// the final member action, including in diagnostic firmware, or this is a
	// use-after-free when control returns from send_packet().
	const auto beforeCallbackControl = harness.pingoOwnedAllocations();
	harness.beginAction();
	harness.writeClearCallback(callbackScript, callbackTarget);
	harness.addCallback(callbackScript, 0x0181);
	harness.sendPingo(callbackTarget, 0, {32, 32});
	harness.setPingoNotification(callbackTarget, 0xC401);
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(callbackTarget),
		"failed to establish callback-teardown Pingo control");

	harness.beginAction();
	harness.sendPingo(callbackTarget, 38, {pingoTarget});
	harness.expectCompletions(
		harness.beginAndBarrier(), {{"P3DR", 1, 0xC401, 0}});
	require(harness, !harness.pingoExists(callbackTarget),
		"completion callback did not clear its active Pingo control");
	require(harness,
		harness.pingoOwnedAllocations() == beforeCallbackControl,
		"completion-callback teardown leaked Pingo-owned allocations");

	harness.beginAction();
	harness.removeCallback(callbackScript, 0x0181);
	harness.clearBuffer(callbackScript);
	expectNoneAfter(harness);

	// End with both state types live on distinct IDs, then prove global clear
	// erases both registries and every Pingo-owned allocation.
	harness.beginAction();
	harness.initializeWolf(clearAllWolf, tilemap);
	expectNoneAfter(harness);
	require(harness, harness.pingoExists(shared) &&
		harness.wolfExists(clearAllWolf),
		"failed to establish both typed states before clear-all");

	harness.beginAction();
	harness.clearBuffer(0xFFFF);
	expectNoneAfter(harness);
	require(harness, !harness.pingoExists(shared) &&
		!harness.wolfExists(clearAllWolf) &&
		harness.wolfControlCount() == 0,
		"global clear retained typed control state");
	require(harness, harness.pingoOwnedAllocations() == baseline,
		"global clear leaked Pingo-owned allocations");
}

} // namespace

int main(int argc, char ** argv) {
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	if (argc != 2) {
		std::fprintf(stderr, "usage: %s VDP_SO\n", argv[0]);
		return 2;
	}

	Harness harness(argv[1]);
	std::puts("cross-test: initializing combined native VDP");
	harness.setup();
	std::puts("cross-test: native VDP initialized");
	const auto baseline = harness.pingoOwnedAllocations();

	testDistinctIdsAndFormerScratchIsolation(harness, baseline);
	testWolfTilemapTypeIsolation(harness);
	testRepeatedWolfCreateClearLifecycle(harness);
	testTransactionalPingoCreation(harness);
	testExclusiveReplacementRecreateAndClearAll(harness, baseline);

	harness.shutdown();
	std::puts(
		"combined_subsystem_test: exclusive ID replacement, private Wolf "
		"scratch, typed tilemap isolation, exact P3DR/W3DR isolation, "
		"transactional Pingo replacement, 128-cycle Wolf lifecycle, "
		"callback-safe teardown, read-only "
		"inspection, targeted clear, "
		"unknown-command rejection, recreation, and clear-all passed");
	return 0;
}
