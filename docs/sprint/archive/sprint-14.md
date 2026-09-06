# Sprint 14 (closed 2026-09-06)

**Goal:** Backlog tooling fix + Tier-1 Windows portability
**Dates:** 2026-09-06 to 2026-09-06.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| TOOL-03 | `check-task-structure.js` trips on the `⛔`/SUPERSEDED ANLZ-03 index lines | ✅ FIXED |
| PORT-01 | Tier-1 guarded Windows/macOS platform fixes (`unistd.h` guard + exec-extension check, `_commit()`, `GetModuleFileNameW`/`_NSGetExecutablePath`) | ✅ DONE |

No mid-sprint pulls. Points not estimated (consistent with Sprints 3–13).

## What shipped

- **TOOL-03** (PR #23, squash `69e10e0`): `BULLET_START_RE` / `TODO_LINE_RE` in
  `scripts/check-task-structure.js` gained `⛔\s*` in the marker alternation and made the
  ` SUPERSEDED` text between the `CODE` and the closing `**` optional — the non-canonical
  shape the ANLZ-03 supersession lines use (TOOL-02 was scoped to `🔲`/`🚧` only). Capture-group
  indices unchanged; orphan and duplicate-code detection unchanged. `node
  scripts/check-task-structure.js` now exits 0 and `check-tracking-sync.js --full` still exits 0.
  Standalone Node script, no test harness — verified by the acceptance-criteria runs. The
  `check-task-structure.js` script therefore recognises every marker the tracking files now use.

- **PORT-01** (PR #24, squash `4edb0a2`): Tier-1 Windows/macOS portability — three isolated
  `#ifdef` guards, no shared platform layer, **zero Linux behaviour change** (per the 2026-09-06
  platform-dependency audit). `settings_dialog.cpp/.h`: `<unistd.h>` now
  `#if !defined(_WIN32)`; `isValidEnginePath` keeps `access(path, X_OK)` byte-for-byte on POSIX
  and under `#if defined(_WIN32)` calls a new pure helper
  `settings_dialog_detail::hasExecutableExtension` (`.exe`/`.bat`/`.cmd`/`.com`, case-insensitive).
  `rdb_container.cpp`: new `#if defined(_WIN32)` branch calls `::_commit(::_fileno(f))` with the
  identical error handling as the untouched POSIX `::fsync` branch, closing the Windows `.rdb`
  atomic-save durability gap. `settings_storage.cpp`: `executableDir()` gains
  `#elif defined(_WIN32)` (`GetModuleFileNameW` buffer-grow loop) and `#elif defined(__APPLE__)`
  (`_NSGetExecutablePath` two-call sizing) — `__linux__` `/proc/self/exe` and the `current_path()`
  fallback unchanged; portable-app model kept. Out of scope, untouched: `PR_SET_PDEATHSIG`
  (PORT-03), GResource/`style.css` (PORT-02), `CMakeLists.txt` flags/`WIN32` subsystem (PORT-03).
  +`tests/test_port01_engine_path_check.cpp` (3 cases / 20 assertions in `ranls-gui-ui-tests`,
  pins `hasExecutableExtension` — runs on Linux since it is pure string logic). Linux: clean
  build, no new warnings; `ranls-gui-tests` 209/209 (2466 assertions), `rel02-version` pass,
  `ranls-gui-ui-tests` 28/29 — the lone failure `test_anlz05_no_automove_action` reproduces
  identically on a clean `main` (`c90b0fd`) build, Linux ctest result unchanged. The `_WIN32` /
  `__APPLE__` branches cannot be compiled or run on this build host; per-guard correctness
  reasoning is in the fix-log detail, and an **MSYS2 MINGW64 build + launch + save-a-`.rdb` +
  pick-an-engine-path smoke remains a required human step**.

## Lessons

- The build host has no engine binary, no display server, and cannot compile the Windows/macOS
  toolchain branches — live-engine, display-dependent, and non-Linux-toolchain behaviour cannot be
  verified here. PORT-01 leaned on this hard: its `_WIN32` / `__APPLE__` guards carry documented
  per-guard reasoning and an explicit human MSYS2 smoke step, not a claimed pass. This gap now
  compounds for PORT-02/03 (GResource linking, native MSVC flags, a Win32 Job Object) — Sprint 15
  needs the human MSYS2/MSVC verification loop budgeted in, not tacked on at close.
- A hand-rolled strict handler for a small surface kept the change narrow again (TOOL-03 chose the
  minimal regex-alternation widening over "teach the script a whole new shape"); prefer the
  narrowest change that satisfies the acceptance criteria.
- The `⛔`/SUPERSEDED format was invented ad hoc for ANLZ-03 and then needed a tooling follow-up
  (TOOL-02 → TOOL-03) to be recognised. New marker/format conventions should update
  `scripts/check-task-structure.js` in the same change that introduces them.

## Rolled over to Backlog

Nothing rolled over — both committed items finished. (PORT-01's MSYS2 MINGW64 smoke is an
outstanding **human verification** step recorded in its fix-log, not rolled-over scope.)

## Next sprint

Sprint 15 — goal "Tier-2 Windows portability: bundled assets + MSVC/CI harness" (PORT-02, PORT-03),
opened at this close. Native MSVC / Windows CI confirmed a real target 2026-09-06 (user decision),
un-gating PORT-03 — see `docs/audit/2026-09-06-native-msvc-windows-ci-goal.md`. Release `v0.4.1`
cut at this close (PATCH — TOOL-03 + PORT-01 are fix/polish with no new user-visible feature on the
shipping Linux platform).
