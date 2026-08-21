# 2026-08-21 — RT-03: PVView full rebuild destroys hover, breaking the board PV preview

**Task:** `docs/todo/RT-03-pvview-rebuild-breaks-hover.md` (Backlog, P0). Implemented directly on
explicit request rather than staying queued behind Sprint 1's committed order (`TEST-01` →
`PROTO-01` → `STATE-01` → `RT-01`, per `docs/sprint/current.md`); RT-03 was not part of that
committed set and this fix does not change the sprint plan. No `docs/instruction/RT-03-*.md` entry
exists — expected, per `instruction.md`'s convention (not every task has one).

## Problem

`PVView::update()` (`src/ui/pv_view.cpp`) destroyed every row widget and rebuilt it from scratch,
including a brand-new `Gtk::EventControllerMotion` per row, on every analysis update — many times per
second during a search (see RT-01). Consequences:

1. Hovering a PV row fires `signal_pv_hovered` → `main_window.cpp` shows board ghost stones.
2. The next analysis update destroyed that row widget. Its motion controller's destruction emitted a
   synthetic `leave`, which the list-level handler turned into `signal_pv_hover_left` →
   `pvPreview.clear()` → ghost stones vanish.
3. The replacement row is a new widget under a now-stationary pointer, so it gets no `enter` until
   the mouse physically moves again.

Net effect: the ghost-stone preview flickered and mostly failed to show during the only time it's
useful — while the engine is actively analyzing.

## Fix

`src/ui/pv_view.h`: added a `PVView::RowWidgets` struct (row `Gtk::Box*` + the four child
`Gtk::Label*`s + a cached `moves` vector) and a `std::vector<RowWidgets> rows_` member so row widgets
persist across `update()` calls instead of being recreated from `Gtk::ListBox::get_first_child()`
teardown each time.

`src/ui/pv_view.cpp`: `PVView::update()` now:
- Removes row widgets from the tail only while `rows_.size() > pvLines.size()` (shrink).
- Appends new row widgets (with a freshly attached `Gtk::EventControllerMotion`) only while
  `rows_.size() < pvLines.size()` (grow).
- For every index in range, updates the four labels' text in place via `set_text()` — no widget is
  touched otherwise.
- The `signal_enter()` lambda captures the row's fixed index and reads `rows_[i].moves` at hover
  time, not a value captured at row-construction time, so an already-hovering row whose PV content
  changes underneath it emits the current moves, not stale ones from when the row was created.

Row content formatting (`evalText`, `coordStr`, the 12-move truncation) and the list-level
`signal_leave()` → `signal_pv_hover_left` wiring (`src/ui/pv_view.cpp:35-37` before the edit) were
left untouched, per the todo file's scope boundary.

**Approach chosen vs. the todo file's alternative:** in-place row reuse inside the existing
`Gtk::ListBox`, not a migration to `Gio::ListStore` + `ColumnView`/`ListView`. It's the smaller
correct fix — same widget type, same CSS, a ~90-line change confined to `pv_view.h`/`.cpp` — and the
`gtk-ui-design` skill documents this codebase's `ColumnView` pattern (`TreeExplorer`) as "rebuild the
whole store every call," so adopting it here would still require layering the same "only touch what
changed" logic on top via a bind factory, at higher cost for no extra correctness.

## Verification

- `bash build.sh` (Ninja/Release): compiles cleanly. Pre-existing unused-function warnings in
  `src/engine/gomocup_protocol.cpp` are unrelated and unchanged by this fix.
- `RUN_TESTS=1 ./build.sh`: run in the isolated implementation worktree, `tests/` did not yet exist
  there (a stale copy — `TEST-01`'s harness had landed on `main` but not yet been merged into this
  branch); re-run against `main` post-merge, `50/50` pre-existing cases still pass unaffected by
  this change.
- No new automated regression test was added: `PVView` only
  constructs through live GTK4 widget API calls (`Gtk::make_managed`, `Gtk::EventControllerMotion`)
  with no display-free/headless construction path in this codebase, and there is no test harness
  present in this checkout to add one to. Stated explicitly rather than skipped silently, per
  `CLAUDE.md`'s bug-fix workflow rule.
- Correctness verified by code reading instead: (a) the shrink/grow `while` loops are the only places
  that call `listBox_.remove`/`listBox_.append`/`add_controller`, and they're gated strictly on
  `rows_.size()` vs. the new PV count — so a same-count refresh touches no widget lifetime, only
  `set_text`; (b) each row's `Gtk::EventControllerMotion` is constructed exactly once, inside the grow
  loop, never inside the per-row content-refresh loop.
- **Not verified:** live interactive hover behavior. No display server is available in this
  sandboxed environment to run the app and hover a PV row while an engine analyzes, so the actual
  "ghost stones stay stable" UX was not click-tested — only reasoned from the code path described
  above. Flagging this honestly rather than claiming interactive verification that wasn't performed.

## Status

Marked ✅ in `TODO.md`'s Backlog line and in `docs/todo/RT-03-pvview-rebuild-breaks-hover.md`'s
`Status` field — the code-level fix is complete and matches all three acceptance criteria in the
todo file. The one open gap is interactive/live verification, which this environment cannot perform;
noted above rather than glossed over.
