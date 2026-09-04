# RDB-01 — `.rdb` container framing + compression interface + `GameGraph` DTO + CBOR payload

**Status:** 🔲 OPEN (Active — Sprint 11) [Model: Sonnet 5]
**Area:** new `src/model/rdb/` — `rdb_container.{h,cpp}` (magic/header framing, atomic write,
truncation detection), `compressor.{h,cpp}` (`ICompressor` + `RawCodec` + `DeflateCodec` over the
already-present `zlib`), `game_graph.h` (serialisation DTO), `game_graph_cbor.{h,cpp}`
(DTO ↔ CBOR bytes), `game_graph_convert.{h,cpp}` (`VariationTree` ↔ `GameGraph`). `tests/`.
Build: `CMakeLists.txt` + `tests/CMakeLists.txt` gain `ZLIB::ZLIB` (`find_package(ZLIB REQUIRED)`)
and the new sources on both `ranls-gui` and `ranls-gui-tests`.
**Priority:** P2
**Source:** `features/rdb-save-format/` — user-approved 2026-09-04. First of three tasks splitting
that feature (`RDB-01` container/codec/DTO, `RDB-02` Save/Open wiring, `RDB-03` end-to-end analysis
persistence). Integration branch `feat/rdb-save-format`.
**Design:** `features/rdb-save-format/planning.md` (Q1–Q8 resolved) + `diagram/container.md`
(byte table + concrete CBOR schema + load algorithm). No open questions.
**Depends on / relates to:** nothing upstream (pure model-layer, no UI). `RDB-02` and `RDB-03`
build on this. Supersedes ANLZ-03's `.yxgame`-extension approach.

## Problem

There is no format capable of round-tripping the variation tree with per-node analysis.
`.yxgame` (`src/model/game_io.cpp`) is a flat plain-text mainline-only move list and its loader
hard-rejects any non-current version. This task builds the *storage substrate* for `.rdb` —
container + compression + the serialisable graph representation — with no UI wiring yet.

## Scope (in order)

1. **`ICompressor`** (`compressor.h`): `compress(string_view) -> string`,
   `decompress(string_view, size_t rawSizeHint) -> string`, `uint8_t id()`. Impls: `RawCodec`
   (id 0, identity), `DeflateCodec` (id 2, `zlib` `compress2`/`uncompress` at a fixed level, e.g. 7).
   Id 1 is **reserved** for `zstd` — not implemented. Unknown id at read time ⇒ clean error.
2. **Container** (`rdb_container.h`): write `= "RDB1"` magic, `container_version` u16,
   `codec` u8, `flags` u8, `payload_raw_size` u64, `payload_packed_size` u64, optional `crc32` u32
   (flags bit0), then packed payload. All little-endian. `writeContainer(path, codec, payloadBytes)`
   does: pack → write to `<path>.tmp` → `fsync` → `rename` over `path`. `readContainer(path) ->
   optional<string>` (the raw payload) validates magic, version, sizes vs. file length, crc if
   present; any mismatch ⇒ `nullopt` + error string.
3. **`GameGraph` DTO** (`game_graph.h`): plain structs mirroring `diagram/container.md` —
   `GameGraph { uint16 schema; uint8 board; uint8 rule; int64 created,modified; string generator;
   vector<SetupStone> setup; vector<EngineInfo> engines; vector<GraphNode> nodes; }`,
   `GraphNode { uint32 parent; bool hasParent; optional<Move> move; optional<NodeAnalysis> analysis;
   string comment, glyph; optional<uint64> zobrist; }`, `NodeAnalysis { optional<double> winrate;
   optional<int> depth; optional<int64> nodes; string evalText; vector<Move> pv; optional<uint16>
   engineRef; optional<int64> analyzedUtc; }`. No gtkmm, no `VariationTree` include.
4. **CBOR codec** (`game_graph_cbor.{h,cpp}`): `encodeCbor(const GameGraph&) -> string` /
   `decodeCbor(string_view) -> optional<GameGraph>`. String keys per `diagram/container.md`.
   A minimal RFC 8949 subset (maps, arrays, uints, negints, text strings, float64, bool, null) is
   acceptable and preferred over a vendored dependency — keep it a self-contained pair of functions.
   Unknown map keys are skipped. `winrate` outside `[0,1]` ⇒ decoded as absent.
5. **Convert** (`game_graph_convert.{h,cpp}`): `toGameGraph(const VariationTree&, int boardSize,
   GameRule, meta) -> GameGraph` (flat DFS pre-order, `nodes[0]` = root sentinel, parent indices,
   sibling order preserved; a node whose `eval` is NaN emits **no** `analysis.winrate`) and
   `applyGameGraph(const GameGraph&, VariationTree& out, int& boardSize, GameRule& rule)` (validates
   `parent < i`, rebuilds via `addMove`, sets `eval`/`nodes`/`depth`/`comment`). This is the only
   file that includes `variation_tree.h`.
6. **Tests** (`tests/test_rdb01_container.cpp`, `tests/test_rdb01_game_graph.cpp` in
   `ranls-gui-tests`): container round-trip with `RawCodec` and `DeflateCodec`; truncated /
   bad-magic / bad-version / size-mismatch ⇒ `nullopt`; CBOR round-trip of a `GameGraph` with a
   mix of analysed and NaN nodes, branches, comments, unicode; unknown-key skip; `winrate` 1.4 ⇒
   absent on decode; `toGameGraph`∘`applyGameGraph` on a hand-built 3-branch tree with some NaN
   nodes reproduces the tree and the eval vector exactly (NaN stays NaN, never `0.5`).

## Acceptance criteria

- A `GameGraph` survives `encodeCbor` → `writeContainer` → `readContainer` → `decodeCbor` →
  identical (including NaN-as-absence and branch/sibling order).
- `DeflateCodec` output is strictly smaller than `RawCodec` for a non-trivial tree; both decode.
- Every corrupt-container case returns `nullopt` + a non-empty error, never throws, never asserts.
- No UI, no `main_window.cpp`, no `GameIO` change in this task. `.rdb` is not yet reachable from
  the app menus — that is RDB-02.
- `./build.sh` clean (only the 3 known pre-existing `-Wunused-function` warnings in
  `gomocup_protocol.cpp`); `ctest` 3/3, with the two new `ranls-gui-tests` files.

## Scope boundary

- **No UI wiring, no Save/Open, no dialog changes, no `GameIO::saveGame` removal** — RDB-02.
- **No `TreeNode` new fields** (pv/glyph/engineRef/timestamps) — the DTO carries them but the
  in-memory model is not extended until RDB-03. `toGameGraph` writes only what `TreeNode` has
  today (`move`, `eval`, `nodes`, `depth`, `comment`); `applyGameGraph` restores only those.
- No `zstd` (`libzstd-dev` is not a dependency). Codec id 1 reserved, not implemented.
- No multi-game: `GameGraph` is one game. (Container may still wrap it as a 1-element `games`
  array if that falls out naturally — not required for v1.)
- Do not touch eval→win% maths, UI-01 attribution, ANLZ-04 bridge, `buildWinGraphSeries`.
- `schema` / `container_version` are **not** `APP_VERSION` — do not touch CMake `project(VERSION)`
  or `src/version.h.in`.
