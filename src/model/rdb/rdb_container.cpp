#include "rdb_container.h"

#include "compressor.h"

#include <zlib.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#  include <io.h>
#else
#  include <unistd.h>
#endif

namespace rdb {

namespace {

constexpr std::array<char, 4> kMagic = {'R', 'D', 'B', '1'};
constexpr size_t              kHeaderNoCrc = 24;
constexpr size_t              kHeaderWithCrc = 28;
constexpr uint8_t             kFlagCrc32 = 0x01;

void putLE16(std::string &b, uint16_t v)
{
    b.push_back(static_cast<char>(v & 0xFF));
    b.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void putLE32(std::string &b, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        b.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

void putLE64(std::string &b, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        b.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

uint16_t getLE16(const unsigned char *p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t getLE32(const unsigned char *p)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v |= static_cast<uint32_t>(p[i]) << (8 * i);
    return v;
}

uint64_t getLE64(const unsigned char *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= static_cast<uint64_t>(p[i]) << (8 * i);
    return v;
}

void setError(std::string *error, std::string msg)
{
    if (error)
        *error = std::move(msg);
}

} // namespace

bool writeContainer(const std::filesystem::path &path, uint8_t codecId,
                    std::string_view payload, std::string *error)
{
    auto codec = makeCompressor(codecId);
    if (!codec) {
        setError(error, "writeContainer: unknown or unsupported codec id "
                            + std::to_string(codecId));
        return false;
    }

    std::string packed = codec->compress(payload);
    if (packed.empty() && !payload.empty()) {
        setError(error, "writeContainer: compression produced no output");
        return false;
    }

    const uint64_t rawSize    = static_cast<uint64_t>(payload.size());
    const uint64_t packedSize = static_cast<uint64_t>(packed.size());
    const uint32_t crc        = static_cast<uint32_t>(
        ::crc32(::crc32(0L, Z_NULL, 0),
                reinterpret_cast<const Bytef *>(packed.data()),
                static_cast<uInt>(packed.size())));

    std::string buf;
    buf.reserve(kHeaderWithCrc + packed.size());
    buf.append(kMagic.data(), kMagic.size());
    putLE16(buf, kContainerVersion);
    buf.push_back(static_cast<char>(codecId));
    buf.push_back(static_cast<char>(kFlagCrc32));
    putLE64(buf, rawSize);
    putLE64(buf, packedSize);
    putLE32(buf, crc);
    buf.append(packed);

    std::error_code ec;
    std::filesystem::path tmp = path;
    tmp += ".tmp";

    // Write the temp file.
    {
        FILE *f = std::fopen(tmp.string().c_str(), "wb");
        if (!f) {
            setError(error, "writeContainer: cannot open " + tmp.string()
                                + " for writing");
            return false;
        }
        size_t written = std::fwrite(buf.data(), 1, buf.size(), f);
        if (written != buf.size()) {
            std::fclose(f);
            std::filesystem::remove(tmp, ec);
            setError(error, "writeContainer: short write to " + tmp.string());
            return false;
        }
        if (std::fflush(f) != 0) {
            std::fclose(f);
            std::filesystem::remove(tmp, ec);
            setError(error, "writeContainer: fflush failed");
            return false;
        }
#if !defined(_WIN32)
        if (::fsync(::fileno(f)) != 0) {
            std::fclose(f);
            std::filesystem::remove(tmp, ec);
            setError(error, "writeContainer: fsync failed");
            return false;
        }
#endif
        std::fclose(f);
    }

    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::error_code rmec;
        std::filesystem::remove(tmp, rmec);
        setError(error, "writeContainer: rename " + tmp.string() + " -> "
                            + path.string() + " failed: " + ec.message());
        return false;
    }
    return true;
}

std::optional<std::string> readContainer(const std::filesystem::path &path,
                                         std::string                 *error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        setError(error, "readContainer: cannot open " + path.string());
        return std::nullopt;
    }
    std::string bytes((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
    if (in.bad()) {
        setError(error, "readContainer: read error on " + path.string());
        return std::nullopt;
    }

    if (bytes.size() < kHeaderNoCrc) {
        setError(error, "readContainer: file too small to hold a header");
        return std::nullopt;
    }
    const auto *p = reinterpret_cast<const unsigned char *>(bytes.data());

    if (std::memcmp(p, kMagic.data(), kMagic.size()) != 0) {
        setError(error, "readContainer: bad magic (not an .rdb container)");
        return std::nullopt;
    }

    const uint16_t version = getLE16(p + 4);
    if (version == 0 || version > kContainerVersion) {
        setError(error, "readContainer: unsupported container_version "
                            + std::to_string(version));
        return std::nullopt;
    }

    const uint8_t codecId = p[6];
    const uint8_t flags   = p[7];
    const bool    hasCrc  = (flags & kFlagCrc32) != 0;

    const size_t headerLen = hasCrc ? kHeaderWithCrc : kHeaderNoCrc;
    if (bytes.size() < headerLen) {
        setError(error, "readContainer: file too small for its declared header");
        return std::nullopt;
    }

    const uint64_t rawSize    = getLE64(p + 8);
    const uint64_t packedSize = getLE64(p + 16);

    const uint64_t bodyLen = static_cast<uint64_t>(bytes.size() - headerLen);
    if (packedSize != bodyLen) {
        setError(error, "readContainer: payload_packed_size (" + std::to_string(packedSize)
                            + ") does not match file body length ("
                            + std::to_string(bodyLen) + ") — truncated or corrupt");
        return std::nullopt;
    }

    auto codec = makeCompressor(codecId);
    if (!codec) {
        setError(error, "readContainer: unknown or unsupported codec id "
                            + std::to_string(codecId));
        return std::nullopt;
    }

    const char *packedPtr = bytes.data() + headerLen;
    std::string_view packed(packedPtr, static_cast<size_t>(packedSize));

    if (hasCrc) {
        const uint32_t stored = getLE32(p + 24);
        const uint32_t actual = static_cast<uint32_t>(
            ::crc32(::crc32(0L, Z_NULL, 0),
                    reinterpret_cast<const Bytef *>(packed.data()),
                    static_cast<uInt>(packed.size())));
        if (stored != actual) {
            setError(error, "readContainer: crc32 mismatch — payload is corrupt");
            return std::nullopt;
        }
    }

    std::string raw;
    std::string codecErr;
    if (!codec->decompress(packed, rawSize, &raw, &codecErr)) {
        setError(error, "readContainer: " + codecErr);
        return std::nullopt;
    }
    return raw;
}

} // namespace rdb
