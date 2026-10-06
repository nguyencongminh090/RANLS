# Extract orchestration out of MainWindow (ARCH-02)

## Background

The 2026-10-05 architecture review (`docs/audit/2026-10-05-architecture-review.md`, finding 2) found
`MainWindow` (`src/main_window.cpp` 1328 lines, `main_window.h` 269 lines, ~38 methods) mixes widget
construction with application logic. The logic that matters most — *when does the engine analyse, when
does it auto-move, what cancels auto-play* — grew across ENG-02, ANLZ-01/05/06/07 and UI-06, and every
one of those fixes needed a bespoke friend-class probe (`RanlsAnlz05Probe`, the ANLZ-07 probe) to reach
private `MainWindow` state, because the rules cannot be exercised without a GTK window. The rules are
application policy, not UI.

The logic in question today (all in `main_window.cpp`):

| Responsibility | Methods | State it owns |
|---|---|---|
| Analyze Mode restart | `scheduleAnalyzeModeRestart`, `onToggleAnalyzeMode` | `analyzeModeScheduled_`, `analyzeModeForce_` |
| Engine-plays auto-move | `maybeStartAutoMove`, `onSetEnginePlays` | `autoMoveScheduled_` |
| ENG-02 manual-override revert | `revertEnginePlaysToOff`, `onStartAnalysis`, `onStopAnalysis` | — |
| Game file orchestration | `onLoadGame`, `onSaveGame` (graph build, meta, archive reader/writer, apply) | — |
| Graceful close | `requestGracefulClose` | — (one line; see planning Q6) |

## Actors

- **Developer / maintainer** — wants to change an analysis rule and prove it with a plain unit test, with
  no display and no friend-class hack into a GTK window.
- **User** — must notice nothing: same menus, same shortcuts, same behaviour on every path above.

## User stories

1. As a maintainer, I can unit-test "Analyze Mode restarts exactly once after a burst of board changes,
   skips when the search already converged unless forced, and never auto-moves" without GTK.
2. As a maintainer, I can unit-test "Engine plays White arms, fires `requestEngineMove()` only on the
   engine's turn while Idle, and a manual Analyze/Stop reverts it" without GTK.
3. As a maintainer, I can unit-test that saving builds the right `GameGraph` (engine entry from
   `EngineConfig`, `.rdb` default extension, no writer for other extensions) and that loading applies it
   and re-syncs engine config, without opening a file dialog.
4. As a user, nothing changes: every existing `test_anlz*`, `test_eng*`, `test_rt01*` and RDB/console
   test keeps passing, and the UI is identical.

## Out of scope

UI redesign, menu/shortcut changes, behaviour changes of any kind, the `EngineController` internals,
`CommandDispatcher` (ARCH-03 is already done), splitting the remaining widget code in `MainWindow`.
