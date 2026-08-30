# Instruction — UX-06: fix + clarify the Settings dialog "UI Setting" section

Detail: [docs/todo/UX-06-settings-dialog-ui-section-broken-and-unclear.md](../todo/UX-06-settings-dialog-ui-section-broken-and-unclear.md)

## Design decisions (resolved with the user 2026-08-30 — do not re-open)

### WinGraph Mode — keep both modes, define them precisely

`WinGraphMode` stays a two-option setting. Relabel and re-implement so each mode is correct and
visibly distinct:

- **SingleSide** (label suggestion: **"Single line (engine's perspective)"** — final wording at
  implementer's discretion as long as it conveys the below):
  - One win-rate line.
  - Default perspective = **Black**. An opponent-perspective win-rate is converted to this
    perspective as `100% - x`.
  - **Auto:** when `MatchConfig::enginePlays` (from UI-06) is `Black` or `White`, the single line
    follows **that** side's perspective instead of the Black default. `enginePlays == Off`
    (manual analysis) → Black perspective.
- **BothSide** (label suggestion: **"Two lines (Black & White)"**):
  - Draw a Black line and a White line together.
  - For every move, convert the raw win-rate to each colour's own perspective before plotting
    (e.g. a White-to-move eval expressed for White is converted to Black's perspective for the
    Black line, and shown as-is on the White line). Net effect: per-move win-rate for each colour.

### UI Profile — remove it

`ViewConfig::uiProfile` has no spec. **Remove the control from the dialog and remove the
`uiProfile` field** from `ViewConfig` (`src/model/config.h:28`). Drop its serialization in
`src/model/settings_storage.cpp`. Do not leave a dead widget.

### Show Coordinates — make it actually work

`ViewConfig::showCoordinates` is written on Apply but the board ignores it. Wire it into the board
rendering path (`src/ui/board_view.cpp` / `board_renderer` / `board_geometry`) so toggling it
adds/removes the A–O / 1–N coordinate labels, applied on Apply and honoured at startup.

### Theme (System/Light/Dark) — make it actually work

No libadwaita in this build. Drive
`Gtk::Settings::get_default()->property_gtk_application_prefer_dark_theme()` (and/or a CSS provider)
from `ViewConfig::theme`, both at startup and on Apply. `System` = leave the GTK default untouched.

### Dialog organisation

Group the ~25-row flat grid into sections or a `Gtk::Notebook`: **Engine · Time · Search · UI ·
Hotkeys**. Add tooltips / per-field help. Make the window resizable or scrollable.

## Approach

1. Do UI-06 first (it creates `MatchConfig`); this task reads `matchConfig().enginePlays` for the
   SingleSide "Auto" perspective. If for some reason `MatchConfig` is not yet present when you
   start, implement the Black-default SingleSide behaviour and leave a clearly-marked TODO for the
   Auto hook rather than inventing a parallel setting.
2. `src/ui/win_graph_view.cpp` `onDraw` (`src/ui/win_graph_view.cpp:68`) + `setData`
   (`src/ui/win_graph_view.cpp:56`): the mode branching at line 131 currently only draws the white
   series in `SingleSide` mode, which is backwards. Rework so SingleSide = one perspective-correct
   line, BothSide = two perspective-correct lines. The per-move perspective conversion should
   happen where the series are built (see `GameState::evalHistory` /
   `toDisplayWinrate` referenced at `src/ui/win_graph_view.cpp:113`) — check
   `src/model/game_state.cpp:355` (`evalHistory`) and the win-graph data feed in
   `src/ui/analysis_panel.cpp` / `src/main_window.cpp`.
3. `src/ui/settings_dialog.cpp`: relabel the WinGraph combo, remove the UI Profile control, add
   the section grouping + tooltips, make resizable/scrollable.
4. `src/model/config.h`: remove `uiProfile`.
5. `src/model/settings_storage.cpp`: drop `uiProfile` (de)serialization; keep back-compat (ignore
   the key if present in an old file, don't error).
6. Theme application: a small helper called from startup and from the settings `signal_applied`
   handler in `src/main_window.cpp`.
7. Coordinates: thread `showCoordinates` into the renderer; ensure `signal_applied` triggers a
   board `queue_draw`.

## Boundaries — do not touch

- Do not touch the menu bar or `MatchConfig` definition (UI-06 owns those). You only *read*
  `matchConfig().enginePlays`.
- Do not change win-rate *attribution* logic (that was UI-01) — only the per-mode display /
  perspective conversion and the mode labels.
- Do not add new `ViewConfig` fields beyond what's needed; the net field change here is a
  **removal** (`uiProfile`).
- Do not introduce libadwaita.
- Keep the STATE-02 fix intact — the dialog must still preserve `multiPV` / `customParams` on Apply.
- Don't redesign hotkey capture — just move those rows into a section.

## Pitfalls

- `win_graph_view.cpp:131` guards on `whiteData_.size() == blackData_.size()` — preserve a
  size-mismatch guard in the new code.
- NaN handling (unevaluated positions) at `win_graph_view.cpp:113-128` must be preserved in both
  modes — don't interpolate through gaps.
- Theme toggle via `prefer_dark_theme` can require re-applying on Apply without a restart; verify
  the board's own fixed wood colour is unaffected (it deliberately doesn't follow the theme) but
  the win-graph / labels do follow `get_color()`.
- Removing `uiProfile`: grep the whole tree for `uiProfile` — there may be readers in
  `command_dispatcher.cpp` or elsewhere.
- `SettingsDialog` has no test coverage — add config round-trip logic tests where feasible
  (`tests/` — mirror `tests/test_settings_storage.cpp`), at minimum for the `uiProfile` removal
  back-compat and the theme/coordinates fields surviving save+load.

## Verification before marking this task done

1. **Build:** clean build, no new warnings.
2. **Unit tests:** `ctest` — all pass, including new settings round-trip cases.
3. **Grep:** `grep -rn uiProfile src/ tests/` returns nothing after the change.
4. **Manual smoke (done, or reported skipped-with-reason if no display server):**
   - Toggle Show Coordinates → board labels appear/disappear, and the state survives a restart.
   - Switch System/Light/Dark → app visibly re-themes, survives a restart.
   - Switch WinGraph mode → SingleSide shows one perspective-correct line (and follows the
     engine's side when UI-06's "Engine plays" is set); BothSide shows two perspective-correct
     lines. Each mode looks clearly different.
   - UI Profile control is gone; opening an old settings file with a `uiProfile` key still loads.
   - Settings window is sectioned/tabbed, resizable, every control has a comprehensible label.

Passing unit tests alone is NOT sufficient — build clean + grep clean + manual smoke (or explicit
skip reason) all required.
