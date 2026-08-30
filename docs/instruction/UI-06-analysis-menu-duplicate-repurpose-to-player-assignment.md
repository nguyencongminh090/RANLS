# Instruction — UI-06: replace the "Analysis" menu with an "Engine plays" side selector

Detail: [docs/todo/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md](../todo/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md)

## Design decisions (resolved with the user 2026-08-30 — do not re-open)

- **Menu:** rename the existing menu-bar **Analysis** submenu to **Engine plays**. It contains a
  radio group of exactly three entries: **Black**, **White**, **Off**. No "Both" entry
  (engine-vs-engine is out of scope for this task).
- **Semantics:** "Engine plays <side>" means **auto-move** — when it becomes that side's turn, the
  engine is asked to produce the move and the app plays it. It is NOT merely auto-analyse. `Off`
  (the default) = no automatic play; the existing toolbar `▶ Analyze` / `■ Stop` remain the only
  way to get a one-shot analysis.
- **Persistence:** a **new `MatchConfig` struct** in [src/model/config.h](../../src/model/config.h),
  holding one field: `EnginePlaysSide enginePlays = EnginePlaysSide::Off;` (enum `Off, Black, White`).
  Wire it through `GameState` the same way `EngineConfig` / `ViewConfig` already are
  (`setMatchConfig` / `matchConfig()` / `signal_config_changed`), and round-trip it in
  `src/model/settings_storage.cpp` next to the other config blocks.
- The old toolbar `▶ Analyze` / `■ Stop` actions (`win.analyze` / `win.stop`) stay exactly as they
  are — this task does not touch the toolbar.

## Approach

1. `src/model/config.h`: add `enum class EnginePlaysSide { Off, Black, White };` and
   `struct MatchConfig { EnginePlaysSide enginePlays = EnginePlaysSide::Off; };`.
2. `GameState` (`src/model/game_state.h` / `.cpp`): add `matchConfig_` member, `matchConfig()`
   accessor, `setMatchConfig()` mutator that emits `signal_config_changed` (mirror
   `setViewConfig` at `src/model/game_state.cpp:407`).
3. `src/model/settings_storage.cpp`: serialize/deserialize the new field (see how `ViewConfig` /
   `EngineConfig` fields are handled around `settingsFilePath` / the load/save functions). Add a
   round-trip test in `tests/test_settings_storage.cpp`.
4. `src/main_window.cpp` `buildMenuBar()` (`src/main_window.cpp:140-212`):
   - Replace the `analysisMenu` block (lines 197-200) and its `append_submenu("Analysis", …)`
     (line 208) with an **Engine plays** submenu built from a
     `Gio::SimpleAction::create_radio_string("engine-plays", "off")` — mirror the `set-rule`
     radio-string action at `src/main_window.cpp:161-170`.
   - Entries: `win.engine-plays::black`, `::white`, `::off`.
   - On activate: `change_state(...)` then call a new `MainWindow::onSetEnginePlays(EnginePlaysSide)`
     that updates `MatchConfig` via `GameState::setMatchConfig`.
   - Initialize the radio action's state from the loaded `MatchConfig` at startup.
5. Auto-move hook: when a move is made / position changes and it is now `enginePlays` side's turn
   (and the engine is `Idle`), trigger the engine to move. Reuse the existing analyze/best-move
   path — look at how `EngineController::analyze()` (`src/engine/engine_controller.cpp:213`) and the
   protocol's best-move handling work; the simplest correct wiring is: on the game-state
   move-applied signal in `MainWindow`, if `matchConfig().enginePlays` matches the side to move and
   `engineController_` is usable/idle, request a move and apply the returned best move as if the
   user had played it. If a clean "engine, make one move and play it" path does not already exist,
   keep the change minimal: request an analysis and, on the first best-move result for that
   position, play it — but gate it so it fires exactly once per position and never while
   `enginePlays == Off`.
6. Update the radio state if `MatchConfig` is changed elsewhere (settings load).

## Boundaries — do not touch

- No "Both" / engine-vs-engine mode. Single side only.
- Do not change the toolbar, `win.analyze`, `win.stop`, or `EngineController::analyze()` /
  `stopAnalysis()` semantics for the manual path.
- Do not add auto-analyse-without-moving as a separate setting — not in scope.
- Do not move the setting into `EngineConfig` or `ViewConfig` — it goes in the new `MatchConfig`.
- Do not touch `src/ui/settings_dialog.cpp` — the selector lives only in the menu bar for this task.
  (UX-06 owns the settings dialog.)
- Do not rework `docs/sprint/*` beyond the status update for this item.

## Pitfalls

- `Gio::SimpleAction::create_radio_string` state must be kept in sync with the model in BOTH
  directions — set it from the loaded config on startup, and update the model on activate. The
  `set-rule` action is the pattern to copy but note it does NOT currently reload its state from
  config, so don't just copy it blindly.
- Auto-move must not create an infinite loop (engine plays → position changes → engine plays for
  the same side again). Gate on side-to-move and on "not already thinking / already moved for this
  ply".
- Auto-move must be inert when the engine process is not running or not `Idle`.
- Watch `GameState` construction order in tests — a new member with a defaulted enum is fine, but
  the settings round-trip test must assert the default is `Off`.

## Verification before marking this task done

1. **Build:** the project builds clean (`./build.sh` or the documented CMake invocation) with no
   new warnings.
2. **Unit tests:** `ctest` in the build dir — all pass, including the new
   `tests/test_settings_storage.cpp` case for the `MatchConfig` round-trip (default `Off`, and a
   set-to-Black value survives save+load).
3. **Manual smoke (state it was done, or state the display server was unavailable and it was
   skipped):** launch the app, open the **Engine plays** menu, confirm three radio entries
   Black/White/Off, that selecting one is persisted across a restart, and that with a running
   engine set to the side-to-move the engine makes a move on its own; with `Off` it does not.
4. No menu entry duplicates a toolbar action (Acceptance criterion 1).

Passing unit tests alone is NOT sufficient — the build must be clean and the manual smoke must be
run or explicitly reported as skipped-with-reason.
