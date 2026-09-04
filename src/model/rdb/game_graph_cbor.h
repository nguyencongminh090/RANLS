#pragma once

// RDB-01: GameGraph <-> CBOR bytes.
//
// A hand-written, deliberately tiny RFC 8949 subset — major types 0 (uint),
// 1 (negint), 2 (bytes), 3 (text), 4 (array), 5 (map), plus 0xF4/0xF5 bool,
// 0xF6 null, 0xFB float64 (0xFA float32 accepted on read only). No
// indefinite-length items, no tags, no bignums. Map keys are short text
// strings; unknown keys are fully (recursively) consumed and skipped, never
// errored — that is the whole forward-compat story.
//
// decodeCbor bounds-checks p < end before every read; a truncated blob yields
// the empty optional, never a crash or out-of-bounds read. A "winrate" outside
// [0,1] is decoded as absent (not clamped, not a parse failure).

#include "game_graph.h"

#include <optional>
#include <string>
#include <string_view>

namespace rdb {

/// Encode `g` as a single CBOR map. Never throws.
std::string encodeCbor(const GameGraph &g);

/// Decode a CBOR blob produced by encodeCbor (or a forward-compatible superset).
/// Returns the empty optional on any structural error or truncation.
std::optional<GameGraph> decodeCbor(std::string_view bytes);

} // namespace rdb
