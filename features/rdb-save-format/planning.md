# `.rdb` — planning (research + open questions)

See [user_story.md](user_story.md) · [diagram/container.md](diagram/container.md).

The user asked for research on three axes: (1) tree/graph structure, (2) data
compression, (3) writing/reading compressed binary. Findings below, then the open
questions that must be resolved with the user before this becomes tracked work.

---

## Research 1 — graph structure

### What we have

`src/model/variation_tree.h`: `TreeNode { Coord move; double eval; int64 nodes;
int depth; string comment; vector<unique_ptr<TreeNode>> children; TreeNode*
parent; }`. Sentinel root, children keyed/looked-up by `move`. Strictly a **tree**
(one parent per node). Ply count strictly increases (stones only added) ⇒ no
cycles are ever possible.

### Options considered

| Option | Transpositions | Editing (promote/delete/comment) | Precedent | Verdict |
|---|---|---|---|---|
| **A. Tree (arborescence)** — keep as-is | duplicated as separate subtrees | unambiguous, well-understood | SGF, Sabaki, Lizzie, KaTrain, ChessBase | simple, matches model |
| B. DAG keyed by Zobrist position | merged (one node per position) | ambiguous — a comment belongs to a *path*, delete is unclear | lichess opening explorer (position-keyed), engine TTs | powerful for a *database*, wrong shape for "a game with annotations" |
| **C. Tree on disk + `zobrist` per node** | reader may build an in-memory transposition index; on-disk stays a tree | unambiguous (still a tree) | — (pragmatic hybrid) | **recommended** |

**Recommendation: C.** Serialise the tree structure (paths, comments, mainline =
first child — all unambiguous), but store an optional `zobrist: u64` on each node.
A reader that wants DAG behaviour (future opening explorer, "reached before?"
markers) builds a `hash → node[]` map in memory. A future schema revision can
switch the *canonical* representation to a node-list + edge-list DAG without a
container break, because nodes are already addressed by stable integer `id`.

### On-disk node schema (maps 1:1 onto `TreeNode` + headroom)

```
Graph {
  schema_version : u16
  board_size     : u8
  rule           : u8            // GameRule enum
  setup_stones   : [{x,y,color}] // pre-placed stones for handicap/setup positions; usually empty
  created_utc    : i64           // unix seconds
  modified_utc   : i64
  generator      : string        // e.g. "RANLS 0.2.0"
  engines        : [{ id:u16, name:string, version:string, params:string }]
  nodes          : [Node]        // flat; node[0] = sentinel root
}
Node {
  id         : u32               // stable, array index is fine
  parent     : u32               // 0 = root's own sentinel; root has parent=0,id=0
  move       : {x:i16, y:i16}?   // null on the root
  side       : u8?               // derivable from ply parity; stored for robustness
  analysis   : Analysis?         // absent ⇒ node never evaluated (NaN)
  comment    : string?
  label      : string?           // "!", "?", "TR", … annotation glyphs (future)
  zobrist    : u64?              // transposition hint (future reader-side DAG)
  child_order: u32               // position among siblings; 0 = mainline
}
Analysis {
  winrate     : f32?             // absent ⇒ not evaluated; validated to [0,1] on load, out-of-range ⇒ absent
  depth       : i16?
  nodes       : i64?
  eval_text   : string?          // "+M5", "0.53", …
  pv          : [{x,y}]?
  engine_ref  : u16?             // → Graph.engines[].id
  analyzed_utc: i64?
}
```

All fields after the first few are **optional** — that is the "open structure,
allow future update" guarantee. `winrate == absent` is how NaN round-trips.

---

## Research 2 — compression

### Data characteristics

Coords are small ints; PV strings and structure are highly repetitive; a single
analysed game is ~KBs–low MBs uncompressed, a future multi-game DB tens of MBs.
Ratio matters more than speed at these sizes (any codec is "instant" here).

| Codec | Ratio (structured data) | Speed | Dependency | Notes |
|---|---|---|---|---|
| none | 1× | — | — | keep as a valid codec id for tests / tiny files |
| DEFLATE (zlib) | ~3–5× | fast | **`zlib` 1.3.1 already present** (pkg-config `zlib`) | gzip/raw; ubiquitous; zero new dep |
| DEFLATE (miniz) | ~3–5× | fast | single public-domain .c/.h vendored | zero *system* dep; identical format to zlib |
| zstd | ~4–7× | very fast | `libzstd` — **NOT installed on this host**, needs `apt install libzstd-dev` + `find_package` | modern default, level 1–22, dictionary support |
| brotli / xz | ~5–9× | slow-ish | libbrotli / liblzma — not installed | best ratio, heavier; overkill for now |

**Recommendation: DEFLATE via the already-present `zlib`** for v1 (zero new
dependency — `zlib` 1.3.1 is on the host; `libzstd` and `sqlite3` are not). The
container's `codec` byte reserves ids for `zstd` (1) and others so upgrading later
is a new impl behind the interface, not a format break. If the user wants zstd's
ratio now, it is one `apt` package + `pkg_check_modules(ZSTD libzstd)`.

### Pluggability (user requirement 4)

```
class ICompressor {
  virtual std::string compress(std::string_view raw) = 0;
  virtual std::string decompress(std::string_view packed, size_t rawSizeHint) = 0;
  virtual uint8_t id() const = 0;          // written into the container header
};
// impls: RawCodec(0), DeflateCodec(2)  [zstd = 1, reserved]
```

Reader dispatches on the header byte; an unknown codec id ⇒ clean load failure
with a clear message, never a crash.

---

## Research 3 — binary container + serialisation + write/read

### Container framing (`diagram/container.md` has the byte table)

```
"RDB1" magic (4) | container_ver u16 | codec u8 | flags u8
| payload_raw_size u64 | payload_packed_size u64 | [crc32 u32 if flags.bit0]
| <packed payload>
```
Little-endian; magic enables `file(1)` detection; sizes catch truncation and size
the decompress buffer.

### Payload encoding — the main decision (open question Q2)

| Approach | Fwd/bwd compat | New dependency | Effort | Partial update / query | Precedent |
|---|---|---|---|---|---|
| Hand-rolled TLV + varint | manual (skip unknown tag ids) | none | high, error-prone | no | — |
| **CBOR / MessagePack** | good — self-describing, tolerant of unknown keys | one vendored header (tinycbor / a small msgpack) | low | no | — |
| Protocol Buffers | excellent — optional fields, unknown-field passthrough | libprotobuf + protoc (build-time) | low-med | no | gRPC ecosystems |
| FlatBuffers | excellent + zero-copy random access | flatc (build-time) | med | partial | large read-mostly assets |
| SQLite (`.rdb` = a SQLite db, optionally zstd-wrapped at rest) | excellent — `PRAGMA user_version` migrations | libsqlite3 (**not installed**; amalgamation = 1 file vendored) | med-high | **yes — real SQL, transactions, incremental save** | **En Croissant's game DB is SQLite** (project-cited precedent) |

Two realistic finalists:

- **CBOR payload + DEFLATE container.** Schemaless, tolerant, inspectable, one
  vendored header, no build-time codegen. Whole-file rewrite per save (fine for
  single games). Simplest thing that meets every stated requirement.
- **SQLite `.rdb`.** Literally "Ranls **Database**". `games` / `nodes` /
  `engines` tables, `parent_id` FK, `PRAGMA user_version` migrations, incremental
  writes, and a future opening-explorer / multi-game DB nearly for free. Bigger
  architectural commitment; SQLite not on the host (vendor the ~1-file
  amalgamation). Matches En Croissant, which this project already cites.

### How to write / read (both finalists)

- **Write:** serialise payload → `ICompressor::compress` → write header + bytes →
  `fsync` → `rename` `<path>.tmp` over target.
- **Read:** validate magic/version → read sizes → read packed payload →
  `decompress` (verify raw size matches the hint) → parse → build `VariationTree`.
  Any failure ⇒ `std::nullopt` + `*error`.
- **SQLite variant:** `SQLITE_OPEN_READONLY` for load; `BEGIN IMMEDIATE`…`COMMIT`
  for save; check + step `user_version` migrations; if zstd-wrapped, inflate to a
  temp file / memory db first.

### Abstraction / low coupling (user requirement 4)

```
src/model/game_archive.h
  struct GameGraph { ... };                       // serialisation DTO, decoupled from VariationTree
  GameGraph  toGameGraph(const VariationTree&, boardSize, rule, meta);   // pure, unit-tested
  void       applyGameGraph(const GameGraph&, GameState&);               // pure-ish, unit-tested
  class IGameArchiveReader { virtual std::optional<GameGraph> load(path, string* err) = 0; };
  class IGameArchiveWriter { virtual bool save(path, const GameGraph&, string* err) = 0; };
  RdbArchive   : IGameArchiveReader, IGameArchiveWriter;   // .rdb  — canonical
  YxgameReader : IGameArchiveReader;                       // .yxgame — import only (wraps existing GameIO::loadGame)
  // factory picks impl by file extension
```

`main_window.cpp` depends only on the interface + factory. `.yxgame` **writing is
removed**; `GameIO::saveGame` is deleted or left dead. Everything testable in
`tests/` with the `none` codec and no display.

---

## Open questions

### Resolved with user (2026-09-04)

| # | Question | Resolution |
|---|---|---|
| Q1 | Tree vs DAG **on disk**? | **Tree. No DAG.** User reviewed the cons (comment belongs to a path not a position; ambiguous navigation / delete / mainline) and rejected DAG. An optional `zobrist: u64` per node stays in the schema as a *cheap, ignorable* hint for a possible future reader-side transposition index + eval write-through — it is NOT a structural change and NOT required for v1. |
| Q6 | Multi-game in one `.rdb`? | **No.** One `.rdb` = one game = one full `VariationTree` (mainline + every branch explored via undo/alt-move in that session). "New Game" starts a fresh file's worth of content. A multi-game *library* (opening repertoire, pro-game collections, opening explorer) is a separate future feature. **Container still wraps the single game as a `games: [1 entry]` collection-of-one** so multi-game is an additive change later, not a schema break. |

| Q2 | Payload encoding? | **RESOLVED: CBOR + DEFLATE.** With Q6 = single-game, SQLite loses its main advantage. CBOR: schemaless, string-keyed, tolerant of unknown keys, one vendored header, no build-time codegen. |
| Q3 | Compression codec for v1? | **RESOLVED: DEFLATE via the already-installed `zlib` 1.3.1.** `zstd` = reserved codec id 1 for a later opt-in once `libzstd-dev` is a build dependency. |
| Q7 | Sprint 11 re-scope | **RESOLVED: `feat/rdb-save-format` integration branch.** Sub-PRs `RDB-01`, `RDB-02`, `RDB-03` merge into the feature branch; the branch rebases on `main`; one final PR `feat/rdb-save-format → main`. Sprint 11 re-planned around `RDB-01..03`. |

### Still to confirm

| # | Question | Proposed default |
|---|---|---|
| Q4 | File extension + magic | `.rdb`, magic `"RDB1"`. Mime / `.desktop` association deferred. |
| Q5 | Deprecation window for dropping `.yxgame` write? | **No.** v0.x, no `.yxgame` files expected in the wild. `.yxgame` stays importable indefinitely. `docs/audit/` entry records the format change. |
| Q8 | ANLZ-03 the code | Mark **superseded** (not done) in `TODO.md` + `docs/todo/ANLZ-03-*.md`, cross-link to this feature and `RDB-03`. Regression-test intent carries into `RDB-03`. |

Task split (Q7 resolved):
- **`RDB-01`** — container framing + `ICompressor` (Raw/Deflate) + `GameGraph` DTO + `toGameGraph`/`applyGameGraph` + CBOR payload codec + round-trip unit tests. No UI wiring.
- **`RDB-02`** — `IGameArchive*` interfaces + `RdbArchive` + `YxgameReader` + extension factory; wire into `main_window.cpp` Save/Open; retire `GameIO::saveGame`; keep `.yxgame` import; dialog filters + default extension.
- **`RDB-03`** — extend `TreeNode` (or a parallel struct) with the persisted analysis fields; end-to-end persist + restore of per-node win%/depth/nodes/pv/comment; `invalidateEvalHistoryCache()` on load; NaN-round-trip + old-`.yxgame`-import + out-of-range regression tests (carried from ANLZ-03). `docs/audit/` entry + `CHANGELOG.md`.

## Implementation sequencing (draft)

1. `ICompressor` + `RawCodec` + `DeflateCodec` (zlib) — pure, unit-tested.
2. Container read/write (`RdbContainer`) — header framing, atomic write, truncation
   detection — unit-tested with the `none` codec.
3. `GameGraph` DTO + `toGameGraph` / `applyGameGraph` + CBOR (or SQLite) payload
   codec — round-trip unit tests (mixed evaluated/NaN tree, branches, comments).
4. `IGameArchive*` interfaces + `RdbArchive` + `YxgameReader` + extension factory.
5. Wire into `main_window.cpp` Save/Open; retire `GameIO::saveGame`; keep
   `.yxgame` import. Update file dialogs' filters/default extension.
6. `docs/audit/` entry for the format change; `CHANGELOG.md` `[Unreleased]`.
7. Manual smoke: analyse → save `.rdb` → reopen → WinGraph + tree intact (needs a
   human — no display on the build host).

## Follow-ups

- Multi-game `.rdb` database + opening explorer (Q6 = no for v1).
- SGF import/export as a second `IGameArchiveReader`/`Writer` pair.
- **RenLib `.lib` import** as a third `IGameArchiveReader` — many Renju players hold
  opening libraries in RenLib's format. See "RenLib prior art" below; the `.lib`
  file is a pure tree in DFS pre-order, so it maps cleanly onto `GameGraph`
  (15×15 only, no engine evals ⇒ all nodes NaN + comments/marks).
- `zstd` codec once `libzstd-dev` is a build dependency.

## RenLib `.lib` prior art (researched 2026-09-04)

Source: `github.com/gomoku/RenLib` (`RenLibDoc.cpp`, `MoveNode.cpp`). RenLib is
*the* established Renju library tool; its `.lib` format is worth knowing.

- **Header:** 20 bytes, magic + major/minor version (`CheckVersion()` /
  `WriteHeader()`), old-vs-new format flag.
- **Node = 2 bytes** (min):
  - byte 0 — position, `(y-1)*15 + (x-1)` for the fixed 15×15 board (one byte,
    0..224). A `NO_MOVE` flag marks the root / pass.
  - byte 1 — flags: `0x80` DOWN (has child), `0x40` RIGHT (has sibling), `0x20`
    old-comment, `0x10` MARK, `0x08` COMMENT, `0x04` START, `0x02` NO_MOVE,
    `0x01` EXTENSION (more flag bytes follow, e.g. `BOARD_TEXT`).
- **Structure is implicit** — no pointers. `DOWN` ⇒ the child is the *next*
  record; `RIGHT` ⇒ the sibling follows after this node's *entire subtree*.
  Rebuilt by recursing on the two bits. This is the same DFS-pre-order stream
  RDB uses — RDB just keeps an explicit `parent` index (costs ~1–2 bytes/node
  pre-DEFLATE, ~0 after) for O(1) bound-checking and future incremental writes.
- **Comments / board text:** variable-length, NUL-terminated, padded to an even
  byte boundary, written inline right after the node when its flag is set.
- **Uncompressed.** Compact purely from dense bit-packing.
- Stores *human library annotations* (marks, comments, "start" nodes) — **no
  engine telemetry** (winrate / depth / nodes / pv). The whole ANLZ-03 goal has
  no `.lib` equivalent.

**Does it change the RDB design? No — and deliberately so.** RenLib's density
comes from fixed-15×15 + hand-packed bits + an `EXTENSION` escape hatch — the
exact opposite of requirement 4 ("open structure, low coupling, allow future
update"). DEFLATE already recovers most of the size gap over CBOR. What RenLib
*confirms*: a flat DFS-pre-order node stream is the correct, proven shape for a
serialised game tree. What it *adds to our roadmap*: a `.lib` import reader as a
follow-up.
