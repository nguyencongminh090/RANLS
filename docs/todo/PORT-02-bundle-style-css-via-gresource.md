# PORT-02 — Bundle style.css into the binary via GResource

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/application.cpp`, `CMakeLists.txt`, new `src/resources/ranls.gresource.xml`
**Priority:** P3 — cosmetic-only failure mode, but the current fallback bakes a developer path into the binary
**Source:** Platform-dependency audit 2026-09-06 — see [docs/audit/2026-09-06-platform-dependency-audit.md](../audit/2026-09-06-platform-dependency-audit.md). Split from PORT-01 (Tier 2) per the 2026-09-06 tiering decision.
**Design:** none — standard GTK resource-bundling pattern, scoped directly.
**Depends on / relates to:** PORT-01, PORT-03; audit 2026-09-06.

## Problem

[src/application.cpp:34-38](../../src/application.cpp) loads `style.css` from the launch
directory, and if that fails falls back to
`std::filesystem::path(__FILE__).parent_path() / "resources" / "style.css"` — i.e. the build
machine's absolute source path (`/run/media/ngmint/...`), compiled into the binary. On any
install where CWD ≠ install dir (a shortcut, a packaged app, any non-Linux release) both
lookups miss and the app runs with no custom styling, emitting
`[RANLS] Warning: style.css not found.`

The `CMakeLists.txt` `POST_BUILD` step that copies `style.css` next to the binary
(`CMakeLists.txt` ~line 114) is a build-tree convenience only; `install(FILES ... DESTINATION .)`
puts it next to the installed binary but still relies on CWD at launch.

## Scope (in order)

1. Add `src/resources/ranls.gresource.xml` listing `style.css` (prefix `/org/ranls/`).
2. `CMakeLists.txt` — compile it with `glib-compile-resources` (or gtkmm's
   `gnome_compile_resources` / a custom command) into a generated `.c` (or `.o`) linked into
   `ranls-gui`. Keep the generated file out of the source tree (build dir only).
3. `src/application.cpp` — load via `Gtk::CssProvider::load_from_resource("/org/ranls/style.css")`;
   drop the `__FILE__` fallback entirely. Keep a single warning if the resource load itself
   fails (should be impossible once linked).
4. Remove the `POST_BUILD` copy and the `install(FILES style.css ...)` line — the stylesheet
   is in the binary now. (Leave `src/resources/style.css` in the tree as the source of truth.)

## Acceptance criteria

- `ranls-gui` launched from any directory shows the custom styling; no
  `style.css not found` warning.
- No absolute build-host path remains in the binary (`strings ranls-gui | grep /run/media`
  is empty).
- Linux build + `ctest` unchanged.

## Scope boundary

- `style.css` only — do not move other assets (there are none bundled yet) or restructure
  `src/resources/`.
- No behaviour change to what the stylesheet contains.
- Not adding an `.desktop` file / icon resources here (separate, NAME-01 follow-up territory).
