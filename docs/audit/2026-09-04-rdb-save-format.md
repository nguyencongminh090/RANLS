# 2026-09-04 — On-disk save format changed: text `.yxgame` → binary `.rdb`

## Decision

As of RDB-02, RANLS writes games **only** in the binary `.rdb` container
(`src/model/rdb/` — RDB-01 substrate: `"RDB1"` framing header + zlib crc32 +
DEFLATE codec + a hand-rolled CBOR payload; see
`features/rdb-save-format/diagram/container.md`). The legacy plain-text
`.yxgame` format (`GameIO`, IO-01) is now **import-only**:

- `GameIO::saveGame` is deleted. There is no code path that produces a `.yxgame`
  file, and the Save dialog offers a single `.rdb` filter with default name
  `game.rdb`.
- `GameIO::loadGame` is unchanged and still parses `.yxgame` on open, wrapped by
  `rdb::YxgameReader`, which maps its flat move list into a linear-chain
  `GameGraph` with no per-node analysis (every eval NaN — identical to a fresh
  game). The Open dialog lists `.rdb` (default), `.yxgame`, and all files.
- **No migration tool.** A user who wants a `.yxgame` in the new format opens it
  and re-saves; there is no bulk converter, no auto-upgrade, no recent-files or
  auto-save (all explicitly out of scope per `features/rdb-save-format/user_story.md`
  and IO-01).

## Rationale

- `.yxgame` stores only the played line (board size, rule, moves) — it cannot
  represent the variation tree, per-move comments, or any engine analysis. The
  reason ANLZ-03 ("reloaded game keeps its WinGraph") could not be done as a
  `.yxgame` extension: the user rejected growing an ad-hoc text schema and chose
  a structured, versioned binary container instead (user decision 2026-09-04,
  recorded in `TODO.md` — ANLZ-03 superseded by RDB-01/02/03).
- `.rdb` carries a payload `schema` version and skips unknown CBOR keys on read,
  so RDB-03 can add per-node PV / annotation glyphs / engine metadata /
  timestamps without a format break and without touching this decision.
- Keeping `.yxgame` readable (rather than dropping it outright) costs one thin
  adapter class and preserves every game a user saved under IO-01.

## Scope / non-goals

- RDB-02 persists only the `TreeNode` fields that exist today
  (`move`/`eval`/`nodes`/`depth`/`comment`). PV, glyphs, engine info, timestamps:
  the DTO carries them, RDB-02 neither writes nor restores them — **RDB-03**.
- No SGF, no RenLib import (follow-ups noted in `features/rdb-save-format/planning.md`).
- `GameIO::kFormatVersion` / `yxgame_version` left as-is (still checked-and-rejected
  on read).
