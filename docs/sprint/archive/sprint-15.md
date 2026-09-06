# Sprint 15 (closed 2026-09-07)

**Goal:** Tier-2 Windows portability: bundled assets + MSVC/CI harness
**Dates:** 2026-09-06 to 2026-09-07.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| PORT-02 | Bundle `style.css` into the binary via GResource; drop the `__FILE__` build-host-path fallback | ✅ DONE |
| PORT-03 | Native MSVC flags + `WIN32` subsystem; portable `mock_engine` test target; Win32 Job Object; Windows CI | ✅ DONE (scope item 4, the Windows CI job, deferred as an optional follow-up) |
| UI-15 | Move Log sticky-bottom races the GTK4 kinetic-scroll animation (the recurring "ui12 scroll flake") | ✅ FIXED |

UI-15 was pulled into Active mid-sprint (2026-09-06), after PORT-02/PORT-03 had landed — a
`/systematic-debugging`-diagnosed GTK4 scroll defect, unrelated to the Windows-portability goal.
Points not estimated (consistent with Sprints 3–14). Two follow-up fixes landed in the same window
outside a `CODE`: `test_anlz05_no_automove_action`'s long-mislabelled "environmental flake" was
traced to a PROTO-04 regression and fixed test-only; PORT-02's GResource was found to have **never
actually compiled in** (`project(... LANGUAGES CXX)` has no C compiler, so the generated
`ranls_gresource.c` was silently dropped from the link) — fixed by adding `C` to the project
languages and rewriting the `port02` guard to grep a sentinel that only lives in `style.css`.

## What shipped

- **PORT-02** (PR #25, squash `328490e`): `style.css` is bundled into the binary as a GResource.
  New `src/resources/ranls.gresource.xml` (prefix `/org/ranls/`); `CMakeLists.txt` locates
  `gio-2.0` + `glib-compile-resources` and compiles the bundle to `build/generated/ranls_gresource.c`
  (build-dir only), linked into `ranls-gui`; the `POST_BUILD` `style.css` copy and
  `install(FILES style.css …)` are removed. `application.cpp::loadStylesheet()` now calls
  `CssProvider::load_from_resource("/org/ranls/style.css")` in a `try/catch(const Glib::Error&)` and
  drops `<filesystem>`, the CWD probe, and the `__FILE__` build-host-path fallback that baked an
  absolute build-host path into the executable and lost all styling on any non-CWD launch. New
  `port02-style-css-bundled` CTest guards the bundle. **Follow-up fix** (`docs/fix-log/2026-09-06-port02-gresource-never-compiled-cxx-only-project.md`):
  the bundle never linked because the project declared `LANGUAGES CXX` only — fixed with
  `LANGUAGES CXX C` + `-w` on the generated TU, and the guard rewritten to a `@ranls-gresource-marker`
  sentinel. Live from-any-directory styled-launch smoke still needs a human (no display server on the
  build host).

- **PORT-03** (PR #26, squash `38ce332`): native MSVC toolchain + portable test harness + Windows
  engine cleanup. `CMakeLists.txt`: `if(MSVC) /W4 /permissive- else() -Wall -Wextra -Wpedantic`,
  `WIN32` on `add_executable(ranls-gui …)`, gtkmm/gio discovery gated `if(MSVC)`→`find_package(PkgConfig)`
  shim (else branch unchanged) + a README "Building on Windows" section. New `tests/mock_engine/`
  (one source, targets `mock_engine` cat-like and `mock_engine_quit` true-like; paths passed as
  `MOCK_ENGINE_PATH` / `MOCK_ENGINE_QUIT_PATH` compile defs); 7 test suites migrated off `/bin/cat` +
  `/bin/true` with assertions untouched, `test_eng03_pdeathsig.cpp` left Linux-guarded.
  `engine_process.cpp`: `#elif defined(_WIN32)` Win32 Job Object
  (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`) — the Linux `PR_SET_PDEATHSIG` path and the macOS/other
  `Gio::Subprocess` fallback are untouched (macOS gap documented). Also fixed a pre-existing
  `BottomPanel` deferred-scroll UAF the mock-engine migration made deterministic (bare `this` in 4
  idle slots → `sigc::track_obj`-bound, + regression case). Scope item 4 (Windows GitHub Actions
  job) deferred as an optional follow-up. Native MSVC compile, no-console-window, and the
  `taskkill /f` orphan check remain a required human Windows smoke test.

- **UI-15** (PR #29, squash `28d1513`): the recurring "ui12 scroll flake" root cause —
  `BottomPanel::scrollMoveLogToEnd()` had diverged from `scrollEngineLogToBottom()` and still called
  only `scroll_to(mark)`, never UI-14's direct vadjustment snap, so a fast burst append left the Move
  Log mid-kinetic-animation and a few px short of the newest move (load-dependent test failure; a
  real gap on fast game replay/paste). Fix (`src/ui/bottom_panel.cpp` only): after both the immediate
  `scroll_to` and the `sigc::track_obj` idle re-issue, snap `scrolledMoveLog_`'s vadjustment directly
  to `upper - page_size` when it is behind. `programmaticScroll_` / `stickToBottom_` semantics, the
  Engine Log path, the RT-02 buffer cap and the UI-05 gutter are untouched.
  `test_ui12_move_log_scroll_target`'s fixed `pump(300)` is converted to a condition-based wait and
  kept as the permanent regression guard. `test_ui12` 20/20 under `nproc`-wide CPU load + 5/5 under
  ctest load; `ranls-gui-ui-tests` 30/30, `ranls-gui-tests` 209/209, `ctest` 4/4.

## Lessons

- The build host has no engine binary, no display server, and cannot compile the Windows/macOS
  toolchain branches. Every non-Linux acceptance criterion this sprint (a from-any-dir styled launch,
  native MSVC compile, `taskkill /f` orphan check, MSYS2 MINGW64 rebuild) is still an outstanding
  **human** step. PORT-03's own Windows CI job (deferred scope item 4) is the structural fix for this
  recurring gap — carry it forward.
- A green guard can still be a false positive. `port02-style-css-bundled` passed for a full release
  cycle while the GResource was never compiled in, because the guard grepped for
  `org/ranls/style.css` — also a plain string literal in `application.cpp`. A build-artifact guard
  must key on something that can only come from the artifact it is guarding (the rewrite greps a
  sentinel that lives solely inside `style.css`).
- A "pre-existing environmental flake" label, once attached, propagated unchallenged through ~6
  fix-log entries and hid a real PROTO-04 regression whose green→red transition was pinpointable by
  bisect. Re-verify a "known flake" against the commit it supposedly always failed at, not against a
  baseline already past it.
- Keep divergent copies of the same mechanism in sync. `scrollMoveLogToEnd()` vs
  `scrollEngineLogToBottom()` drifted apart at UI-14 and cost UI-15 to reconcile — the Move Log
  simply never received the fix the Engine Log did.

## Rolled over to Backlog

Nothing rolled over — all committed items finished. Outstanding **human verification** steps
(recorded in the respective fix-logs, not rolled-over scope): a from-any-directory styled-launch
smoke for PORT-02; native MSVC compile + no-console-window + `taskkill /f` orphan check for PORT-03;
the still-pending PORT-01 MSYS2 MINGW64 build+launch+save-`.rdb`+pick-engine smoke.

## Next sprint

Sprint 16 — not yet opened. Run `/sprint open 16 "<goal>" <CODE...>` to commit Backlog items and
start it. Release `v0.4.2` cut at this close (PATCH — PORT-02/PORT-03 are portability groundwork with
no new user-visible feature on the shipping Linux platform, and UI-15 is a scroll fix).
