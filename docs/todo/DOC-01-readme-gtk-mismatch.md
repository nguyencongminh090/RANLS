# DOC-01 — README claims GTK3, project actually targets GTK4

**Status:** ✅ DONE
**Area:** documentation
**Priority:** P4
**Source:** `docs/notes/2026-08-20-gtk4-vs-readme.md`, re-surfaced during a 2026-08-30 leftover-task sweep

## Context

`README.md` states "The code has been migrated to GTK3." This is stale: `CMakeLists.txt:15`
requires `gtkmm-4.0` (`pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)`), and every UI
file under `src/ui/` uses GTK4-only APIs (`Gtk::ColumnView`, `Gio::ListStore`,
`DrawingArea::set_draw_func`'s 3-arg signature). Confirmed against the build config, not just
headers — this isn't a doc lag on an in-progress migration, the migration is done and the doc never
caught up.

## Scope boundary

- Doc-only change: correct the GTK3 → GTK4 claim in `README.md` (and anywhere else it's repeated,
  e.g. a build-prerequisites section listing `libgtkmm-3.0-dev` instead of `libgtkmm-4.0-dev`).
- No code change. No need to touch `CMakeLists.txt` or any `src/` file.

## Acceptance criteria

- `README.md` accurately states GTK4/gtkmm-4.0 as the target, matching `CMakeLists.txt`.

## Related

- `docs/notes/2026-08-20-gtk4-vs-readme.md` — original finding, deferred as out of scope for the
  skills-writing task it was found during.
