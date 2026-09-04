#pragma once

// RDB-01: the `.rdb` binary container — magic + framing header + packed payload.
//
// Byte layout (all integers little-endian, written/read one byte at a time —
// never memcpy a host-endian integer):
//
//   off  size  field
//   0    4     magic "RDB1"            (0x52 0x44 0x42 0x31)
//   4    2     container_version u16   framing revisions only
//   6    1     codec u8               0 none / 1 zstd (reserved) / 2 deflate
//   7    1     flags u8               bit0 = crc32 present
//   8    8     payload_raw_size u64    decompressed size (decode buffer hint)
//   16   8     payload_packed_size u64 bytes of packed payload that follow
//   24   4     crc32 u32              of the packed payload; iff flags.bit0
//   24|28 …    packed payload
//
// write(): pack -> <path>.tmp -> fsync -> std::filesystem::rename over path.
// read():  validate magic / version / sizes-vs-file-length / crc BEFORE
//          allocating the decode buffer; any problem => empty optional + error.
//          Never throws, never asserts.
//
// gtkmm/glibmm-free — lives in the model-only test target.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace rdb {

/// Highest container framing version this build can read. Independent of
/// APP_VERSION and of the CBOR payload `schema`.
inline constexpr uint16_t kContainerVersion = 1;

/// Write `payload` into `path` as an `.rdb` container using codec `codecId`
/// (0 = raw, 2 = deflate). Atomic: writes `<path>.tmp`, fsyncs, renames over
/// `path`. On any failure returns false, sets `*error`, and leaves any existing
/// `path` untouched. crc32 of the packed payload is always written (flags bit0).
bool writeContainer(const std::filesystem::path &path,
                    uint8_t                      codecId,
                    std::string_view             payload,
                    std::string                 *error = nullptr);

/// Read and validate the container at `path`, returning the decompressed
/// payload. Returns the empty optional and sets `*error` (non-empty) on a
/// missing/unreadable file, bad magic, unknown container_version, unknown
/// codec, size mismatch, or crc mismatch. Never throws.
std::optional<std::string> readContainer(const std::filesystem::path &path,
                                         std::string                 *error = nullptr);

} // namespace rdb
