# RT-03 — PVView full rebuild destroys hover, breaking the board PV preview

**Status:** ✅ DONE — see [fix-log](../fix-log/2026-08-21-rt-03-pvview-rebuild-breaks-hover.md)
**Area:** PV view / board overlay interaction
**Priority:** P0
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

`PVView::update()` (`src/ui/pv_view.cpp:41-95`) removes **every** row and recreates it, along with
a fresh `Gtk::EventControllerMotion` per row, on every analysis update — which during a search is
many times per second (see RT-01).

This breaks the PV-hover → board-ghost-stone feature:

1. The user hovers a PV row; `signal_pv_hovered` fires and ghost stones appear on the board
   (`src/main_window.cpp:314-318`).
2. The next analysis update destroys that row. Its motion controller emits `leave`, which the
   `listBox_`-level handler turns into `signal_pv_hover_left` (`src/ui/pv_view.cpp:35-37`) →
   `pvPreview.clear()` (`src/main_window.cpp:320-323`) → ghost stones vanish.
3. The replacement row is a new widget under a stationary pointer, so it receives no `enter` event
   until the mouse physically moves again.

Net effect: while the engine is analyzing — the only time PV hover is useful — the ghost-stone
preview flickers and mostly stays off.

## Why it matters

Mapping a PV back onto the board is the PV view's stated job (see the `ui-ux-review` surface
table). It currently only works reliably when the engine is idle.

## Acceptance criteria

- PVView updates in place: reuse existing row widgets, update their labels, add/remove rows only
  when the PV *count* changes. Alternatively migrate to `Gio::ListStore` + `ColumnView`/`ListView`
  with a bind-based factory, per the `gtk-ui-design` skill's data-view pattern.
- Hovering a PV row keeps the board ghost stones stable across analysis updates.
- `signal_pv_hover_left` fires only on a genuine pointer-leave, never as a side effect of a data
  refresh.

## Scope boundary

- Row *content* and formatting (`evalText`, `coordStr`, the 12-move truncation at
  `src/ui/pv_view.cpp:73-77`) stay as they are.
- Does not cover the empty-state message — that is UX-01.
- Does not cover stale PV data after a move — that is STATE-01.

## Related

- RT-01 (update rate), STATE-01 (stale PV content), STATE-03 (empty PVLine rows), UX-01 (empty state)
