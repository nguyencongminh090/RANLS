# 2026-08-21 — Selected rule (Renju/Standard) had no effect on what the board shows (UI-03)

## Summary

The active `GameRule` (Freestyle/Standard/Renju) was forwarded to the engine but never reflected
anywhere in the GUI: no persistent rule indicator, no Renju forbidden-point marking, an
overline-blind `BoardState::checkWin()`, and a `signal_rule_changed` signal with zero listeners.
Under Renju the board would happily let Black set up a forbidden double-three with no visual cue
that the engine considers it illegal.

Design decision (confirmed with the user before implementation): the GUI **indicates** forbidden
points, it does not **enforce** them — a marked point stays fully clickable, which is useful for
analysis / deliberately constructing illegal positions. `GameState::makeMove()` was left untouched.

## Action

- **Persistent rule indicator.** Added `Gtk::Label ruleLabel_` to `MainWindow`
  (`src/main_window.h`), packed into the header bar in `buildToolbar()` (`src/main_window.cpp`), so
  the active rule is visible without opening the Game > Rule menu. `MainWindow::updateRuleLabel()`
  refreshes it; called once at startup and from a new `gameState_.signal_rule_changed` connection in
  `connectSignals()` (previously this signal had no listeners at all).
- **Renju forbidden-point detection — new domain module.** `src/model/renju_rule.h`/`.cpp`:
  `RenjuRule::isForbidden(board, pos)` and `RenjuRule::forbiddenPoints(board)`. Implements
  overline (6+), double-four (4-4), and double-three (3-3) detection for Black, with an exact
  five-in-a-row always overriding any of the three (mirrors the Rapfi engine's own
  `checkForbiddenPoint()`/`YXSHOWFORBID` semantics — Renju-only concept: 3-3, 4-4, overline; see
  `docs/protocol.md` §7.3 in the sibling Rapfi engine repo). Placed in `src/model/`, not
  `BoardRenderer`, per the project's software-architecture layering rule (model owns domain logic,
  UI only renders it).
- **View-model → renderer wiring.** `BoardViewModel::forbiddenPoints` (new field, populated in
  `update()` only when `rule() == GameRule::Renju` and it is currently Black's turn) is drawn by a
  new `BoardRenderer::drawForbiddenPoints()` layer — a "no entry"-style ring with a diagonal cross
  plus a small "X" text glyph, so the marker doesn't rely on colour alone (UX-03). `BoardRenderer`
  never computes the Renju rule itself; it only receives the already-computed coordinates.
- **Standard/Renju overline consistency.** `BoardState::checkWin()` (`src/model/board_state.h`/
  `.cpp`) now takes a `GameRule` parameter and applies the same overline condition the Rapfi engine
  uses (`game/pattern.cpp`: `CheckOverline = R == STANDARD || (R == RENJU && Black)`): Freestyle
  wins on any run of 5+; Standard requires an *exact* 5 for either color (a 6+ overline does not
  win); Renju requires exact 5 for Black but allows White to win with an overline. `GameRule` moved
  from `game_state.h` into `board_state.h` to avoid a circular include; `game_state.h` re-exposes it
  transitively so no other include site needed changing. `checkWin()` had no callers anywhere in the
  codebase before this fix and still has none after it — this corrects the API's own bug without
  adding a new (out-of-scope) win-announcement feature.

## Test plan

Two new unit-test files added to the existing headless `tests/` suite (`rapfi-gui-tests`, no gtkmm):

- `tests/test_ui03_renju_forbidden.cpp` — 8 cases: isolated stone not forbidden; an exact five
  always overrides forbidden; overline (6-in-a-row) is forbidden; a double-four (two crossing
  three-in-a-rows sharing the candidate point) is forbidden while a single four is not; a
  double-three (two crossing open threes) is forbidden while a single open three is not;
  `forbiddenPoints()` correctly aggregates a known forbidden coordinate.
- `tests/test_ui03_standard_overline.cpp` — 7 cases: Freestyle/Standard/Renju × overline/exact-five
  × Black/White, matching the Rapfi engine's `CheckOverline` rule.

Both files' shapes were manually traced cell-by-cell against the algorithm before trusting the
"passes" result, to confirm the double-three/double-four/overline classifications match real Renju
rule definitions and not just "the code happens to return true."

## Verification

- `cmake -G Ninja` + `cmake --build` (Release) — clean build, no new warnings introduced by the
  touched files.
- `ctest` — full suite passes (all pre-existing tests + the 15 new UI-03 cases).
- Confirmed via `docs/protocol.md` (sibling Rapfi engine repo) that the Renju forbidden-point
  concept (3-3, 4-4, overline) and the Standard-rule overline exclusion match this implementation's
  semantics, rather than reinventing rule definitions from scratch.

## Known limitation

`RenjuRule`'s "four" detection only counts a completing move if it yields an *exact* five (not one
that would merely complete an overline) — documented in `renju_rule.h` as a deliberate, common
simplification. Exotic multi-line combinations beyond straight/single-gap four and three shapes are
not exhaustively handled. Since this feature is indication-only (never enforced), a rare
misclassification cannot corrupt gameplay or block a move.

## Out of scope (explicitly not done)

- No move rejection/enforcement of forbidden points — by design decision, they stay fully playable.
- No new "you win" / game-end UI wired to the now-rule-aware `checkWin()` — it remains an unused,
  now-correct API, same as before this fix; wiring a win announcement is a separate feature.
- PROTO-02/STATE-03/RT-04 and other already-closed items were not touched.
