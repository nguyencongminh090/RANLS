# UI-23 — Typography and layout polish

**Status:** ✅ DONE (2026-11-11, branch `ui-23/typography-and-layout-polish`). Size M (ui layer only, reversible).
Pulled straight into Sprint 27 on the user's direct request ("Improve UI of program. Look also to
the Font (Typography). Design and apply without my decision.").

Decision record: [audit 2026-11-11 — UI-23 typography](../audit/2026-11-11-ui23-typography.md).

## What

- **Type system in `style.css`.** Keep the desktop UI font (no global family override). Relative sizes:
  caption 0.85em / body 1em / title 1.15em; no px font sizes. Weights 400/600/700.
- **Numeric readouts** (engine stats, PV score/depth/moves, tree table) use the UI font with tabular
  figures (`font-feature-settings: "tnum"`) instead of monospace.
- **Monospace only for raw text** (logs, console rows), with a named stack: JetBrains Mono → Cascadia
  Mono → Hack → Fira Mono → DejaVu Sans Mono → Consolas → monospace; 0.92em.
- **Engine status** becomes two rows: state + controls, then a wrapping `Gtk::FlowBox` of stats (quiet
  caption keys, semibold values). The one-line row used to set the analysis panel's minimum width and
  squeezed the board (worst in Vietnamese).
- **Win-rate graph** axis labels and hover tooltip are drawn with Pango in the widget font (caption
  scale, tabular figures, right-aligned, centred on gridlines) instead of Cairo toy text at 9/10px.
  Faint 0%/100% bounds added.
- **Board coordinate labels** use the UI font family (passed from `BoardView`) instead of fontconfig's
  generic `sans-serif`. Sizes and positions are unchanged; stone numbers are not touched.
- **Move log** gets 12/8px text margins (the engine log is unchanged because its gutter is aligned to buffer lines).
- **BoardView minimum size** of 240×240 (`kBoardMinPx`). This also fixes the GtkPaned 1px measure warning; see
  [fix-log](../fix-log/2026-11-11-ui-23-paned-measure-warning.md).

## Verification

- Release build; `ctest` 5/5 under Xvfb (UI tests ran, not skipped). New `tests/test_ui23_paned_min_measure.cpp` (2 cases; failed before the fix).
- Screenshots read: light/en analysing, dark/vi analysing, 22×22, 5×5. No GTK warnings in any run.
- Not verified: Windows/MSYS2 font fallback, HiDPI, a desktop with text scaling ≠ 1.
