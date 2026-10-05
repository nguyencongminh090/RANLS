# 2026-10-05 — Architecture review (layer boundaries, MainWindow, CommandContext)

**Status:** Active

## Prompt
"Review Architecture" — checked `src/` against the `software-architecture` skill's layer rule
(`ui → command/engine → model`) and `docs/knowledge/principle-layering-dependency-rule.md`.

## Method
Include-graph scan per layer (`grep '#include'`), file-size survey, read of `CommandContext`,
`IEngineProtocol`, `MainWindow` method list. Static inspection only; nothing built or run.

## Findings
**Clean:** `engine/` and `command/` have no include of `ui/` or gtkmm; `model/` has no GTK; no
singletons/globals; `GameState` signal hub (20 signals) decouples layers; protocol abstraction
(`IEngineProtocol` → `GomocupProtocol` → `EngineProcess`) intact; ~37 test files + mock engine.

1. **model ↔ engine include cycle (highest).** `model/game_state.h:7` and `model/board_view_model.h:5`
   include `engine/engine_types.h`, which includes `model/board_state.h`. `engine_types.h` holds
   `PVLine`, `EngineStatus`, `DatabaseEntry`, `AnalysisOverlay` — analysis *domain data*, not
   protocol types. Violates "model must not know engine". → ARCH-01.
2. **`MainWindow` god-object** (1329-line cpp, 269-line header, ~38 methods): menu/toolbar build,
   file I/O orchestration (rdb, SettingsStorage), analyze/auto-move/engine-plays state machine
   (`maybeStartAutoMove`, `scheduleAnalyzeModeRestart`, `sync*Menu`), graceful close, tick throttle.
   Application logic living in a GTK class is hard to test (SRP). → ARCH-02.
3. **`CommandContext` holds concrete `EngineProcess&`** alongside `EngineController&`, so `command/`
   reaches the lowest engine layer (DIP). → ARCH-03.
4. **Large files:** `protocol_extension.cpp` 1191, `gomocup_protocol.cpp` 1101,
   `command_dispatcher.cpp` 887 (all commands in `registerBuiltins()`). Readability, not correctness.
   → folded into ARCH-03 (dispatcher split only); protocol file split left unfiled.
5. **Protocol leak:** `EngineController` and `i_custom_command_source.h` include
   `protocol_extension.h` directly. Not filed — needs a design call first.

## Decision
Record findings; file ARCH-01/02/03 in Backlog (not Active, no sprint commitment). No code changed.
Suggested order: ARCH-01 (small, mechanical) → ARCH-02 (large, needs regression tests first) → ARCH-03.

## Summary
Layering is mostly sound; one real rule violation (cycle), one SRP hotspot (MainWindow).
