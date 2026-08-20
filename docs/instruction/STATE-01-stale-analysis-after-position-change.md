# STATE-01 — execution guidance

## Approach

The bug is six copies of a half-done invariant. Do not fix it six times.

Add one private helper on `GameState` — something like `resetAnalysisState()` — that clears
`pvLines_`, resets `engineStatus_`, and marks the analysis UI dirty. Call it from every operation
that changes the current position:

`newGame`, `loadPosition`, `makeMove`, `undoMove`, `redoMove`, `gotoMove`, `gotoPath`.

Then make sure the notification actually reaches `PVView` and `EngineStatusView`. Two options:

1. Emit `signal_engine_analysis` from the reset path (smallest change), or
2. Have `AnalysisPanel`'s `signal_board_changed` handler also refresh `PVView` and
   `EngineStatusView` (`src/ui/analysis_panel.cpp:88-95` currently refreshes only the win graph and
   trees).

Prefer (1) — it keeps "analysis data changed" as one signal with one meaning, rather than making
`signal_board_changed` implicitly also mean "analysis data changed."

## Pitfalls

- `MainWindow`'s handler clears `candidateMoves` at `src/main_window.cpp:280` and then
  `boardViewModel_.update()` refills it from `pvLines_` on the next line. That defensive clear
  becomes redundant once the model is correct — remove it rather than leaving two mechanisms that
  disagree.
- `gotoPath` (`src/model/game_state.cpp:148-179`) already clears `pvLines_` **and** can return
  `false` partway through after mutating the board (`:164`, `:165`, `:168`, `:172`). That
  partial-failure path leaves the model inconsistent regardless of this fix — note it, and either
  fix it here or file it separately rather than silently inheriting it.
- `undoAll`/`redoAll` loop over `undoMove`/`redoMove`, so a naive reset-per-call adds another
  per-ply signal to the flood NAV-01 describes. Coordinate: either land NAV-01's batching first, or
  make the reset cheap and idempotent so batching can absorb it later.
- Do not clear analysis data when the engine is mid-search on the *same* position — only on position
  change. `setAnalyzing()` state is the discriminator.

## Testing

Requires TEST-01. The tests are model-only, no display needed:

- `makeMove` → `undoMove` → assert `pvLines()` empty and `engineStatus()` default
- `setAnalysisData(...)` → `newGame()` → assert cleared **and** the analysis signal fired
- One test per position-changing operation, so the next one added without a reset call fails loudly

Per `/CLAUDE.md`: never discard these after they pass — they are the permanent guard against the
seventh copy of this bug.

## Do not touch

- The tree-eval writeback condition at `src/model/game_state.cpp:211` — that is UI-01. Touch it only
  if the shared reset path genuinely requires it, and say so in the summary if you do.
- Update *frequency* — RT-01.
- Empty-state placeholder text — UX-01. This item makes the data empty; that item makes empty look
  intentional.
