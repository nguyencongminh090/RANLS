# 2026-08-31 — Engine Log gutter tags now use the log's own font

## Prompt

User: "Add small update, no need to write TODO. Scope: text of gutter,
engine_log. Desc: set size and font of gutter text as same of log's text."

## Problem

`BottomPanel::drawGutter()` painted the direction/category tags (`SEND`,
`MESSAGE`, …) with a hardcoded `Pango::FontDescription("Monospace 11")`, while
the Engine Log `TextView` renders its rows in whatever font the `monospace` CSS
class resolves to (theme monospace family, default size). When those differ, a
gutter tag sits at a slightly different baseline/size than the log row it
labels.

## Fix

`src/ui/bottom_panel.cpp` — `drawGutter()` now takes its `FontDescription` from
`engineLogView_.get_pango_context()->get_font_description()`, i.e. the exact
resolved font of the log's own text, instead of the literal `"Monospace 11"`.
Used for both the one-time column-width probe and per-line tag painting, so the
column width also tracks the real font.

Gutter y-positioning was already driven by `engineLogView_.get_line_yrange()`
(the TextView's own layout) and is unchanged; only the paint font moved.

## Scope

One line of behaviour, `drawGutter()` only. No change to `EngineLogModel`, the
UI-10 sticky-scroll path, buffer bounding (RT-02), or clipboard payload (UI-05).

## Verification

- `cmake --build build -j4` — clean, no new warnings.
- `ctest --output-on-failure` — 3/3 (`rapfi-gui-tests`, `rapfi-gui-ui-tests`,
  `rel02-version-single-source`).
- No regression test: the gutter draw callback needs a realized widget +
  display server (same constraint noted for UI-05 / UI-10); the change is a
  direct substitution of the font source with no branching logic to pin.
- Manual visual check pending (no engine binary on the build machine to stream
  real log lines).
