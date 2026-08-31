# UI-11 — Rewrite the About window (custom layout, more info, correct app name)

**Status:** 🔲 ACTIVE (Sprint 8, pulled 2026-08-31)
**Area:** About dialog (`MainWindow::onAbout()` — `src/main_window.cpp:860`)
**Priority:** P3
**Source:** filed 2026-08-31 from a user request (via `/prompt-architect`)

## Problem

`MainWindow::onAbout()` builds a bare `Gtk::AboutDialog` and sets only three
fields:

```cpp
dialog->set_program_name("Rapfi Analysis");
dialog->set_version(APP_VERSION);              // REL-02, single-sourced — keep
dialog->set_comments("Professional Gomoku Analysis Tool");
```

Three shortcomings:
1. The application name shown is `"Rapfi Analysis"` — the app's name is **RANLS**
   (confirmed with user 2026-08-31).
2. It carries almost no information — no developer credit, no build/tech info, no
   links, no protocol/engine context.
3. It uses the stock `Gtk::AboutDialog` layout; the user wants a deliberate,
   custom layout.

## Scope

Replace the stock dialog with a **custom** dialog (`Gtk::Window` or `Gtk::Dialog`
with a hand-built `Gtk::Box`/`Gtk::Grid`), laid out deliberately:

- Two-pane arrangement: application logo/icon on the left, information column on
  the right.
- Right column top: **RANLS** as a prominent heading, version string
  (`APP_VERSION`) beside or beneath it.
- Grouped / aligned below (label–value grid, or short titled sections):
  - **Developer credit:** `Developer: Nguyen Minh` + a copyright line with year.
  - **Tech / build info:** GTK / gtkmm version, Cairo version if readily
    available, build date, git commit hash if obtainable at build time, license.
  - **Links & protocol:** project / repository URL, engine-protocol reference,
    and the supported-engine list (Rapfi, Yixin, and Gomocup/Yixin-protocol
    compatible engines). URLs rendered clickable (`Gtk::LinkButton` or a
    linkified `Gtk::Label`).
- Single "Close" button, bottom-right.
- Correct in both light and dark themes; reasonably sized; resizable is fine.

Implementation notes:
- Prefer a dedicated `AboutDialog` class under `src/ui/` over inlining a large
  layout in `main_window.cpp` (see `software-architecture` skill). Follow the
  `gtk-ui-design` skill for construction patterns.
- Keep `APP_VERSION` single-sourced from CMake `project(VERSION)` (REL-02) — do
  **not** hard-code a version. Add any new build-time values (build date, git
  hash) through CMake / `configure_file` the same way `version.h` is generated.
- Keep the existing heap + `set_hide_on_close` + delete-on-`signal_hide` dialog
  lifetime convention (CLEAN-01) unless the skill dictates a better GTK4-idiomatic
  approach — if changed, say why.

## Scope boundary

- **About window only.** No drive-by refactor of other dialogs.
- App-wide rename `"Rapfi Analysis"` → `"RANLS"` (window title `set_title` at
  `src/main_window.cpp:114`, `set_program_name`, `style.css` header comment,
  possibly the GTK application id and a future `.desktop` file) is **out of scope
  here** — the About window shows `RANLS`, but a consistent global rename is
  filed separately (see Related). Confirm with the user whether to fold it in.
- Do not add an argument-parsing or templating library.

## Acceptance criteria

- Help → About opens a custom-laid-out dialog (not stock `Gtk::AboutDialog`).
- It shows: name `RANLS`, version (== `APP_VERSION` == CMake `PROJECT_VERSION`),
  `Developer: Nguyen Minh` + copyright, tech/build info block, and clickable
  links + supported-engine/protocol info.
- Renders correctly in light and dark themes.
- Verified by building and launching the real app (`run` skill); reported.
- `rapfi-gui-ui-tests` (links gtkmm, asserts the rendered widget tree) covers the
  dialog's presence and key labels if that target can reasonably reach it;
  otherwise say so explicitly.

## Related

- REL-02 — single-sourced the version string into `onAbout()`; this task must not
  regress that.
- CLEAN-01 — the delete-on-hide dialog lifetime pattern to follow.
- (to file) NAME-01 — consistent app-wide rename to `RANLS`.
