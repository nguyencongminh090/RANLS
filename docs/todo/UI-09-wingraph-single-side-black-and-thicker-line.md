# UI-09 — Win-rate graph: SingleSide is always Black; thicker, higher-contrast line

**Status:** ✅ DONE (Sprint 7, 2026-08-31) — `WinGraphMode::SingleSide` now unconditionally Black's
perspective (`buildWinGraphSeries` `whitePerspective` branch removed; `enginePlays` param kept but
`(void)`-discarded); `BothSide` untouched. Win-rate line thickened (main 1.5→2.8 px, White 1.2→2.6
px) and recoloured to blue `#1A73E8` + green `#1E8E3E`, which clear WCAG 3:1 non-text contrast
against both the light (`#fafafb`: 4.32 / 4.03:1) and dark (`#242424`: 3.45 / 3.69:1) Adwaita panel
backgrounds; White line keeps its (widened) dash for CVD-safe shape redundancy; `dataviz` palette
validator all-PASS in both modes. `config.h` / `win_graph_series.h` doc comments + Settings combo
label ("Single line (Black's perspective)") updated. `tests/test_ux06_wingraph_series.cpp`: the
"SingleSide Auto follows enginePlays == White" case replaced with "SingleSide is always Black
regardless of enginePlays" (other 3 cases kept). `./build.sh` clean; `ctest` 2/2 suites — 132
`rapfi-gui-tests` + 6 `rapfi-gui-ui-tests` cases pass. Line-width/colour change verified by code
inspection + documented contrast math (no screenshot harness). UX-06 reversal rationale recorded in
[docs/fix-log/2026-08-31-ui-09-wingraph-single-side-black-and-thicker-line.md](../fix-log/2026-08-31-ui-09-wingraph-single-side-black-and-thicker-line.md).
**Area:** `src/ui/win_graph_view.cpp`, `src/ui/win_graph_series.h` (`buildWinGraphSeries`),
`src/ui/analysis_panel.cpp` (`toDisplayWinrate`), `WinGraphMode` docs (`src/model/config.h`)
**Priority:** P2
**Source:** UI review request, 2026-08-30

## Problem / request

Three related changes to the win-rate graph:

### 1. SingleSide = always Black (remove the "follow engine's side" behaviour)

UX-06 (Sprint 6) made `WinGraphMode::SingleSide` show one perspective-correct line that **follows
`MatchConfig::enginePlays`** — Black by default, the engine's side when set. The user now wants
SingleSide to be **unconditionally Black's perspective**, dropping the `enginePlays` coupling
entirely.

- Remove the `EnginePlaysSide` argument path from `buildWinGraphSeries` / `toDisplayWinrate` for
  the single-side case (or fix it to a constant Black).
- Update `tests/test_ux06_wingraph_series.cpp` — the "SingleSide Auto follows enginePlays == White"
  case must be replaced with "SingleSide is always Black regardless of enginePlays".
- Update the `WinGraphMode` doc comment in `config.h` and the settings-dialog combo label
  ("Single line (engine's perspective)" → e.g. "Single line (Black's perspective)").
- **Write notes**: record the rationale for removing the coupling in `docs/notes/` (or the fix-log
  detail) so the reversal of the UX-06 decision is traceable.

### 2. BothSide — unchanged

Keep the two-line Black + White perspective-correct rendering as-is.

### 3. Thicker, easier-to-read win-rate line — colour task

The line is currently too thin / low-contrast against the panel. Make it visibly thicker and pick
a colour that stays legible in both light and dark themes.

- Increase Cairo `set_line_width` for the series stroke (pick a concrete value, e.g. 2.5–3.0 px
  device-independent; verify on HiDPI).
- **Colour theory / accessibility pass** (the "find colour" sub-task): choose series colours that
  meet WCAG contrast against both the light and dark panel backgrounds, and are distinguishable
  from each other (BothSide) and from the axis/grid. Black & white strokes with a subtle outline/
  halo is one option raised by the user; a distinct accent colour is another. Document the chosen
  values and their contrast ratios. Use the `dataviz` skill for palette guidance.
- Keep the axis/grid scaffold subordinate to the data line.

## Acceptance criteria

- SingleSide renders Black's perspective for every position regardless of `MatchConfig::enginePlays`.
- BothSide behaviour and appearance unchanged except for the new line width/colour.
- Win-rate line is visibly thicker and passes a contrast check in light + dark themes; chosen
  values + ratios recorded in the fix-log/notes.
- Series tests updated; build + full test suite (incl. `rapfi-gui-ui-tests`) green.

## Scope boundary

- Do not change win-rate *attribution* logic (UI-01) or the eval→win-rate conversion maths.
- Do not redesign the graph axes / layout beyond line width + colour.

## Related

- Reverses the SingleSide "Auto" decision in `docs/todo/UX-06-settings-dialog-ui-section-broken-and-unclear.md`
- UI-01 (win-rate attribution), UI-08 (same widget — empty-state text), UX-06 (WinGraph modes),
  `dataviz` + `ui-ux-review` skills
