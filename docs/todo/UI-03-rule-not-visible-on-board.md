# UI-03 — Selected rule (Renju/Standard) has no effect on what the board shows

**Status:** ✅ DONE
**Area:** board renderer / rule handling
**Priority:** P2
**Source:** UI/UX + codebase review, 2026-08-21

## Resolution (2026-08-21)

Design decision (resolved with the user, see "Open question" below): **indicate only, never
enforce** — a forbidden point is marked but stays fully clickable, useful for analysis / deliberately
setting up illegal positions.

Implemented:
- **Persistent rule indicator** — `MainWindow::ruleLabel_` (`src/main_window.h`), a `Gtk::Label`
  packed into the header bar (`src/main_window.cpp` `buildToolbar()`), always visible regardless of
  menu state. Kept in sync by `MainWindow::updateRuleLabel()`, called once at startup and on every
  `gameState_.signal_rule_changed` emission.
- **Renju forbidden-point detection (domain logic)** — new `src/model/renju_rule.h`/`.cpp`,
  `RenjuRule::isForbidden()`/`forbiddenPoints()`. Implements the standard Renju forbidden-move
  shapes for Black: overline (6+), double-four (4-4), double-three (3-3), with an exact five always
  overriding any of the three (mirrors the Rapfi engine's own `YXSHOWFORBID` semantics — see
  `docs/protocol.md` §7.3 in the sibling Rapfi repo: "checkForbiddenPoint() holds (Renju-only concept
  — 3-3, 4-4, overline)"). Lives in `src/model/`, not `BoardRenderer` — per the software-architecture
  layering rule, `BoardRenderer` only draws coordinates it's handed.
- **BoardViewModel → BoardRenderer wiring** — `BoardViewModel::forbiddenPoints` (populated in
  `update()` only when `rule() == Renju` and it's Black's turn) is drawn by a new
  `BoardRenderer::drawForbiddenPoints()` layer: a ring-with-diagonal-cross shape plus an "X" text
  glyph (UX-03: not colour-only). Marked points are never blocked from a click —
  `GameState::makeMove()` is untouched.
- **`signal_rule_changed` connected** — `MainWindow::connectSignals()` now listens and refreshes
  both `ruleLabel_` and the board view model/redraw. Previously nothing listened to it at all.
- **Standard overline consistency** — `BoardState::checkWin()` (`src/model/board_state.h`/`.cpp`)
  now takes a `GameRule` and applies the engine's own overline rule: Freestyle wins on any 5+;
  Standard requires an exact 5 for either color (6+ does not win); Renju requires exact 5 for Black,
  but White may win with an overline. `GameRule` moved from `game_state.h` to `board_state.h` (no
  circular include) and is re-exposed transitively, so no other call site needed updating.
  `checkWin()` was previously unused anywhere in the codebase and remains so — this fixes the API's
  own correctness bug without adding a new win-announcement feature (out of scope, not requested).

### Verification
- `cmake --build` (Ninja/Release) — clean, no new warnings from the touched files.
- `ctest` — full suite passes, including two new test files:
  - `tests/test_ui03_renju_forbidden.cpp` (8 cases): isolated stone not forbidden, five overrides
    forbidden, overline forbidden, double-four forbidden vs. a single four not forbidden,
    double-three forbidden vs. a single open three not forbidden, `forbiddenPoints()` aggregation.
  - `tests/test_ui03_standard_overline.cpp` (7 cases): Freestyle/Standard/Renju × overline/exact-five
    × Black/White, cross-checked against the Rapfi engine's `CheckOverline` condition.
- Manually traced the double-four and double-three test boards cell-by-cell against the algorithm
  to confirm the shapes match real Renju rule definitions, not just "the code returns true."

### Known limitation (documented in `renju_rule.h`)
A "four" completion is only counted if it yields an *exact* five (not a completion that would only
produce an overline) — the common simplification most Gomocup-protocol clients use. Exotic multi-line
edge cases beyond straight/single-gap four and three shapes are not exhaustively covered. Given this
is indication-only (not enforcement), a rare misclassification does not block or corrupt gameplay.

### Explicitly out of scope
- No move rejection/enforcement — forbidden points remain fully playable, by design decision.
- No new win-announcement UI wired to `checkWin()` — it was unused before this fix and stays
  available-but-unused after it; wiring it up is a separate feature, not part of this bug's scope.
- PROTO-02/STATE-03/RT-04 and other already-fixed items untouched.

## Problem

`GameRule` (`src/model/game_state.h:14-18`) is applied in exactly one place: it is forwarded to the
engine as an `INFO rule N` command (`src/main_window.cpp:474-478` →
`src/engine/gomocup_protocol.cpp:205-207`).

Nothing in the UI reflects it:

- `BoardRenderer` has eight draw layers (`src/ui/board_renderer.cpp:79-86`) and none of them is a
  forbidden-move layer. There is no Renju forbidden-point indication on the board.
- `BoardState::checkWin` (`src/model/board_state.h:66`) is a plain 5-in-a-row check with no
  overline handling for Standard rule.
- `GameState::makeMove` (`src/model/game_state.cpp:52-76`) validates only "in bounds" and "cell
  empty" — it does not consult `rule_` at all.
- `signal_rule_changed` (`src/model/game_state.h:86`) is emitted by `setRule`
  (`src/model/game_state.cpp:257-261`) but **nothing connects to it**.

So the rule is a dropdown the user must remember having set. Under Renju, the GUI will happily let
Black play a forbidden double-three while the engine considers it illegal — a silent divergence
between what the board shows and what the engine is analyzing.

The `ui-ux-review` checklist (item 6) asks for exactly this: forbidden-move indication visible on the
board itself, not just in a config dropdown.

## Acceptance criteria

- The active rule is visible somewhere persistent in the UI, not only inside the menu.
- Under Renju, forbidden points for Black are marked on the board.
- Under Standard, overline handling is consistent between what the GUI accepts and what the engine
  enforces.
- `signal_rule_changed` is connected to whatever refreshes this, or removed.
- Decide and record where forbidden-move detection lives — this is domain logic and belongs in
  `src/model/`, not in `BoardRenderer` (see the `software-architecture` skill on layer boundaries).

## Open question for the user

Should the GUI *enforce* rules (reject a forbidden move on click) or only *indicate* them (mark the
point, still allow the move for analysis purposes)? An analysis tool often wants the second — being
able to set up an illegal position deliberately is useful. This needs deciding before implementation;
consider working it through `features/<slug>/` first if the answer isn't obvious.

## Related

- UX-03 (colour-only meaning — forbidden marks need shape/text, not just a colour)
