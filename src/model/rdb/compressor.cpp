#include "compressor.h"

#include <zlib.h>

#include <limits>

namespace rdb {

// ── RawCodec ────────────────────────────────────────────────────────────────
std::string RawCodec::compress(std::string_view input) const
{
    return std::string(input);
}

bool RawCodec::decompress(std::string_view packed, uint64_t rawSizeHint,
                          std::string *out, std::string *error) const
{
    if (packed.size() != rawSizeHint) {
        if (error)
            *error = "raw codec: packed size does not match raw-size hint";
        return false;
    }
    if (out)
        out->assign(packed.data(), packed.size());
    return true;
}

// ── DeflateCodec ────────────────────────────────────────────────────────────
std::string DeflateCodec::compress(std::string_view input) const
{
    if (input.empty())
        return std::string();
    uLongf bound = ::compressBound(static_cast<uLong>(input.size()));
    std::string out;
    out.resize(bound);
    uLongf destLen = bound;
    int rc = ::compress2(reinterpret_cast<Bytef *>(out.data()), &destLen,
                         reinterpret_cast<const Bytef *>(input.data()),
                         static_cast<uLong>(input.size()), kLevel);
    if (rc != Z_OK) {
        // compress2 only fails on OOM / bad level here; return input uncompressed
        // is not acceptable (codec id says deflate) — return empty, callers treat
        // an empty deflate of non-empty input as an error at write time.
        return std::string();
    }
    out.resize(destLen);
    return out;
}

bool DeflateCodec::decompress(std::string_view packed, uint64_t rawSizeHint,
                              std::string *out, std::string *error) const
{
    if (rawSizeHint == 0) {
        if (!packed.empty()) {
            if (error)
                *error = "deflate codec: non-empty payload for zero raw-size hint";
            return false;
        }
        if (out)
            out->clear();
        return true;
    }
    if (rawSizeHint > std::numeric_limits<uLongf>::max()) {
        if (error)
            *error = "deflate codec: raw-size hint too large";
        return false;
    }
    std::string buf;
    buf.resize(static_cast<size_t>(rawSizeHint));
    uLongf destLen = static_cast<uLongf>(rawSizeHint);
    int rc = ::uncompress(reinterpret_cast<Bytef *>(buf.data()), &destLen,
                          reinterpret_cast<const Bytef *>(packed.data()),
                          static_cast<uLong>(packed.size()));
    if (rc != Z_OK) {
        if (error)
            *error = "deflate codec: inflate failed (corrupt or truncated stream)";
        return false;
    }
    if (destLen != rawSizeHint) {
        if (error)
            *error = "deflate codec: inflated size does not match raw-size hint";
        return false;
    }
    if (out)
        *out = std::move(buf);
    return true;
}

// ── factory ─────────────────────────────────────────────────────────────────
std::unique_ptr<ICompressor> makeCompressor(uint8_t codecId)
{
    switch (codecId) {
    case 0:  return std::make_unique<RawCodec>();
    case 2:  return std::make_unique<DeflateCodec>();
    default: return nullptr; // includes id 1 (zstd, reserved, unimplemented)
    }
}

} // namespace rdb
