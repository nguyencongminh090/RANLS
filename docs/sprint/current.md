# Current sprint

## Sprint 14

**Goal:** Backlog tooling fix + Tier-1 Windows portability

**Dates:** 2026-09-06 to — (open — no fixed end date set yet)

**Dependency graph:**

- **TOOL-03** — repo tooling only (`scripts/check-task-structure.js` and/or the ANLZ-03 index
  lines in `TODO.md` + `instruction.md`). `node scripts/check-task-structure.js` still exits 1 on
  the `⛔`-marked, non-canonical ANLZ-03 lines (TOOL-02 was scoped to `🔲`/`🚧` only). **Small
  decision to make first:** (1) teach the script the `⛔`/SUPERSEDED shape, or (2) normalise the
  ANLZ-03 lines to canonical form + add just `⛔` to the alternation — detail file leans toward (2)
  (keeps the regexes strict). Must **not** touch `check-tracking-sync.js` or re-litigate the
  ANLZ-03 supersession itself. No `/systematic-debugging` needed (known cause). Standalone Node
  script, no test harness — verified by the acceptance-criteria runs. Independent of PORT-01.
- **PORT-01** — three isolated `#ifdef` guards, no shared surface: `#if !defined(_WIN32)` around
  `<unistd.h>` + a Windows executable-extension check replacing `access(X_OK)` in
  `src/ui/settings_dialog.cpp`; `::_commit(::_fileno(f))` alongside the POSIX `::fsync` in
  `src/model/rdb/rdb_container.cpp` (so `.rdb` saves are crash-durable on Windows); `GetModuleFileNameW`
  / `_NSGetExecutablePath` branches in `executableDir()` in `src/model/settings_storage.cpp`. Must
  **not** change any Linux code path, add a settings location (portable-app model stays, user
  decision 2026-09-06), touch the `#ifdef __linux__` `PR_SET_PDEATHSIG` block (that is PORT-03), or
  do GResource work (PORT-02). The new `_WIN32` / `__APPLE__` branches cannot be compiled or run on
  the Linux build host — an MSYS2 MINGW64 build + launch + save-a-`.rdb` + pick-an-engine-path
  smoke is a required human step before DONE; the fix-log records the per-guard correctness
  reasoning. Independent of TOOL-03. Design fully resolved in
  `docs/todo/PORT-01-cross-platform-build-and-runtime.md` (audit `docs/audit/2026-09-06-platform-dependency-audit.md`).

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| TOOL-03 | `check-task-structure.js` trips on `⛔` SUPERSEDED ANLZ-03 index lines | — | — | ✅ Done (PR #23, squash `69e10e0`) |
| PORT-01 | Tier-1 guarded Windows fixes: `unistd.h` guard + exec-extension check, `_commit()`, `GetModuleFileNameW` | — | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–13).

**Lessons carried in from Sprint 13:**

- The build host has no engine binary and no display server, and cannot compile the Windows/macOS
  toolchain branches either — live-engine, display-dependent, and non-Linux-toolchain behaviour
  cannot be verified here. PORT-01 leans on this hard: its `_WIN32` / `__APPLE__` guards get a
  documented human MSYS2 smoke step, not a claimed pass.
- A hand-rolled strict handler for a small, security-sensitive surface (PROTO-03's `.ptc` parser),
  fully covered by malformed-input tests, kept the trust boundary smaller than pulling in a
  library — relevant if TOOL-03 goes the "teach the script a new shape" route (prefer the
  narrower line-normalisation instead).

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
