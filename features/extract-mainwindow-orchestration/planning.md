# Extract MainWindow orchestration — planning

See [user_story.md](user_story.md) · [diagram/flow.md](diagram/flow.md).

**Status: RESOLVED 2026-10-06** — see "Resolution" below. (Draft text kept as history; the
"Proposed default" column is the decision of record only where the Resolution says so.)

## Resolution — 2026-10-06 (user)

The user answered the three decisions that shape the work; the remaining questions take their proposed
defaults (stated to the user as such when asking).

- **Q1 → `src/engine/`** (GTK-free `AnalysisCoordinator`; no change to rule 8).
- **Q2 → injected `postIdle`** (coordinator owns coalescing flags + guard order; `MainWindow` supplies the Glib idle adapter; tests use a manual queue).
- **Q5 → yes, `GameFileService`** as its own task.
- **Q3, Q4, Q6, Q7, Q8 → proposed defaults** (menu sync, persistence and graceful close stay in `MainWindow`; one `AnalysisCoordinator`; existing friend probes kept as the safety net).

Task split (CODE format forbids `ARCH-02a`): **ARCH-04** characterization tests (first, tests only) →
**ARCH-02** `AnalysisCoordinator` (rescoped to just this) → **ARCH-05** `GameFileService`. One PR each.

## Open questions (draft, as proposed)

| # | Question | Proposed default | Alternatives |
|---|---|---|---|
| Q1 | **Target layer** for the extracted decision logic? `AnalysisCoordinator` needs `EngineController` (engine) + `GameState` (model). | `src/engine/analysis_coordinator.{h,cpp}`, GTK-free. `engine/ → model/` is already the allowed direction, `EngineController` is already orchestration, and no new layer means no change to rule 8 / the `software-architecture` skill. | (B) new `src/app/` layer (`ui → app → engine → model`) — cleaner naming, but needs a rule-8 + skill + knowledge-note update first. (C) put it in `command/` next to `revertEnginePlaysIfEnginesTurn` — wrong: it is not a command. |
| Q2 | **Scheduling seam.** Today `maybeStartAutoMove` / `scheduleAnalyzeModeRestart` call `Glib::signal_idle().connect_once` directly (the earlier ARCH-02 text says "timer", but it is idle coalescing — `analyzeModeScheduled_`, `autoMoveScheduled_`). | Coordinator takes an injected `std::function<void(std::function<void()>)> postIdle`. `MainWindow` passes a Glib-idle adapter; tests pass a manual queue they drain explicitly — deterministic, no main loop. | Keep the idle call in `MainWindow` and have the coordinator expose `bool shouldRestartNow()` / `bool shouldAutoMoveNow()` pure predicates (smaller extraction, but coalescing flags stay in the GTK class, so burst-coalescing stays untestable). |
| Q3 | **Menu/toggle sync.** `sync*Menu()` touches `Gio::SimpleAction`s. | Stays in `MainWindow`. Coordinator exposes what changed (a `sigc::signal` or a `std::function` hook for "enginePlays changed by revert"); `MainWindow` re-syncs the menu. | Coordinator holds action pointers — rejected, drags GTK in. |
| Q4 | **Persistence.** `onSetEnginePlays` / `onToggleAnalyzeMode` call `SettingsStorage::save(...)` with every config block (STATE-02/04); the ENG-02 revert deliberately does **not** save. | Persistence stays in `MainWindow` (it already has `persistGameSetup()`); the coordinator only mutates `GameState` configs. The no-save-on-revert rule becomes a coordinator unit test. | Inject a `persist` callback so the toggle handlers move wholesale — more moved code, more risk, little extra testability. |
| Q5 | **`GameFileService` scope.** Dialogs (`Gtk::FileDialog`, filters, `showErrorDialog`, `confirmDiscardGame`) cannot move. | Move only the GTK-free parts: build `GraphMeta` + `toGameGraph` from `GameState` (incl. the RDB-03 engine entry), default-extension handling, writer lookup, and "load via `archiveReaderFor` + `applyGameGraphToState`" returning `{ok, error}`. `MainWindow` keeps dialogs and the `controller_.sendConfig()` call after a successful load. | Skip `GameFileService` entirely this round (≈60 lines, already mostly delegating to `rdb::`) and do only the coordinator. |
| Q6 | **Graceful close** (`requestGracefulClose`, ENG-03). | Leave in `MainWindow`: it is one call (`controller_.stopEngine([this]{ close(); })`) and `close()` is inherently a window action; already covered by `test_eng03_close_request`. | Wrap in coordinator — no benefit. |
| Q7 | **Naming / count of classes.** | One `AnalysisCoordinator` owning Analyze-Mode restart + engine-plays auto-move + ENG-02 revert (they share state: `analyzeMode` gates auto-move, ANLZ-05). Splitting them would re-introduce cross-class coupling. | Two classes (`AnalyzeModeScheduler`, `AutoPlayController`) with a shared interface. |
| Q8 | **Replace the friend probes?** Existing tests reach into `MainWindow` via friend probes (`RanlsAnlz05Probe`, the ANLZ-07 probe, `test_anlz05_no_automove_action`, `test_anlz07_*`). | Keep them untouched as the safety net through the whole extraction; add coordinator unit tests alongside, and only retire a probe in a separate follow-up once the coordinator test covers the same case. | Port probes to coordinator tests in the same PR — higher risk of silently weakening coverage. |

## Characterization first (before any extraction)

Rule from `docs/todo/ARCH-02`: no extraction without tests that pin current behaviour. Existing coverage
to audit and reuse: `test_anlz01_*`, `test_anlz05_*`, `test_anlz06_*`, `test_anlz07_*`,
`test_eng01/02/03_*`, `test_rt01_throttle`. Gaps to check and fill *against current `MainWindow`* (the
probes already give access): auto-move only on the engine's turn + Idle; auto-move suppressed in Analyze
Mode (ANLZ-05); burst of `signal_board_changed` → one restart; force-latch not downgraded by a later
non-forced call (ANLZ-07); Analyze Mode off → `stopAnalysis()` without touching `enginePlays` (Q7/Q8 of
`features/analyze-mode`); ENG-02 revert is not persisted.

## Proposed sequencing (one PR per step, per the `github` skill)

1. **ARCH-02a — characterization tests** (tests only, no `src/` change). Fills the gap list above.
2. **ARCH-02b — `AnalysisCoordinator`.** Move decision logic + coalescing flags; `MainWindow` keeps idle
   adapter, menu sync, persistence. All existing tests unchanged and green; new coordinator unit tests
   with the manual scheduler.
3. **ARCH-02c — `GameFileService`** (only if Q5 = move). Pure move of the GTK-free parts.
4. Optional follow-up: retire redundant friend probes (Q8).

Acceptance (from the todo): behaviour unchanged; `test_anlz*`, `test_eng*`, `test_rt01*` pass; extracted
logic unit-tested without GTK; `main_window.cpp` materially smaller. Suggest a measurable bar for "materially":
each extraction PR removes the moved code and reports before/after line counts.

## Risks

- **Ordering subtleties are the whole point of the recent fixes** (ANLZ-05 guard order; ANLZ-07
  force-latch; `stopAnalysis()` must precede `analyze()` — `analyze()` early-returns unless Idle). A
  mechanical move must preserve guard order exactly; the characterization step exists to prove it.
- **Lifetime:** the idle callback captures `this`. The coordinator must not outlive `EngineController` /
  `GameState`, and a pending callback must be harmless after destruction (same constraint ENG-01/03 hit
  with async stop).
- **No live engine / display on the build host:** UI-level confirmation of "nothing changed" remains a
  human smoke step (Analyze Mode on, step through moves, Engine plays, Save/Load).

## Knowledge

`docs/knowledge/principle-solid-overview.md` (SRP), `docs/knowledge/style-ui-mvc-family.md`,
`docs/knowledge/pattern-facade.md`, `docs/knowledge/principle-layering-dependency-rule.md`;
skills `solid-single-responsibility`, `software-architecture`; audit
`docs/audit/2026-10-05-architecture-review.md`; prior features `features/analyze-mode/`.

## Implementation outcome — 2026-10-06 (ARCH-02)

Implemented per the Resolution: `AnalysisCoordinator` in `src/engine/` (Q1), injected `postIdle` (Q2), menu sync via `signal_engine_plays_reverted` handled in `MainWindow` (Q3), persistence stays in `MainWindow` (Q4), one class (Q7), friend probes kept unchanged (Q8; latches exposed through const-reference members). Lifetime handled with a `weak_ptr` liveness token. `main_window.cpp` 1328 -> 1228 lines, `.h` 269 -> 244. See `docs/fix-log/2026-10-06-arch-02-analysis-coordinator.md`.
