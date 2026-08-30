# UI-05 — Engine Log: put the direction tag in its own gutter column, not the copyable text

**Status:** ✅ DONE — see [fix-log](../fix-log/2026-08-30-ui05-engine-log-gutter-column.md)

Implemented as a payload-only `Gtk::TextView` plus a sibling fixed-width
`Gtk::DrawingArea` gutter (`gutterArea_`, CSS class `engine-gutter`) that paints
each buffer line's tag at that line's live y-position (`get_line_yrange` minus the
scroll offset). Chosen over `Gtk::ColumnView` because the TextView keeps GTK's
native selection/copy (which then yields payload only, no `[SEND]`/`[MESSAGE]`),
and over the `Gtk::TextView` gutter-widget API because a sibling DrawingArea works
regardless of the ScrolledWindow/Viewport wrapping and can't be desynced — its
positions come straight from the live text layout. The `EngineLogModel` (already
the RT-02 bounded/batched source of truth) also supplies each line's tag, so
buffer and gutter never disagree. RT-02 bounded-buffer trim and per-tick batching
are unchanged.

**Verification:** `./build.sh` clean; `ctest` all pass (added
`tests/test_ui05_gutter_clipboard.cpp` — 2 cases pinning that the string fed to
the buffer is the raw payload with no prefix). App launches and constructs the
panel without warnings from the new code. The gutter draw itself is pure GTK and
not headless-testable; noted in the test file.

---

**Status (original):** 🔲 BACKLOG
**Area:** bottom panel / Engine Log (`src/ui/bottom_panel.cpp`, `src/ui/engine_log_model.h`)
**Priority:** P2
**Source:** filed 2026-08-30 from a UI review session

## Problem

Each engine-log line is rendered as one run of text beginning with `[SEND]` / `[MESSAGE]` /
`[OUTPUT]` / … . When the user selects lines to copy (e.g. to share a protocol trace), the
`[SEND]` / `[MESSAGE]` prefixes come along with the payload and have to be hand-stripped.

The user wants the direction/category shown in a **separate fixed-width gutter column** on the
left — analogous to a text editor's line-number gutter: visible, aligned, but *not* part of the
selectable/copyable text. Selecting rows and copying should yield only the raw engine text.

## Where to look

- `EngineLogLine` already separates `prefix` / `text` / `tag` (`src/ui/engine_log_model.h:21`), so
  the model is ready — this is a view change.
- Current rendering in `bottom_panel.cpp` (tag CSS classes `tagSend_` etc.). Need a two-column
  layout: a non-selectable `Gtk::Label`/gutter cell for the tag + a selectable text cell. Options:
  `Gtk::ColumnView` + `Gio::ListStore` (see gtk-ui-design skill), or a `Gtk::Grid`/`Gtk::TextView`
  with the gutter drawn separately.
- If `Gtk::TextView` stays: only the payload goes in the buffer; tags render in a
  `line-numbers`-style gutter via `Gtk::TextView` gutter API, or as a parallel widget.
- Keep the RT-02 bounded-buffer / no-desync-on-wrap guarantees.

## Acceptance criteria

- Direction tag column is fixed-width, left-aligned, visually distinct.
- Selecting one or more log rows and copying yields only the engine text — no `[SEND]`/`[MESSAGE]`.
- Buffer stays bounded (RT-02) and the gutter never desyncs from its line on wrap/resize.

## Related

- RT-02 (engine log unbounded + gutter desync on wrap) — same widget.
- gtk-ui-design skill (ColumnView + ListStore patterns).
