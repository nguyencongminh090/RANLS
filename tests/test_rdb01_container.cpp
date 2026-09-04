// RDB-01 regression tests: the `.rdb` container framing + compression codecs.
//
// Covers instruction "Verification" tier 3, test_rdb01_container.cpp bullets:
//   - RawCodec + DeflateCodec round-trip through writeContainer/readContainer
//   - DeflateCodec output strictly smaller than RawCodec for a ~200-node blob
//   - truncated / bad-magic / wrong container_version / packed-size-mismatch /
//     bad-crc  =>  empty optional + non-empty error, never a throw/assert
//   - random truncations of a good file never crash readContainer

#include "vendor/doctest.h"

#include "model/rdb/compressor.h"
#include "model/rdb/rdb_container.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace {

std::filesystem::path tmpFile(const std::string &name)
{
    return std::filesystem::temp_directory_path() / ("ranls-rdb01-" + name);
}

std::string readAll(const std::filesystem::path &p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

void writeAll(const std::filesystem::path &p, const std::string &bytes)
{
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// A payload with plenty of internal repetition so deflate can win.
std::string bigRepetitivePayload(int nodes)
{
    std::string s;
    for (int i = 0; i < nodes; ++i)
        s += "node[" + std::to_string(i % 32) + "]={move:[7,7],eval:0.5,depth:12};";
    return s;
}

} // namespace

TEST_CASE("RDB-01 codec: RawCodec is identity and size-checked") {
    rdb::RawCodec raw;
    const std::string in = std::string("hello\0binary\0world", 17);
    std::string       out, err;
    CHECK(raw.compress(in) == in);
    REQUIRE(raw.decompress(raw.compress(in), in.size(), &out, &err));
    CHECK(out == in);
    CHECK_FALSE(raw.decompress(in, in.size() + 1, &out, &err));
    CHECK_FALSE(err.empty());
}

TEST_CASE("RDB-01 codec: DeflateCodec round-trips and rejects corruption") {
    rdb::DeflateCodec def;
    const std::string in = bigRepetitivePayload(50);
    const std::string packed = def.compress(in);
    CHECK(packed.size() < in.size());

    std::string out, err;
    REQUIRE(def.decompress(packed, in.size(), &out, &err));
    CHECK(out == in);

    std::string bad = packed;
    bad[bad.size() / 2] ^= 0x7F;
    CHECK_FALSE(def.decompress(bad, in.size(), &out, &err));
    CHECK_FALSE(err.empty());

    // wrong raw-size hint
    CHECK_FALSE(def.decompress(packed, in.size() + 100, &out, &err));
}

TEST_CASE("RDB-01 codec: makeCompressor rejects reserved / unknown ids") {
    CHECK(rdb::makeCompressor(0) != nullptr);
    CHECK(rdb::makeCompressor(2) != nullptr);
    CHECK(rdb::makeCompressor(rdb::kCodecZstdReserved) == nullptr); // id 1
    CHECK(rdb::makeCompressor(99) == nullptr);
}

TEST_CASE("RDB-01 container: Raw + Deflate round-trip via write/read") {
    const std::string payload = bigRepetitivePayload(200);

    for (uint8_t codec : {uint8_t(0), uint8_t(2)}) {
        CAPTURE(codec);
        auto path = tmpFile("roundtrip-" + std::to_string(codec) + ".rdb");
        std::string err;
        REQUIRE_MESSAGE(rdb::writeContainer(path, codec, payload, &err), err);
        auto got = rdb::readContainer(path, &err);
        REQUIRE_MESSAGE(got.has_value(), err);
        CHECK(*got == payload);
        std::remove(path.string().c_str());
    }
}

TEST_CASE("RDB-01 container: Deflate output is strictly smaller than Raw") {
    const std::string payload = bigRepetitivePayload(200);
    auto rawPath = tmpFile("size-raw.rdb");
    auto defPath = tmpFile("size-def.rdb");
    REQUIRE(rdb::writeContainer(rawPath, 0, payload));
    REQUIRE(rdb::writeContainer(defPath, 2, payload));

    const auto rawSize = std::filesystem::file_size(rawPath);
    const auto defSize = std::filesystem::file_size(defPath);
    CHECK(defSize < rawSize);

    std::remove(rawPath.string().c_str());
    std::remove(defPath.string().c_str());
}

TEST_CASE("RDB-01 container: every corrupt case => empty optional + error") {
    const std::string payload = bigRepetitivePayload(20);
    auto good = tmpFile("corrupt-src.rdb");
    REQUIRE(rdb::writeContainer(good, 2, payload));
    const std::string base = readAll(good);
    std::remove(good.string().c_str());

    struct Case { const char *name; std::string bytes; };
    std::vector<Case> cases;

    // bad magic
    { std::string b = base; b[0] = 'X'; cases.push_back({"bad-magic", b}); }
    // wrong container_version (byte 4-5, little-endian) -> 0xFFFF
    { std::string b = base; b[4] = char(0xFF); b[5] = char(0xFF);
      cases.push_back({"bad-version", b}); }
    // unknown codec id (byte 6)
    { std::string b = base; b[6] = char(0x7E); cases.push_back({"bad-codec", b}); }
    // packed-size mismatch: drop the last 5 bytes but leave header sizes intact
    { std::string b = base; b.resize(b.size() - 5);
      cases.push_back({"packed-size-mismatch", b}); }
    // truncated mid-header
    { std::string b = base.substr(0, 10); cases.push_back({"truncated-header", b}); }
    // bad crc: flip a byte in the packed body (last byte), keep length
    { std::string b = base; b.back() ^= 0x01; cases.push_back({"bad-crc", b}); }
    // append trailing garbage -> body longer than packed_size
    { std::string b = base; b += "garbage"; cases.push_back({"trailing-garbage", b}); }

    for (const auto &c : cases) {
        CAPTURE(c.name);
        auto path = tmpFile(std::string("corrupt-") + c.name + ".rdb");
        writeAll(path, c.bytes);
        std::string err;
        auto got = rdb::readContainer(path, &err);
        CHECK_FALSE(got.has_value());
        CHECK_FALSE(err.empty());
        std::remove(path.string().c_str());
    }
}

TEST_CASE("RDB-01 container: missing file fails cleanly") {
    std::string err;
    auto got = rdb::readContainer(tmpFile("nope-does-not-exist.rdb"), &err);
    CHECK_FALSE(got.has_value());
    CHECK_FALSE(err.empty());
}

TEST_CASE("RDB-01 container: random truncations never crash readContainer") {
    const std::string payload = bigRepetitivePayload(120);
    auto good = tmpFile("fuzz-src.rdb");
    REQUIRE(rdb::writeContainer(good, 2, payload));
    const std::string base = readAll(good);
    std::remove(good.string().c_str());

    std::mt19937 rng(1234567);
    for (int i = 0; i < 400; ++i) {
        std::uniform_int_distribution<size_t> dist(0, base.size());
        std::string truncated = base.substr(0, dist(rng));
        auto path = tmpFile("fuzz.rdb");
        writeAll(path, truncated);
        std::string err;
        auto got = rdb::readContainer(path, &err); // must not throw / assert
        if (got.has_value())
            CHECK(*got == payload); // only the full file can validate
        else
            CHECK_FALSE(err.empty());
        std::remove(path.string().c_str());
    }
}

TEST_CASE("RDB-01 container: empty payload round-trips") {
    auto path = tmpFile("empty.rdb");
    std::string err;
    REQUIRE(rdb::writeContainer(path, 2, "", &err));
    auto got = rdb::readContainer(path, &err);
    REQUIRE_MESSAGE(got.has_value(), err);
    CHECK(got->empty());
    std::remove(path.string().c_str());
}
