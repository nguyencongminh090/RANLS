# ARCH-04 — Characterization tests for analyze / auto-move behavior

**Status:** ✅ DONE
**Area:** `tests/` only (no `src/` change)
**Priority:** P2
**Source:** `features/extract-mainwindow-orchestration/planning.md` "Characterization first" — 2026-10-06; split from ARCH-02
**Design:** [features/extract-mainwindow-orchestration/](../../features/extract-mainwindow-orchestration/planning.md) — RESOLVED 2026-10-06
**Depends on / relates to:** blocks ARCH-02

## Problem

ARCH-02 moves the analyze-restart / auto-move / ENG-02 decision logic out of `MainWindow`. Those rules were built up across ENG-02, ANLZ-01/05/06/07 and UI-06, and several orderings are subtle. Before moving anything, current behaviour must be pinned by tests.

## Scope (in order)

1. Audit existing coverage: `test_anlz01_*`, `test_anlz05_*`, `test_anlz06_*`, `test_anlz07_*`, `test_eng01/02/03_*`, `test_rt01_throttle`.
2. Add tests, against the current `MainWindow` (via the existing friend probes), for any gap in: auto-move only on the engine's turn and Idle; auto-move suppressed while Analyze Mode is on (ANLZ-05); a burst of `signal_board_changed` causes one restart; the force-latch is not downgraded by a later non-forced call (ANLZ-07); Analyze Mode off calls `stopAnalysis()` without touching `enginePlays`; the ENG-02 revert is not persisted.
3. Report which cases were already covered and which are new.

## Scope boundary

- Tests only: no `src/` edits, no behaviour change, no new test seams in production code.
- Do not retire or rewrite existing probes.

## Reasoning

Characterize before refactoring so "no behaviour change" is a test result. Rejected: writing the coordinator tests only after extraction (they would encode the new code, not the old behaviour).

## Knowledge

- `features/extract-mainwindow-orchestration/planning.md`
- `docs/fix-log/2026-09-04-anlz05-analyze-mode-no-automove-mid-search-click.md`
- Lesson from Sprint 20: capture the golden from pre-refactor code (ARCH-03).

## Acceptance criteria

- Every bullet in Scope step 2 is covered by a passing test, new or pre-existing, and the report says which.
- `RUN_TESTS=1 ./build.sh` green; no `src/` file changed.

## Outcome (2026-10-06)

Test-only; `git diff --stat main` touches no `src/` file. Six new passing cases (all `ranls-gui-ui-tests`, real `MainWindow` + `mock_engine`), added by extending the existing probes (no probe retired; only read-only latch accessors added to `RanlsAnlz05Probe` / `RanlsAnlz07Probe`):

| # | Behaviour | Status | Test |
|---|---|---|---|
| a | auto-move only on engine's turn AND Idle | happy path already covered; negatives (Off, not its turn, not Idle) **new** | `ARCH-04: auto-move fires only on the engine's turn AND when the engine is Idle` (`test_anlz05_no_automove_action.cpp`); happy path also `ANLZ-05: Analyze Mode blocks auto-move...` Scenario B; predicate in `test_eng02_revert_predicate.cpp` |
| b | auto-move suppressed in Analyze Mode on engine's turn | **already covered** | `ANLZ-05: Analyze Mode blocks auto-move and analyses the engine's-turn position` Scenario A |
| c | burst of `signal_board_changed` -> one check / one restart | **new** (auto-move + analyze) | `ARCH-04: a burst of signal_board_changed yields one auto-move check` (05 file); `ARCH-04: a burst of signal_board_changed yields exactly one analyze restart` (07 file) |
| d | force-latch not downgraded (ANLZ-07) | **new** | `ARCH-04: the force latch is not downgraded by a later non-forced call` (07 file; includes a non-forced-alone control) |
| e | Analyze Mode off -> `stopAnalysis()`, `enginePlays` untouched | **new** (ANLZ-01 action test only checked the checkbox state) | `ARCH-04: toggling Analyze Mode off stops the search and leaves enginePlays untouched` |
| f | ENG-02 revert -> Off in GameState, not persisted | **new** (`test_eng03_close_request` only covers the close path) | `ARCH-04: the ENG-02 revert sets enginePlays Off in GameState and is not persisted` (via the `stop` and `analyze` actions; positive control shows `onSetEnginePlays` writes the file; settings file bytes compared against a snapshot) |

Observation notes: (c) the wire count alone cannot distinguish one idle callback from two (a second bails on non-Idle), so the tests also assert the `autoMoveScheduled_` / `analyzeModeScheduled_` latch (read via the probe). (f) is observable without a production seam because `SettingsStorage::settingsFilePath()` is a plain file. Uncovered gap: none for the six bullets. Not characterized (outside them): the "engine not running" bail of the two idle callbacks and `onStartAnalysis`'s engine-not-running / empty-path branch.

Verification: Release `RUN_TESTS=1 ./build.sh` clean; ctest 5/5; `ranls-gui-tests` 228 cases / 2625 assertions (unchanged); `ranls-gui-ui-tests` 60 cases / 460 assertions (was 54 / 393; +6 / +67). All six new cases executed individually (display available on the host, none skipped).
