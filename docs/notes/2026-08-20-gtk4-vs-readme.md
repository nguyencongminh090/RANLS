# 2026-08-20 — README says GTK3, build actually targets GTK4

## Finding

`README.md` claims "The code has been migrated to GTK3." `CMakeLists.txt:15` requires
`gtkmm-4.0` (`pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)`), and every UI file
in `src/ui/` uses GTK4-only APIs (`Gtk::ColumnView`, `Gio::ListStore`, `DrawingArea::set_draw_func`
with the 3-arg callback signature). The README is stale — the actual target is GTK4, confirmed
against the build config, not just headers.

Surfaced while writing `.claude/skills/gtk-ui-design/SKILL.md` (2026-08-20), which documents GTK4
patterns per the user's explicit confirmation that GTK4 is the intended target (mid-migration).

## Not done yet

Didn't correct `README.md` itself — out of scope for the skills-writing task and not asked for. Worth
a small `docs/fix-log.md` entry (doc-accuracy fix, no code change) if/when addressed.
