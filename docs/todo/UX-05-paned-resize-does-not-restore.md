# UX-05 — `Gtk::Paned` divider position doesn't rescale after shrink-then-grow

**Status:** open
**Area:** main window layout
**Priority:** P3
**Source:** discovered during UX-04's window-resize testing, 2026-08-21

## Context

While testing UX-04's "window resize behaviour at both ends", shrinking the main window down to a
small size (tested at 500×400 under a headless Xvfb display) squeezed the board pane down to a
sliver — no crash, GTK simply allocates what space remains, consistent with
`set_shrink_start_child(false)` / `set_shrink_end_child(false)` on `mainHPaned_`/`mainVPaned_`
(`main_window.cpp:261,263,270,272`).

The actual bug: growing the window back up to its original size (1280×800) does **not** restore the
board pane's size. `Gtk::Paned` stores its divider as an absolute pixel offset
(`main_window.cpp:264,273` set it via `set_position()`), so once the window shrinks and the divider
gets clamped down, the divider stays at that small absolute position even after the window regains
its original size — the board stays squeezed until the user manually drags the divider back out.

This is a `main_window.cpp` Paned-configuration concern, distinct from UX-04's board-geometry
rendering/hit-testing fixes, so it was filed separately rather than bundled into that fix.

## Things to check

- Whether `Gtk::Paned::set_position()` should be re-applied (or recomputed as a fraction of current
  width) on every window resize, not just at startup/profile-switch time.
- Whether the same issue affects `mainVPaned_` (vertical split) as well as `mainHPaned_`.
- Whether there's a simpler fix: store the divider position as a fraction of the pane's own width
  and reassert it in a `signal_size_allocate`/resize handler, vs. some other GTK4-idiomatic pattern
  for proportional paned dividers.

## Acceptance criteria

- Shrinking the window and then growing it back restores (or at least allows restoring) a
  reasonable board pane size, rather than requiring the user to manually drag the divider.

## Related

- UX-04 (where this was found; board-geometry fixes there are unrelated to this Paned-position bug)
