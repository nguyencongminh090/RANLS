# UX-06 — Settings "UI Setting" section: broken toggles + unclear options + polish

**Status:** ✅ VERIFIED (Sprint 6, closed 2026-08-31) — implementation complete; builds clean,
123/123 unit tests pass. Theme, Show Coordinates and the tabbed dialog were **visually verified**
(real screenshots, three theme states); the remaining populated-WinGraph Single↔Two-line check was
**confirmed by the user** during Sprint 6 close-out (2026-08-31).

### Implementation progress (2026-08-30, updated after a real run)

**First pass** removed `uiProfile`, reworked the WinGraph modes, rebuilt the dialog, added the
theme helper — but a real run showed Theme / Coordinates / WinGraph still not visibly working.
Root causes found and fixed:

- **Show Coordinates** — the A–O / 1–N labels *were* being drawn all along, but in the margin
  *outside* the wood board (on the widget/app background), in a fixed dark-brown colour UX-03 had
  tuned for contrast against *wood*. Under a dark theme that is dark-on-dark → invisible; the UX-03
  premise (labels sit on wood) was simply wrong. **Fix:** `BoardView::onDraw` now passes the
  widget's themed `get_color()` foreground to `BoardRenderer::setCoordinateColor()` each frame, so
  the labels follow the theme (the win-graph already does exactly this). Removed the now-unused
  `kCoordR/G/B` constants. **Verified:** labels legible in Light, Dark and System.
- **Theme (System/Light/Dark)** — `gtk_application_prefer_dark_theme` alone is a no-op on this
  machine (KDE, Breeze GTK theme — the property only nudges themes that ship a matching `-dark`
  variant and honour the hint). **Fix:** `applyAppTheme()` now also forces `gtk-theme-name` to
  `Adwaita` / `Adwaita-dark` (both always resolve — Adwaita ships inside GTK); `System` resets both
  properties. **Verified:** startup theme=1 → light app; theme=2 → dark app; theme=0 → untouched
  desktop default. Three visibly distinct states.
- **WinGraph Mode** — the user tested with no analysis run, so `evalHistory()` is empty and both
  modes correctly render the identical empty scaffold ("No analysis yet…"). The mode logic itself
  is fine; it only shows a difference once there is eval data. To make that testable, the pure
  conversion was extracted to `src/ui/win_graph_series.h` (header-only, no gtkmm) and covered by
  `tests/test_ux06_wingraph_series.cpp` (SingleSide = one line / empty white series; BothSide =
  black + complementary white; SingleSide "Auto" follows `enginePlays == White`; NaN gaps kept).

**Other changes (unchanged from first pass):**
- `src/model/config.h`: removed `ViewConfig::uiProfile`; re-documented `WinGraphMode`.
- `src/model/settings_storage.cpp`: dropped `ui_profile` (de)serialization; legacy files with the
  key still load (silently ignored).
- `src/ui/settings_dialog.{h,cpp}`: removed the "UI Profile" dropdown. Rebuilt as a `Gtk::Notebook`
  — **Engine · Time · Search · UI · Hotkeys**, each tab in a `ScrolledWindow`, `set_resizable(true)`,
  tooltips on every row. WinGraph combo relabelled "Single line (engine's perspective)" / "Two
  lines (Black & White)". STATE-02 merge-from-base-config preserved; `onApply` still emits
  `signal_applied`; `MainWindow::onSettings` still passes `matchConfig()` to `save()`. **Verified:**
  dialog opens tabbed and resizable (screenshot).
- `src/ui/analysis_panel.cpp`: `toDisplayWinrate()` now delegates to `buildWinGraphSeries()` and
  takes `EnginePlaysSide` (read from `GameState::matchConfig()`).
- `src/ui/win_graph_view.cpp`: second-series branch now fires for `BothSide` (was `SingleSide`,
  backwards), guarded on non-empty size-matched `whiteData_`; NaN handling preserved.
- `src/main_window.cpp`: `applyAppTheme(AppTheme)` helper, called at startup and from
  `signal_config_changed`; removed the dead `uiProfile` layout-preset block.
- `tests/`: +2 settings round-trip cases (`test_settings_storage.cpp`); +4 WinGraph series cases
  (`test_ux06_wingraph_series.cpp`).

**Verification run:** `./build.sh` clean (only the pre-existing unrelated `gomocup_protocol.cpp`
unused-function warnings); `rapfi-gui-tests` → 123/123 doctest cases pass; `grep -rn uiProfile
src/ tests/` returns nothing. Real GUI run (Wayland, screenshots): Light / Dark / System themes
each render distinctly; board coordinate labels legible in all three; Settings dialog opens as a
resizable 5-tab Notebook with the engine-path validation intact. **Remaining for a human:** run an
engine analysis with moves on the board, then toggle WinGraph Single↔Two-line and confirm one
perspective-correct line vs. two, and that Single follows the "Engine plays" side.
**Area:** settings dialog (`src/ui/settings_dialog.cpp`), view-config application in
`src/main_window.cpp` (`signal_applied` handler), `src/model/config.h`
**Priority:** P2
**Source:** filed 2026-08-30 from a UI review session

## Problems reported

1. **Show Coordinates has no effect** — board renders without A–O / 1–15 coordinates regardless of
   the `checkCoordinates_` state. `ViewConfig::showCoordinates` is written on Apply
   (`settings_dialog.cpp:254`) but the board widget appears not to honour it (or it isn't wired /
   isn't the default). Verify against BoardView draw code.
2. **Theme (System/Light/Dark) does nothing** — `vConfig.theme` is set on Apply
   (`settings_dialog.cpp:251`) but nothing applies it. Need to drive
   `Gtk::Settings::gtk_application_prefer_dark_theme` (or a CSS provider / `Adw` is not linked) from
   the selected `AppTheme`, at startup and on Apply.
3. **"WinGraph Mode" is unclear and misbehaves** — labels "Single side" / "Both side"
   (`WinGraphMode::SingleSide` / `BothSide`, `config.h:14`) don't communicate what changes, and the
   graph doesn't render correctly per mode. Clarify the labels (e.g. "Both players' win rate" vs.
   "Side-to-move advantage" — pick wording with the user) and fix `WinGraphView::onDraw` so each
   mode is actually distinct and correct. See UI-01 (win-rate attribution) for the data side.
4. **"UI Profile" (Default/Compact/Review) is undefined** — a free-text `ViewConfig::uiProfile`
   string with no documented behaviour. Either define concretely what each profile changes (panel
   sizes, visible panels, density) and document it, or remove it until it has a spec. Decide with
   the user.

Plus: **general polish of the Settings window** — the dialog is one long flat grid of ~25 rows.
Group into sections/tabs (Engine · Time · Search · UI · Hotkeys), add per-field help/tooltips,
make it resizable or scrollable.

## Resolved decisions (with user, 2026-08-30)

- **WinGraph Mode:** keep both modes, relabel + reimplement. **SingleSide** = one
  perspective-correct line, default Black, but follows `MatchConfig::enginePlays` side when set
  (UI-06). **BothSide** = Black line + White line, each move converted to that colour's own
  perspective.
- **UI Profile:** remove the control and the `ViewConfig::uiProfile` field entirely (no spec).
- **Show Coordinates / Theme:** wire both to actually take effect (board renderer;
  `gtk_application_prefer_dark_theme`), at startup and on Apply.
- **Dialog:** group into sections/tabs (Engine · Time · Search · UI · Hotkeys), tooltips,
  resizable/scrollable.

Execution guidance: [docs/instruction/UX-06-settings-dialog-ui-section-broken-and-unclear.md](../instruction/UX-06-settings-dialog-ui-section-broken-and-unclear.md)

## Acceptance criteria

- Toggling Show Coordinates changes the board immediately (or on Apply) in both directions.
- Selecting Light/Dark/System visibly re-themes the app and persists across restart.
- WinGraph mode options are self-explanatory and each renders a correct, visibly different graph.
- UI Profile either has a written spec + working effect, or is removed.
- Settings window is organised (sections or tabs) and every control has a label the user
  understands.

## Notes

- No libadwaita in this build (see CMakeLists.txt) — theming must go through GTK `Settings` / CSS.
- `SettingsDialog` has ⚠️ no covering tests — add at least logic tests for the config round-trip
  where feasible.

## Related

- STATE-02 (settings dialog dropped config fields), UX-02 (engine-path validation), UI-01
  (win-rate attribution), gtk-ui-design + ui-ux-review skills.
