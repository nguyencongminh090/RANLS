# ANLZ-03 — persist-winrate-in-save-file

## Approach

Additive, backward-compatible field on the existing `.yxgame` plain-text schema. The per-node eval
already exists on the variation tree (written by UI-13 candidate A and ANLZ-01); this task only
serialises it on save and restores it on load. No new analysis, no new file format.

1. Read `src/model/game_io.cpp` end-to-end first — find the move-record writer/parser and
   `kFormatVersion` / `yxgame_version`. Confirm how an unknown/newer field is currently handled by
   the loader (skip vs. abort) before choosing the token syntax.
2. Pick a token that a pre-ANLZ-03 loader ignores gracefully (or gate the whole field on the bumped
   `kFormatVersion` so old binaries never see it). Old file → new binary must also work: absent
   token ⇒ NaN sentinel.
3. Save: iterate nodes, write the token only for non-NaN evals.
4. Load: parse when present, validate range, set node eval, then invalidate the `evalHistory()`
   cache (`invalidateEvalHistoryCache()` / `treeDirty_`, same path UI-13 uses).

## Pitfalls

- **NaN must round-trip as absence.** Writing `0.5` (or `0.0`) for an unevaluated node reintroduces
  the exact "false 50%" bug UI-01 fixed. Test this explicitly.
- **Backward compat both directions.** Old file + new binary (no token → NaN). New file + old binary
  (token must not break the parse — verify against the current loader's unknown-field behaviour).
- **`kFormatVersion` is not `APP_VERSION`.** REL-02 single-sourced the *app* version; the save-file
  schema version is separate and independent. Bump only `kFormatVersion` / `yxgame_version`. Do not
  touch CMake `project(VERSION)` or `src/version.h.in`.
- **Eval range.** Confirm whether the stored eval is win-probability `[0,1]` or something else
  (`evalHistory()` / `setAnalysisData` semantics) and validate against that. Out-of-range → absent,
  never clamp-and-keep.
- **Don't trigger analysis on load.** The engine must not be poked; this is pure persistence.

## Verification before done

- `./build.sh` clean (only the 3 known pre-existing `-Wunused-function` warnings in
  `gomocup_protocol.cpp`).
- `ctest` 3/3 green, including new model-layer round-trip cases in `ranls-gui-tests`.
- New test file (e.g. `tests/test_anlz03_winrate_persistence.cpp`): (a) mixed evaluated/NaN game →
  save → load → `evalHistory()` matches; (b) synthetic old-format file (no win% tokens) → loads,
  all-NaN, no crash; (c) out-of-range token → treated as absent.
- Manual: save a game with a populated WinGraph, reopen, confirm the graph is restored (needs a
  human — no display on the build host; note it in the fix-log).

## Boundaries

- `src/model/game_io.cpp`, `src/model/game_state.{h,cpp}`, `src/model/variation_tree.*`,
  `tests/`. Nothing in `src/ui/` except read-only reference.
- No eval→win% maths, UI-01 attribution, ANLZ-04 bridge, or `buildWinGraphSeries` changes.
- No SGF, no new file format — extend `.yxgame` in place.
- No app-version change.
