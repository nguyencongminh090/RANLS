# 2026-08-21 — PROTO-02: hardcoded board size 15 breaks every non-15×15 board

**Task:** [docs/todo/PROTO-02-hardcoded-board-size-15.md](../todo/PROTO-02-hardcoded-board-size-15.md)

## Problem

Three places assumed a 15×15 board regardless of the actual `GameState::boardSize()`:

- `parseEngineCoord` (`src/engine/gomocup_protocol.cpp`), a file-static helper with no access to
  `boardSize_`, hardcoded `15 - rowNumber` when decoding Yixin's `flipY_X` A1-notation coordinates
  — silently corrupting parsed move data (not just a display label) on any other board size.
- `EngineStatusView::update` passed a literal `15` into `coordStr()` for the "Best:" readout, while
  `PVView`/`TreeExplorer` were already correctly passed the real board size.
- `BoardRenderer::drawGrid` only drew star points when `bs == 15`, losing them entirely at every
  other size.
- Related staleness risk: `GomocupProtocol::boardSize_` was only refreshed by `generateStart()`,
  reached from `EngineController::sendConfig()`, which returned early (skipping `generateStart()`
  entirely) whenever the engine was not yet usable — so a board-size change made while the engine
  was stopped left the parser working off a stale size until some later successful `sendConfig()`.

## Fix

- `parseEngineCoord` now takes `boardSize` as a parameter; all five call sites in
  `gomocup_protocol.cpp` pass `boardSize_` (or the local `boardSize` parameter inside
  `parseMoveTokens`).
- `EngineController::sendConfig()` now calls `protocol_->generateStart(gameState_.boardSize())`
  unconditionally, before the `isUsable()` early-return — only the actual wire commands (`startCmds`,
  `generateConfig`) stay gated on `isUsable()`. This keeps `protocol_`'s cached `boardSize_` in sync
  with the model even while the engine is stopped.
- `MainWindow::onNewGame()` calls `controller_.sendConfig()` after `gameState_.newGame()` (which
  resets to `DEFAULT_BOARD_SIZE`), matching every other board-size-changing call site.
- `EngineStatusView::update()` gained a required `int boardSize` parameter (default argument
  removed, not left as an easy-to-forget optional); `AnalysisPanel` now passes
  `gameState_.boardSize()`.
- `BoardRenderer::drawGrid` generalizes the star-point layout from the fixed 15×15 set
  (`{3,11},{7,7},...}`) to `offset = 3` from each edge plus the true center (`bs / 2`), gated on
  `bs % 2 == 1 && bs >= 9` — even sizes have no single-intersection center, and sizes below 9 have
  no room for corner/center star points to stay visually distinct, so both are omitted cleanly
  rather than drawing something misleading.

## Files touched

- `src/engine/gomocup_protocol.cpp` — `parseEngineCoord` signature + all call sites.
- `src/engine/engine_controller.cpp` — `sendConfig()` reordered so `generateStart()` always runs.
- `src/main_window.cpp` — `onNewGame()` resyncs the protocol after resetting board size.
- `src/ui/engine_status.h`, `src/ui/engine_status.cpp` — `update()` takes real `boardSize`.
- `src/ui/analysis_panel.cpp` — passes `gameState_.boardSize()` into `EngineStatusView::update()`.
- `src/ui/board_renderer.cpp` — star points computed from board size.
- `tests/test_gomocup_protocol.cpp` — five new regression cases (see below).

## Verification

- `bash build.sh` (Ninja, Release) — clean build; only pre-existing, unrelated `-Wunused-function`
  warnings in `gomocup_protocol.cpp` (functions unused before this change too).
- `RUN_TESTS=1 bash build.sh` / `ctest --test-dir build_cmd --output-on-failure`:
  ```
  Test project .../build_cmd
      Start 1: rapfi-gui-tests
  1/1 Test #1: rapfi-gui-tests ..................   Passed    0.03 sec
  100% tests passed, 0 tests failed out of 1
  ```
  Full doctest run: `[doctest] test cases: 65 | 65 passed | 0 failed | 0 skipped`,
  `assertions: 260 | 260 passed | 0 failed`.
- Filtered to just the new PROTO-02 cases (`-tc="*A1-notation*"`):
  `test cases: 5 | 5 passed | 0 failed | 60 skipped`, `assertions: 21 | 21 passed | 0 failed`.
  Cases cover: A1-notation flip at 5×5 and 22×22 (replacing the old hardcoded-15 flip), the 22×22
  top-right corner (`V22`), a regression guard that 15×15 behavior is unchanged, and A1-notation
  tokens inside a PV line (`INFO BESTLINE`) at 5×5 going through `parseMoveTokens`.
- `grep -n '\b15\b' src/engine/gomocup_protocol.cpp src/ui/engine_status.cpp src/ui/board_renderer.cpp`
  — no remaining board-size-15 literal; the three hits left are comments referencing the old
  15×15-only behavior for context, and an unrelated color constant name (`kVariantR`, not a `15`).
- Manual verification at 5×5/15×15/22×22 in a live display session (the acceptance criterion's
  "stone placement, coordinate labels, Best: readout, PV move labels") was **not performed** — no
  display server / live engine subprocess available in this sandboxed session. Coverage instead
  comes from the unit tests above, which exercise the exact parsing and flip-math path a live engine
  reply would take at those same three sizes, plus manual reading of `EngineStatusView`/
  `BoardRenderer`'s call sites to confirm the real board size reaches them.

## Status

✅ DONE — marked in `TODO.md` and `docs/todo/PROTO-02-hardcoded-board-size-15.md`.

## Scope notes

- Layout/legibility at extreme board sizes (stone size, label overlap, click-target size) is out of
  scope here — that's `UX-03`, per the task's own scope boundary.
