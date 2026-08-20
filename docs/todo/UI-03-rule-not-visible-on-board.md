# UI-03 — Selected rule (Renju/Standard) has no effect on what the board shows

**Status:** open
**Area:** board renderer / rule handling
**Priority:** P2
**Source:** UI/UX + codebase review, 2026-08-21

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
