# UI-11 — Rewrite the About window (custom layout, more info, correct app name)

**Status:** ✅ DONE (Sprint 8, implemented 2026-08-31 on branch `ui-11/about-window-rewrite`)

## Resolution

New `AboutDialog` class (`src/ui/about_dialog.{h,cpp}`) replaces the stock
`Gtk::AboutDialog`. `MainWindow::onAbout()` reduced to the CLEAN-01 heap +
`set_hide_on_close(true)` + `signal_hide → delete` lifetime, `set_visible(true)`.

Layout: horizontal root box — left `Gtk::Image` (themed stock icon
`applications-games-symbolic`, pixel size 96; no binary asset committed, per the
task boundary), right vertical info column: `RANLS` heading (`.title-1` style
class), `Version <APP_VERSION>` (`.dim-label`), tagline, then three titled
sub-sections — Developer (`Developer: Nguyen Minh` + `Copyright © 2026 Nguyen
Minh`), Tech / build info (`Gtk::Grid` of GTK / gtkmm / Cairo runtime versions,
build date, git commit, license), Links & protocol (repository + Yixin/Gomocup
protocol as `<a href>` markup labels, supported-engine list). Single bottom-right
`Close` button; Esc also closes.

Build-time values: new `src/build_info.h.in` → `build/generated/build_info.h`
via `configure_file`, mirroring `version.h`. `APP_BUILD_DATE` from
`string(TIMESTAMP … UTC)`, `APP_GIT_COMMIT` from `git rev-parse --short HEAD` at
configure time, guarded to fall back to `"unknown"` in a no-`.git` build.
`APP_VERSION` still single-sourced from CMake `project(VERSION)` (REL-02) — no
hard-coded version anywhere.

`src/ui/about_dialog.cpp` wired into the `rapfi-gui` target and the
`rapfi-gui-ui-tests` target (with `${CMAKE_BINARY_DIR}/generated` added to that
target's include path).

### Verification (2026-08-31)

- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4` — clean, no new warnings.
- `cd build && ctest --output-on-failure` — 3/3 passed, incl. `rel02-version-single-source` and `rapfi-gui-ui-tests`.
- New `tests/test_ui11_about_dialog.cpp` (3 real-widget cases in `rapfi-gui-ui-tests`): asserts the
  rendered widget tree contains `RANLS`, `Developer: Nguyen Minh`, `APP_VERSION`, the build-info and
  links blocks; asserts modal + transient-for parent; asserts the dialog builds under both the
  `Adwaita` and `Adwaita-dark` GTK theme without crashing. A display server was available in the
  worktree so the cases ran for real (not self-skipped).
- Full manual light/dark *visual* confirmation of the running GTK app (open Help → About, click the
  links) is left for the orchestrator via the `run` skill.

### Out of scope (per task boundary)

- App-wide `"Rapfi Analysis" → "RANLS"` rename (window title `src/main_window.cpp:114`, `style.css`,
  GTK application id) — filed separately as NAME-01. The name `RANLS` appears only inside the About
  dialog's own text.
- No new binary logo asset committed — themed stock icon used as the fallback.

---

<details><summary>Original task</summary>

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

</details>
