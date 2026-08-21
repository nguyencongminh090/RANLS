# RT-02 — Engine log grows unbounded and writes per-line

**Status:** ✅ FIXED — see [fix-log](../fix-log/2026-08-21-rt-02-engine-log-unbounded.md)
**Area:** bottom panel / engine log
**Priority:** P0
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

Every line the engine emits is appended to the Engine Log immediately, with no cap and no batching.

Path: `GomocupProtocol::signal_log` → `EngineController::signal_engine_output`
(`src/engine/engine_controller.cpp:32-42`) → `src/main_window.cpp:350` →
`BottomPanel::appendRecv` → `BottomPanel::appendLogLine` (`src/ui/bottom_panel.cpp:141-165`).

Per single engine line, `appendLogLine` performs:

- two `Gtk::TextBuffer::insert` calls (gutter buffer + content buffer), plus two more for the
  newline separators,
- `create_mark` → `scroll_to` → `delete_mark`, forcing an immediate scroll and TextView relayout.

There is **no line cap anywhere**. During a long analysis session both `TextBuffer`s grow without
bound, so memory climbs for the lifetime of the session and each insert gets progressively more
expensive.

This is the second-largest contributor to realtime lag after RT-01.

## Secondary defect (same widget): gutter/content line desync

The gutter uses `Gtk::WrapMode::NONE` (`src/ui/bottom_panel.cpp:20`) while the content view uses
`Gtk::WrapMode::WORD_CHAR` (`src/ui/bottom_panel.cpp:60`). A long `RECV` line occupies two visual
lines in the content view but one in the gutter, so `syncScroll`
(`src/ui/bottom_panel.cpp:131-139`) — which syncs by pixel offset — leaves every subsequent
`[SEND]`/`[RECV]` label permanently misaligned against its line.

Filed here rather than separately because any fix touches the same widget and likely the same
rendering decision (a single `TextView` with a tagged prefix column, or a `ColumnView`, removes
both problems at once).

## Acceptance criteria

- The engine log keeps a bounded number of lines (configurable; suggest ~5,000 default) — oldest
  lines dropped as new ones arrive.
- Appends are batched rather than one buffer transaction per line (accumulate on a tick, insert
  once).
- Auto-scroll only when the view is already scrolled to the bottom — a user who scrolled up to read
  something must not be yanked back down.
- Gutter labels line up with their content lines regardless of wrapping.
- Verified by running a deep search and confirming memory stays flat and the log stays responsive.

## Scope boundary

- Move Log tab (`appendMoveLog`) is a separate concern — see NAV-01 for its rebuild cost.
- Do not change the tag/color scheme (`src/ui/bottom_panel.cpp:28-51`); it is fine as-is.

## Related

- RT-01 (analysis path throttling), NAV-01 (move log rebuilt per move)
