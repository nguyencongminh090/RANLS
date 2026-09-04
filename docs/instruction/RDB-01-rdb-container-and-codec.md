# Instruction — RDB-01: `.rdb` container + `ICompressor` + `GameGraph` + CBOR

## Approach

Pure model-layer, no gtkmm, no UI. Everything lands in a new `src/model/rdb/` directory and is
exercised only by `ranls-gui-tests` (which links `PkgConfig::SIGCXX` + `PkgConfig::GIOMM` only —
adding `ZLIB::ZLIB` is fine, `zlib` needs no display and is already on the host, `zlib 1.3.1`).

Read `features/rdb-save-format/diagram/container.md` first — it has the exact byte table, the CBOR
schema with string keys, and the load algorithm. Implement to that.

Layering, bottom-up (each testable before the next):

1. `ICompressor` + `RawCodec` + `DeflateCodec`. `DeflateCodec` wraps `zlib` `compress2()` /
   `uncompress()` — pass the raw-size hint straight to `uncompress`'s `destLen`. Fixed level (7).
2. `RdbContainer::write` / `read` — framing + atomic write (`<path>.tmp`, `fsync` the FILE*, then
   `std::filesystem::rename`). `read` validates before allocating: magic, `container_version <=
   kContainerVersion`, `payload_packed_size` == (file size − header), optional crc32.
3. `GameGraph` DTO — dumb structs, `<optional>` / `<vector>` / `<string>` / `<cstdint>` only.
4. `encodeCbor` / `decodeCbor` — a hand-written RFC 8949 subset (see Pitfalls). String keys.
5. `toGameGraph` / `applyGameGraph` — the only place `variation_tree.h` is included.

`GUI_SOURCES` in the top-level `CMakeLists.txt` and both test targets in `tests/CMakeLists.txt`
get the new `.cpp` files; `find_package(ZLIB REQUIRED)` + `ZLIB::ZLIB` on `ranls-gui`,
`ranls-gui-tests`, and `ranls-gui-ui-tests` (the latter two so RDB-02/03 don't have to touch build
wiring again).

## Pitfalls

- **CBOR by hand: keep the subset tiny and total.** Only major types 0 (uint), 1 (negint),
  2 (bytes — probably unused), 3 (text), 4 (array), 5 (map), plus `0xF4/0xF5` false/true,
  `0xF6` null, `0xFB` float64. No indefinite-length items, no tags, no float16/float32 on write
  (accept float32 `0xFA` on read for tolerance). Every decode function takes `(const uint8_t*&
  p, const uint8_t* end)` and returns false on `p >= end` *before* every read — a truncated blob
  must never read past `end`. Fuzz-ish test: feed random truncations of a good blob, assert
  `decodeCbor` returns `nullopt`, never crashes.
- **Unknown map keys must be skipped, not errored** — that is the whole forward-compat story.
  `decodeCbor` reads a key string; if it is not one it knows, it must still fully consume the
  value (recursively) and continue.
- **NaN is absence.** `toGameGraph`: `if (std::isnan(node.eval)) → no analysis block`. Never write
  `winrate: 0` or `0.5`. `applyGameGraph`: missing block ⇒ leave `eval` at
  `std::numeric_limits<double>::quiet_NaN()`. Round-trip test asserts `std::isnan` on both ends.
- **`winrate` range on decode.** `decodeCbor` (or `applyGameGraph`) drops a `winrate` outside
  `[0,1]` — treat as absent, do not clamp, do not fail the whole parse.
- **Parent index safety.** `applyGameGraph` must reject `parent >= i` (forward/self reference) and
  `parent` out of range with a clean error — this is the DAG-free tree invariant.
- **Endianness.** Container integer fields are explicit little-endian byte writes/reads — do not
  `memcpy` a `uint64_t` (host-endian). CBOR numbers are big-endian per the spec — the hand codec
  writes them MSB-first.
- **Atomic write.** If `rename` fails, report it and leave the original file untouched; never
  half-write `path` itself.
- **`crc32`** — `zlib` provides `crc32()`; use it. Keep `flags` bit0 set on write so RDB-02/03 and
  future readers get corruption detection for free.

## Verification before marking this task done

1. `./build.sh` — clean, only the 3 known `-Wunused-function` warnings in `gomocup_protocol.cpp`.
2. `ctest` — `ranls-gui-tests`, `ranls-gui-ui-tests`, `rel02-version-single-source` all green.
3. **New `ranls-gui-tests` cases**, all required:
   - `test_rdb01_container.cpp` — Raw + Deflate round-trip; Deflate output < Raw for a ~200-node
     tree; truncated / bad-magic / wrong `container_version` / packed-size-mismatch / bad-crc ⇒
     `nullopt` + non-empty error; random truncations never crash.
   - `test_rdb01_game_graph.cpp` — CBOR round-trip of a `GameGraph` with analysed + NaN nodes,
     ≥3 branches, unicode comment; unknown-key blob decodes fine; `winrate` 1.4 ⇒ absent;
     `toGameGraph`∘`applyGameGraph` on a hand-built tree reproduces structure + `evalHistory`-style
     eval vector exactly (NaN stays NaN).
4. Confirm by grep that no file under `src/ui/`, `src/command/`, `src/main_window.*`,
   `src/application.*` was touched, and `game_io.{h,cpp}` is unchanged.

Tiers 3–4 are required, not just 1–2.

## Boundaries — do not touch

- `src/main_window.cpp`, anything in `src/ui/` or `src/command/`, `src/application.*`.
- `src/model/game_io.{h,cpp}` — RDB-02 retires `saveGame`, not this task.
- `src/model/variation_tree.h` `TreeNode` fields — RDB-03 extends the struct. `toGameGraph` here
  serialises only today's fields (`move`, `eval`, `nodes`, `depth`, `comment`).
- `GameState::evalHistory()` and its `depth>0 || nodes>0` gate — RDB-03.
- eval→win% maths, UI-01 attribution, ANLZ-04 bridge, `buildWinGraphSeries`.
- CMake `project(VERSION)`, `src/version.h.in`. The `.rdb` `schema` / `container_version` are
  independent constants in the new headers.
- No `zstd` / `libzstd` dependency. Codec id 1 is a reserved constant only.
