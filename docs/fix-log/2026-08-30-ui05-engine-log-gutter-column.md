# 2026-08-30 — Engine Log: direction tag moved into a non-copyable gutter column

**Task:** UI-05 — [todo](../todo/UI-05-engine-log-direction-gutter-column.md)
**Area:** `src/ui/bottom_panel.{h,cpp}`, `src/ui/engine_log_model.h`, `src/resources/style.css`
**Status:** ✅ DONE

## Prompt

The Engine Log rendered each line as one text run beginning with `[SEND]` /
`[MESSAGE]` / `[OUTPUT]` / …. Selecting rows to copy a protocol trace dragged the
prefixes along, so they had to be hand-stripped. UI-05 asks for the tag in a
separate fixed-width, left-aligned, visually-distinct gutter column (like a text
editor's line-number gutter) that is *not* part of the selectable/copyable text,
without regressing the RT-02 bounded-buffer / no-desync-on-wrap guarantees.

## Action

- **`bottom_panel.cpp` / `.h`** — replaced the single-TextView "tagged prefix
  inline in the buffer" scheme with:
  - `engineLogView_` buffer now holds **only** the raw engine payload
    (`logLineClipboardText(line)` == `line.text`), one line per logical engine
    line. GTK's native selection/copy therefore yields payload only.
  - New sibling `Gtk::DrawingArea gutterArea_` (CSS class `engine-gutter`),
    packed left of `scrolledEngineLog_` in a new horizontal `engineLogRow_`.
    `drawGutter()` walks the buffer lines visible in the viewport
    (`get_line_at_y` → `forward_line`, O(visible) not O(buffer)), and for each
    paints its `EngineLogModel` line's `prefix` in that kind's color at
    `y = get_line_yrange(...) - vadjustment->get_value()`. Positions come from
    the live text layout, so wrap/resize can't desync the tag from its line.
  - Fixed column width computed once from the widest tag (`[MESSAGE]`) and set
    via `set_size_request`.
  - Gutter repaint is driven by the scrolled window's vadjustment
    `value_changed` (scroll) and `changed` (resize/relayout) signals, plus an
    explicit `queue_draw()` at the end of `flushPending()` and in
    `clearEngineLog()`.
  - Removed the five `Gtk::TextTag` members + `tagForKind()`; replaced with a
    static `gutterColorForKind()` returning the same historical hex colors
    (RT-02 scope note: the color scheme itself must not change).
- **`engine_log_model.h`** — added `logLineClipboardText(const EngineLogLine&)`,
  the single definition of "what reaches the clipboard for this line" (the raw
  payload). `BottomPanel` inserts exactly this into the buffer.
- **`style.css`** — `.engine-gutter` already existed (leftover from the earlier
  dual-TextView design); reused as-is (border-right + padding).

RT-02 is untouched: `EngineLogModel` is still the bounded source of truth, the
buffer is still trimmed from the front by exactly `push()`'s reported drop count,
appends are still batched per 50 ms tick, and auto-scroll still only fires when
already at the bottom.

## Verification

- `./build.sh` — clean configure + build, no warnings from the changed code.
- `ctest --test-dir build_cmd` — 1/1 passed (whole `rapfi-gui-tests` binary).
- Added `tests/test_ui05_gutter_clipboard.cpp` (2 cases): the string fed into the
  TextView buffer for a line is the raw payload — never the bracketed tag — and a
  multi-line selection is payloads joined by `\n` only. This is the model-level
  contract the non-copyable-gutter behavior rests on; the gutter *drawing* is
  pure GTK and not exercisable headless (noted in the test file).
- App launches and constructs the Engine Log panel with no new warnings.
- Acceptance criteria:
  1. *Fixed-width, left-aligned, visually distinct* — `gutterArea_` width is
     computed once and pinned; `engine-gutter` CSS gives it a right border;
     labels drawn at a fixed x with per-kind color.
  2. *Copy yields only engine text* — buffer contains `line.text` exclusively;
     the tag lives only in Cairo-painted pixels with no text node. Covered by
     the new test at the model boundary.
  3. *Bounded + no desync* — RT-02 trim/batch code unchanged; gutter y-positions
     are read from the live layout every paint, so wrap/resize cannot misalign.
