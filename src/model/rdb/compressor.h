#pragma once

// RDB-01: pluggable payload compression for the `.rdb` container.
//
// The container stores a one-byte `codec` id; the reader picks the matching
// ICompressor to inflate the packed payload. Deliberately gtkmm/glibmm-free so
// it stays in the model-only test target.
//
//   id 0  RawCodec      identity (store)
//   id 1  (reserved)    zstd — NOT implemented in this repo, no libzstd dependency
//   id 2  DeflateCodec  zlib deflate/inflate, fixed level 7
//
// Out of scope (RDB-02/03): streaming, dictionaries, per-file level tuning.

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace rdb {

/// Reserved codec id for a future zstd implementation. Never emitted or
/// accepted by this build — a container carrying it fails to read cleanly.
inline constexpr uint8_t kCodecZstdReserved = 1;

class ICompressor {
public:
    virtual ~ICompressor() = default;

    /// Compress `input`. Returns the packed bytes. Never throws.
    virtual std::string compress(std::string_view input) const = 0;

    /// Decompress `packed`, where `rawSizeHint` is the exact expected size of
    /// the decompressed output (from the container header). Returns false on
    /// any failure (corrupt stream, size mismatch) and sets `*error`. Never
    /// throws.
    virtual bool decompress(std::string_view packed,
                            uint64_t          rawSizeHint,
                            std::string      *out,
                            std::string      *error) const = 0;

    /// The one-byte codec id written into the container header.
    virtual uint8_t id() const = 0;
};

/// id 0 — identity. compress/decompress are memcpy.
class RawCodec final : public ICompressor {
public:
    std::string compress(std::string_view input) const override;
    bool        decompress(std::string_view packed, uint64_t rawSizeHint,
                           std::string *out, std::string *error) const override;
    uint8_t     id() const override { return 0; }
};

/// id 2 — zlib deflate at a fixed level.
class DeflateCodec final : public ICompressor {
public:
    static constexpr int kLevel = 7;

    std::string compress(std::string_view input) const override;
    bool        decompress(std::string_view packed, uint64_t rawSizeHint,
                           std::string *out, std::string *error) const override;
    uint8_t     id() const override { return 2; }
};

/// Returns a compressor for `codecId`, or nullptr if the id is unknown or
/// reserved-but-unimplemented (id 1).
std::unique_ptr<ICompressor> makeCompressor(uint8_t codecId);

} // namespace rdb
