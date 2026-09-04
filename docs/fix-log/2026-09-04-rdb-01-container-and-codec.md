# 2026-09-04 — RDB-01: `.rdb` container framing + compression interface + `GameGraph` DTO + CBOR payload

Tracked work (`docs/todo/RDB-01-rdb-container-and-codec.md`), the storage substrate for the new
`.rdb` binary save format — not a bug fix, but recorded here per CLAUDE.md ("every work item …
carries a regression test") since it ships new model-layer code with its own regression suite.
First of three tasks (RDB-01/02/03) on integration branch `feat/rdb-save-format`.

## Prompt

`/implement-task RDB-01` — build the container + compression + serialisable graph representation
for `.rdb` (CBOR + DEFLATE over the already-present zlib 1.3.1), model-layer only, no UI wiring.
Implement to `features/rdb-save-format/diagram/container.md` (byte table + CBOR schema + load
algorithm). Scope boundary: no Save/Open wiring (RDB-02), no `TreeNode` new fields (RDB-03), no
`GameIO` change, no zstd.

## Action

New directory `src/model/rdb/`:

- **`compressor.{h,cpp}`** — `ICompressor` (`compress`, `decompress(view, rawSizeHint, out, err)`,
  `id()`). `RawCodec` (id 0, identity, size-checked). `DeflateCodec` (id 2, `compress2` /
  `uncompress` at fixed level 7; empty payload handled explicitly). `makeCompressor(id)` returns
  `nullptr` for the reserved zstd id 1 and any unknown id. `kCodecZstdReserved = 1` constant only.
- **`rdb_container.{h,cpp}`** — `"RDB1"` magic + explicit little-endian header (byte-at-a-time
  put/get helpers, never a host-endian `memcpy`): `container_version` u16, `codec` u8, `flags` u8
  (bit0 = crc32 present, always set on write), `payload_raw_size` u64, `payload_packed_size` u64,
  `crc32` u32 (zlib `crc32()`), packed payload. `writeContainer` packs → writes `<path>.tmp` →
  `fflush` → `fsync(fileno)` → `std::filesystem::rename`; on any failure removes the temp and
  leaves an existing `path` untouched. `readContainer` reads the whole file, then validates magic
  / `container_version <= kContainerVersion` / known codec / `payload_packed_size == file body
  length` / crc32 **before** allocating the decode buffer; any problem ⇒ empty `std::optional` +
  non-empty error, never a throw or assert.
- **`game_graph.h`** — plain-struct DTO (`GameGraph`, `GraphNode`, `NodeAnalysis`, `Move`,
  `SetupStone`, `EngineInfo`), `<optional>/<vector>/<string>/<cstdint>` only, defaulted `==`.
  `kSchemaVersion = 1` (independent of `kContainerVersion` and `APP_VERSION`).
- **`game_graph_cbor.{h,cpp}`** — `encodeCbor` / `decodeCbor` implementing a deliberately tiny,
  total RFC 8949 subset: major types 0 (uint), 1 (negint), 2 (bytes), 3 (text), 4 (array),
  5 (map), `0xF4/0xF5` bool, `0xF6` null, `0xFB` float64 on write (also accepts `0xFA` float32 on
  read). No indefinite-length items, no tags. `readHead` bounds-checks every multi-byte length
  before consuming; every `read*` helper returns false on `p >= end` before reading. Unknown map
  keys are fully (recursively) consumed and skipped — never errored. `skipValue` is depth-capped
  (64) so a hostile deeply-nested blob cannot exhaust the stack. A `winrate` outside `[0,1]` is
  decoded as absent (not clamped, not a parse failure). `decodeCbor` requires the four mandatory
  keys (`schema`, `board`, `rule`, `nodes`) to be present.
- **`game_graph_convert.{h,cpp}`** — the ONLY unit that includes `../variation_tree.h`.
  `toGameGraph(tree, boardSize, rule, meta)` walks the tree DFS pre-order into a flat array with
  `nodes[0]` = root sentinel and every other node carrying a parent index strictly less than its
  own; sibling order preserved (first child = mainline); a node whose `eval` is NaN emits **no**
  `analysis` block (never `winrate` 0 or 0.5). `applyGameGraph(g, out, boardSize, rule, err)`
  validates `schema <= kSchemaVersion`, `board` in `[5, MAX_BOARD_SIZE]`, `rule` in `[0,2]`,
  `nodes[0]` is a sentinel, and every back-reference (`parent` present and `< i`) **before**
  touching `out`; then `out.clear()` + rebuild via `addMove`, restoring only today's `TreeNode`
  fields (`move`, `eval`, `nodes`, `depth`, `comment`).

**Build wiring:** `find_package(ZLIB REQUIRED)` in the top-level `CMakeLists.txt`; `ZLIB::ZLIB`
linked on `ranls-gui`, `ranls-gui-tests`, and `ranls-gui-ui-tests` (the last two so RDB-02/03 need
not re-touch build wiring). The four new `.cpp` files added to `GUI_SOURCES` and to both test
targets in `tests/CMakeLists.txt`.

**Not touched:** `src/ui/`, `src/command/`, `src/main_window.*`, `src/application.*`,
`src/model/game_io.{h,cpp}`, `TreeNode` fields, `GameState::evalHistory()`, eval→win% maths,
`buildWinGraphSeries`, CMake `project(VERSION)` / `src/version.h.in`. No zstd/libzstd. No UI is
able to reach `.rdb` yet — that is RDB-02.

## Verification

1. `./build.sh` — clean; no new warnings. (The 3 pre-existing `-Wunused-function` warnings in
   `src/engine/gomocup_protocol.cpp` only appear on a from-scratch compile of that unchanged
   file; the incremental build did not recompile it.)
2. `ctest --test-dir build_cmd` — 3/3 green: `ranls-gui-tests`, `rel02-version-single-source`,
   `ranls-gui-ui-tests`.
3. **New** `tests/test_rdb01_container.cpp` + `tests/test_rdb01_game_graph.cpp` (wired into
   `ranls-gui-tests`) — the RDB-01 selection runs 15 cases / 984 assertions:
   - codec: RawCodec identity + size check; DeflateCodec round-trip + rejects a bit-flipped
     stream + wrong raw-size hint; `makeCompressor` rejects id 1 and id 99.
   - container: Raw + Deflate round-trip via `writeContainer`/`readContainer`; Deflate file
     strictly smaller than Raw for a ~200-node payload; corrupt cases (bad-magic, bad-version,
     bad-codec, packed-size-mismatch, truncated-header, bad-crc, trailing-garbage, missing-file)
     each ⇒ empty optional + non-empty error; 400 random truncations of a good file never crash;
     empty payload round-trips.
   - CBOR: round-trip of a graph with analysed + NaN nodes, 3 sibling branches under one parent,
     comments incl. a UTF-8 comment; unknown-key blob (`"mystery"`, `"extra"`) decodes fine;
     hand-built `winrate` 1.4 ⇒ `analysis` present but `winrate` absent; 500 random truncations
     never crash.
   - convert: `toGameGraph`∘`applyGameGraph` on a hand-built 3-branch tree with NaN nodes
     reproduces sibling order and each node's `eval`/`depth`/`nodes`/`comment` exactly, NaN
     staying NaN; full CBOR round-trip of the produced graph is identical; `applyGameGraph`
     rejects forward-ref, self-ref, missing-parent-flag, board size 99, and schema+1 with a
     clean error and no mutation.
4. `git diff --name-only` confirms only `CMakeLists.txt`, `tests/CMakeLists.txt`, the new
   `src/model/rdb/*` and the two new `tests/test_rdb01_*.cpp` changed — nothing under `src/ui/`,
   `src/command/`, `src/main_window.*`, `src/application.*`, and `game_io.{h,cpp}` unchanged.

## Files

- `src/model/rdb/compressor.{h,cpp}` (new)
- `src/model/rdb/rdb_container.{h,cpp}` (new)
- `src/model/rdb/game_graph.h` (new)
- `src/model/rdb/game_graph_cbor.{h,cpp}` (new)
- `src/model/rdb/game_graph_convert.{h,cpp}` (new)
- `tests/test_rdb01_container.cpp`, `tests/test_rdb01_game_graph.cpp` (new)
- `CMakeLists.txt`, `tests/CMakeLists.txt`
- `docs/todo/RDB-01-rdb-container-and-codec.md`, `TODO.md`
