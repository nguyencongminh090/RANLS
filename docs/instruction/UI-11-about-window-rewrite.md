# UI-11 — about-window-rewrite (execution guidance)

## Approach

1. New class `AboutDialog` in `src/ui/about_dialog.{h,cpp}` (own the layout there,
   not in `main_window.cpp`). `MainWindow::onAbout()` shrinks to: construct, set
   transient/modal, apply the existing heap + `set_hide_on_close(true)` +
   `signal_hide → delete` lifetime (CLEAN-01), `set_visible(true)`.
2. Layout: horizontal root `Gtk::Box` — left `Gtk::Image` (logo/icon), right
   `Gtk::Box`(VERTICAL). Right side: heading label (`RANLS`, large via a CSS
   class or Pango attributes) + version; then a `Gtk::Grid` of label/value rows
   or a few titled sub-sections for the three info blocks; `Close` button in a
   bottom-right-aligned box.
3. Build-time values via CMake, mirroring `src/version.h.in`:
   - Extend the generated header (or a sibling `build_info.h.in`) with
     `APP_BUILD_DATE` and `APP_GIT_COMMIT`. Get the commit at configure time with
     `execute_process(COMMAND git rev-parse --short HEAD ...)`, guarded so a
     tarball build with no `.git` still configures (fall back to `"unknown"`).
   - GTK / gtkmm / Cairo versions: prefer the runtime macros
     (`GTK_MAJOR_VERSION` etc. / `gtkmm` version macros) — no new CMake plumbing.
4. Links: `Gtk::LinkButton`, or `Gtk::Label` with `set_use_markup(true)` and an
   `<a href>` — pick whichever the `gtk-ui-design` skill favours for GTK4.

## Pitfalls / boundaries

- Do **not** hard-code the version — reuse `APP_VERSION` (REL-02). The
  `rel02-version-single-source` test must stay green.
- Do **not** do the app-wide `Rapfi Analysis → RANLS` rename here (window title,
  `style.css`, application id). About window text only. Confirm with the user if
  they want it folded in; otherwise file NAME-01.
- Theme: verify light + dark. Use existing CSS classes / `dim-label` rather than
  hard-coded colours.
- Keep the dialog self-contained — no new dependency on `GameState` / engine
  state; it is purely static app metadata.

## Verification

- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4`
- `cd build && ctest --output-on-failure` — full suite green, incl.
  `rel02-version-single-source`.
- Launch via the `run` skill; open Help → About; confirm every field, clickable
  links, and correct light/dark rendering. Report what was seen.
- Add/extend a `rapfi-gui-ui-tests` case for the dialog if reachable; else note why not.

[detail is this file]
