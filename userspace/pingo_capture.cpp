#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "../video/pingo/pingo_platform.h"

namespace {

struct CaptureConfig {
	bool enabled;
	bool valid;
	std::uint64_t frame;
	std::string prefix;
};

std::uint64_t captureFrameIndex = 0;
bool captureFinished = false;

bool parseCaptureFrame(const char *value, std::uint64_t& frame) {
	if (!value || !*value) {
		frame = 1;
		return true;
	}

	for (const char *digit = value; *digit; ++digit) {
		if (*digit < '0' || *digit > '9') {
			std::fprintf(
				stderr,
				"PINGO_CAPTURE error=invalid-frame\n");
			std::fflush(stderr);
			return false;
		}
	}

	errno = 0;
	char *end = nullptr;
	const auto parsed = std::strtoull(value, &end, 10);
	if (errno || end == value || *end != '\0' || parsed == 0) {
		std::fprintf(
			stderr,
			"PINGO_CAPTURE error=invalid-frame\n");
		std::fflush(stderr);
		return false;
	}
	frame = parsed;
	return true;
}

const CaptureConfig& captureConfig() {
	static const CaptureConfig config = [] {
		const char *prefix = std::getenv("PINGO_CAPTURE_PREFIX");
		if (!prefix || !*prefix) {
			return CaptureConfig{false, true, 1, {}};
		}
		std::uint64_t frame = 1;
		const bool valid = parseCaptureFrame(
			std::getenv("PINGO_CAPTURE_FRAME"), frame);
		return CaptureConfig{
			true,
			valid,
			frame,
			prefix,
		};
	}();
	return config;
}

std::uint32_t crc32(const std::uint8_t *data, std::size_t size) {
	std::uint32_t crc = 0xFFFFFFFFU;
	for (std::size_t i = 0; i < size; ++i) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; ++bit) {
			const std::uint32_t mask =
				0U - static_cast<std::uint32_t>(crc & 1U);
			crc = (crc >> 1U) ^ (0xEDB88320U & mask);
		}
	}
	return ~crc;
}

bool pathExists(const std::string& path) {
	struct stat info {};
	if (::lstat(path.c_str(), &info) == 0) {
		return true;
	}
	if (errno == ENOENT) {
		return false;
	}
	std::perror(path.c_str());
	return true;
}

FILE *openExclusiveOutput(const std::string& path, bool& owned) {
	owned = false;
	int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
	const int descriptor = ::open(path.c_str(), flags, 0666);
	if (descriptor < 0) {
		std::perror(path.c_str());
		return nullptr;
	}

	FILE *file = ::fdopen(descriptor, "wb");
	if (!file) {
		const int error = errno;
		::close(descriptor);
		::unlink(path.c_str());
		errno = error;
		std::perror(path.c_str());
		return nullptr;
	}
	owned = true;
	return file;
}

bool closeOutput(FILE *file, const std::string& path, bool ok) {
	if (std::fclose(file) != 0) {
		std::perror(path.c_str());
		return false;
	}
	return ok;
}

bool writeRgba2222(
	const std::string& path,
	const std::uint8_t *pixels,
	std::size_t size,
	bool& owned) {
	FILE *file = openExclusiveOutput(path, owned);
	if (!file) {
		return false;
	}
	const bool ok = std::fwrite(pixels, 1, size, file) == size;
	if (!ok) {
		std::fprintf(stderr, "%s: short write\n", path.c_str());
	}
	return closeOutput(file, path, ok);
}

bool writePpm(
	const std::string& path,
	const std::uint8_t *pixels,
	std::uint16_t width,
	std::uint16_t height,
	bool& owned) {
	FILE *file = openExclusiveOutput(path, owned);
	if (!file) {
		return false;
	}

	bool ok =
		std::fprintf(file, "P6\n%u %u\n255\n", width, height) > 0;
	std::vector<std::uint8_t> row(static_cast<std::size_t>(width) * 3U);
	for (std::uint16_t y = 0; ok && y < height; ++y) {
		for (std::uint16_t x = 0; x < width; ++x) {
			const std::uint8_t pixel =
				pixels[static_cast<std::size_t>(y) * width + x];
			row[static_cast<std::size_t>(x) * 3U] =
				static_cast<std::uint8_t>((pixel & 0x03U) * 85U);
			row[static_cast<std::size_t>(x) * 3U + 1U] =
				static_cast<std::uint8_t>(
					((pixel >> 2U) & 0x03U) * 85U);
			row[static_cast<std::size_t>(x) * 3U + 2U] =
				static_cast<std::uint8_t>(
					((pixel >> 4U) & 0x03U) * 85U);
		}
		ok =
			std::fwrite(row.data(), 1, row.size(), file) == row.size();
	}
	if (!ok) {
		std::fprintf(stderr, "%s: short write\n", path.c_str());
	}
	return closeOutput(file, path, ok);
}

bool writeMetadata(
	const std::string& path,
	std::uint64_t frame,
	std::uint16_t width,
	std::uint16_t height,
	std::size_t size,
	std::uint32_t checksum,
	bool& owned) {
	FILE *file = openExclusiveOutput(path, owned);
	if (!file) {
		return false;
	}
	const bool ok = std::fprintf(
		file,
		"scope=pingo-render-target\n"
		"format=RGBA2222\n"
		"layout=AABBGGRR\n"
		"origin=top-left\n"
		"row_order=top-to-bottom\n"
		"stride_bytes=%u\n"
		"frame=%llu\n"
		"width=%u\n"
		"height=%u\n"
		"bytes=%zu\n"
		"crc32_variant=CRC-32/ISO-HDLC\n"
		"crc32=%08X\n",
		width,
		static_cast<unsigned long long>(frame),
		width,
		height,
		size,
		checksum) > 0;
	if (!ok) {
		std::fprintf(stderr, "%s: short write\n", path.c_str());
	}
	return closeOutput(file, path, ok);
}

bool publish(
	const std::string& temporary,
	const std::string& final,
	bool& temporaryOwned) {
	// A hard link publishes the closed file atomically without replacing an
	// entry created by another capture process.
	if (::link(temporary.c_str(), final.c_str()) != 0) {
		std::perror(final.c_str());
		return false;
	}
	if (::unlink(temporary.c_str()) != 0) {
		std::perror(temporary.c_str());
		return false;
	}
	temporaryOwned = false;
	return true;
}

void removeTemporary(const std::string& path, bool& owned) {
	if (owned) {
		::unlink(path.c_str());
		owned = false;
	}
}

} // namespace

extern "C" void pingo_platform_rgba2222_frame_ready(
	const std::uint8_t *pixels,
	std::uint16_t width,
	std::uint16_t height) {
	const auto& config = captureConfig();
	if (!config.enabled || !config.valid || captureFinished) {
		return;
	}

	++captureFrameIndex;
	if (captureFrameIndex != config.frame) {
		return;
	}
	captureFinished = true;

	if (!pixels || width == 0 || height == 0) {
		std::fprintf(
			stderr,
			"PINGO_CAPTURE error=invalid-frame frame=%llu width=%u "
			"height=%u\n",
			static_cast<unsigned long long>(captureFrameIndex),
			width,
			height);
		std::fflush(stderr);
		return;
	}

	const std::string rgbaPath = config.prefix + ".rgba2";
	const std::string ppmPath = config.prefix + ".ppm";
	const std::string metadataPath = config.prefix + ".txt";
	const std::string temporarySuffix =
		".tmp." + std::to_string(static_cast<long long>(::getpid()));
	const std::string rgbaTemporary = rgbaPath + temporarySuffix;
	const std::string ppmTemporary = ppmPath + temporarySuffix;
	const std::string metadataTemporary =
		metadataPath + temporarySuffix;

	if (pathExists(rgbaPath) || pathExists(ppmPath) ||
		pathExists(metadataPath)) {
		std::fprintf(
			stderr,
			"PINGO_CAPTURE error=output-unavailable\n");
		std::fflush(stderr);
		return;
	}

	const std::size_t size =
		static_cast<std::size_t>(width) * height;
	const std::uint32_t checksum = crc32(pixels, size);

	bool rgbaOwned = false;
	bool ppmOwned = false;
	bool metadataOwned = false;
	bool ok = writeRgba2222(
		rgbaTemporary, pixels, size, rgbaOwned);
	ok = ok && writePpm(
		ppmTemporary, pixels, width, height, ppmOwned);
	ok = ok && writeMetadata(
		metadataTemporary,
		captureFrameIndex,
		width,
		height,
		size,
		checksum,
		metadataOwned);

	if (ok) {
		// Metadata is deliberately last: its presence marks a complete set.
		ok = publish(ppmTemporary, ppmPath, ppmOwned);
		ok = ok && publish(rgbaTemporary, rgbaPath, rgbaOwned);
		ok = ok && publish(
			metadataTemporary, metadataPath, metadataOwned);
	}

	if (!ok) {
		removeTemporary(rgbaTemporary, rgbaOwned);
		removeTemporary(ppmTemporary, ppmOwned);
		removeTemporary(metadataTemporary, metadataOwned);
		std::fprintf(
			stderr,
			"PINGO_CAPTURE error=write-failed\n");
		std::fflush(stderr);
		return;
	}

	std::fprintf(
		stderr,
		"PINGO_CAPTURE frame=%llu format=RGBA2222 width=%u height=%u "
		"bytes=%zu crc32=%08X\n",
		static_cast<unsigned long long>(captureFrameIndex),
		width,
		height,
		size,
		checksum);
	std::fflush(stderr);
}
