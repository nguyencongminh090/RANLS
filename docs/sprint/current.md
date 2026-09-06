# Current sprint

## Sprint 15

**Goal:** Tier-2 Windows portability: bundled assets + MSVC/CI harness

**Dates:** 2026-09-06 to — (open — no fixed end date set yet)

Sprint 14 closed 2026-09-06 (archived: `docs/sprint/archive/sprint-14.md`, release `v0.4.1`).

**Dependency graph:**

- **PORT-02** — asset bundling only. New `src/resources/ranls.gresource.xml` + a
  `glib-compile-resources` custom command in `CMakeLists.txt` linking a generated `.c`/`.o` into
  `ranls-gui`; `src/application.cpp` switches to
  `Gtk::CssProvider::load_from_resource("/org/ranls/style.css")` and **drops the `__FILE__`
  fallback entirely** (that is the bug — it bakes the build-host absolute path into the binary and
  loses all styling on any non-CWD launch). Remove the `POST_BUILD` `style.css` copy + the
  `install(FILES style.css …)` line. Must **not** move other assets, restructure `src/resources/`,
  change what `style.css` contains, or add a `.desktop`/icon resource (NAME-01 follow-up
  territory). No `/systematic-debugging` needed (cause known — audit 2026-09-06). Standard GTK
  resource-bundling pattern, scoped directly in
  `docs/todo/PORT-02-bundle-style-css-via-gresource.md`. Verification needs a from-any-directory
  launch (`strings ranls-gui | grep /run/media` empty) — a display-capable check, so a human
  MSYS2/Linux-desktop smoke step. Largely independent of PORT-03 (both touch `CMakeLists.txt` —
  land PORT-02 first, PORT-03 rebases).
- **PORT-03** — native MSVC toolchain + portable test harness + Windows engine cleanup. Gate
  resolved 2026-09-06 (native MSVC + Windows CI is a supported goal — see
  `docs/audit/2026-09-06-native-msvc-windows-ci-goal.md`). Four scoped pieces:
  (1) `if(MSVC) … /W4 /permissive- … else() … -Wall -Wextra -Wpedantic endif()` + `WIN32` on
  `add_executable(ranls-gui …)` + a documented vcpkg / gtkmm-on-MSVC discovery path (or
  `pkg_check_modules` behind `if(NOT MSVC)` with a `find_package` fallback);
  (2) new portable `tests/mock_engine/` CMake target replacing every `kFakeEngine = "/bin/cat"` /
  `"/bin/true"` across the 7 affected suites — **do not rewrite what the tests assert**, and keep
  `test_eng03_pdeathsig.cpp` Linux-guarded;
  (3) `#elif defined(_WIN32)` Win32 Job Object (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`) in
  `src/engine/engine_process.cpp` — this **extends ENG-03's Linux-only `PR_SET_PDEATHSIG` block**,
  must not regress it or the `Gio::Subprocess` fallback (macOS has no clean equivalent — leave +
  document);
  (4) optional Windows GitHub Actions job once 1–3 are in. Must **not** regress the Linux build or
  the MSYS2 MINGW64 path. `software-architecture` at the CMake-branch design point; the Job Object
  touches the engine-lifecycle seam ENG-01/ENG-03 own. Native MSVC + `taskkill /f`-leaves-no-orphan
  are non-Linux checks — a required human MSVC/Windows smoke step.
- **UI-15** — `src/ui/bottom_panel.cpp` only. Pulled in mid-sprint 2026-09-06 (unrelated to the
  Windows-portability goal — a `/systematic-debugging`-diagnosed GTK4 scroll defect). Mirror UI-14's
  `scrollEngineLogToBottom()` kinetic-animation snap (`vadj->set_value(upper - page_size)`) into
  `scrollMoveLogToEnd()`, and convert `test_ui12_move_log_scroll_target`'s fixed `pump(300)` to a
  condition-based wait. `/systematic-debugging` phases 1–3 already done (see the detail file). Must
  **not** touch `programmaticScroll_`/`stickToBottom_` semantics, the Engine Log path, the RT-02
  buffer cap, or the UI-05 gutter. `gtk-ui-design` at the `scroll_to`-vs-`vadjustment` seam.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| PORT-02 | Bundle `style.css` via GResource; drop the `__FILE__` build-host-path fallback | — | — | ✅ Done (PR #25, squash `328490e`) |
| PORT-03 | Native MSVC flags + `WIN32` subsystem; portable `mock_engine` test target; Win32 Job Object; Windows CI | PORT-02 (CMake ordering) | — | ✅ Done (PR #26, squash `38ce332`, 2026-09-06; Linux tier verified, Windows/MSVC smoke + CI job pending) |
| UI-15 | Move Log sticky-bottom races the GTK4 kinetic-scroll animation (the recurring "ui12 scroll flake") | — | — | ✅ Done (branch `ui-15/move-log-scroll-races-kinetic-animation`, 2026-09-06; `test_ui12` 20/20 under load + ctest, `ranls-gui-ui-tests` 30/30, `ranls-gui-tests` 209/209) |

Points not yet estimated (consistent with Sprints 3–14).

**Lessons carried in from Sprint 14:**

- The build host has no engine binary, no display server, and cannot compile the Windows/macOS
  toolchain branches. PORT-02 (a from-any-dir styled launch) and PORT-03 (native MSVC build,
  `taskkill /f` orphan check) both have acceptance criteria that can only be verified by a human on
  Windows / a Linux desktop — budget that verification loop into the sprint, do not tack it on at
  close. PORT-03's own CI job is the structural fix for this recurring gap.
- New marker/format or build conventions should update the tooling that must recognise them in the
  same change (the `⛔`/SUPERSEDED format needed TOOL-02 → TOOL-03 to catch up). If PORT-03 adds a
  CMake toolchain branch, update `build.sh` / `build_msys2.sh` / the README build notes with it.
- Prefer the narrowest change that satisfies the written acceptance criteria (TOOL-03's minimal
  regex widening; PORT-01's three isolated guards with zero Linux behaviour change).

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
