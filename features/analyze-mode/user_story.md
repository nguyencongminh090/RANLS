# Analyze Mode — continuous background analysis (the "Lizzie way")

## Background

WinGraph shows near-nothing in the common flow *user plays several moves → Analyze
once → user plays several more → Analyze once*: `GameState::setAnalysisData` only
writes an eval onto the node that was `currentPath()` **while a search ran**, so
every ply the user walked past without a search on that exact position stays NaN
(UI-01 gap). UI-13's candidate A fills only the single reply ply of a search root.

Research (2026-09-04, `docs/notes/2026-09-04-wingraph-analyze-mode-and-backfill.md`)
showed every mature analysis GUI (Lizzie/LizzieYZY, Sabaki, KaTrain, En Croissant)
solves this the same way: **never backfill with a formula — analyse every position
for real**, via continuous pondering while the user navigates. Decision (user,
2026-09-04): implement the Lizzie way.

## Actors

- **Reviewer** — steps/plays through a game and wants the win-rate graph filled
  with real evaluations for every position they visit.
- **Engine** (`EngineController` + Rapfi/Yixin subprocess) — pondering the current
  position as root whenever Analyze Mode is on and it is idle.

## User stories

1. As a reviewer, I turn **Analyze Mode** on; from then on, whenever I make a move,
   undo, redo, or jump to a ply, the engine automatically analyses the new current
   position and the WinGraph gains a real point there — I never have to press
   "Analyze" per position.
2. As a reviewer, I play a **weak or non-best move** (not in the engine's
   candidates); the graph still records the true win-rate of the resulting
   position (measured by re-analysing it), so the drop caused by my mistake is
   visible — not hidden by an estimate.
3. As a reviewer, I turn Analyze Mode **off**; the engine stops pondering and the
   app returns to explicit one-shot "Analyze" / "Stop".
4. As a reviewer with **"Engine plays <side>"** also on, on the engine's turn the
   engine makes its move (existing behaviour) and then resumes pondering the new
   position; on my turn it just ponders.

## Rules

- **No formula backfill on the plotted line.** A position with no real search
  stays a NaN gap (UI-01). A `1 − parent` estimate may be shown as *text* in the
  analysis panel but must not be plotted as a graph point. (Matches Lizzie
  `Board.place()`: it computes `nextWinrate = 100 - parent` but stores
  `playouts = 0`, and `WinrateGraph` only plots `playouts > 0`.)
- Analyze Mode restarts analysis on the **new current position as root** after
  every position change — it does not reuse the parent's candidate list.
- Position-change bursts (game load replaying every move, undoAll/redoAll) must
  coalesce into a single deferred restart — reuse the `autoMoveScheduled_`
  idle-callback pattern from `MainWindow::maybeStartAutoMove`.
- Analyze Mode is **orthogonal** to "Engine plays" and must not trigger the ENG-02
  `enginePlays → Off` revert.
- UI-13 candidate A stays as-is: real analysis overwrites the derived child (its
  guard is `child->depth <= 0 && child->nodes <= 0`), so no conflict.

## Hard constraints (do not touch)

- eval→win% conversion maths; UI-01 attribution; UI-09 SingleSide;
  `buildWinGraphSeries` perspective logic; RT-01 throttle cadence; WinGraph
  axes/layout/drawing.
- The existing one-shot Analyze/Stop buttons stay (Analyze Mode is additive).

## Out of scope (separate follow-ups)

- **ANLZ-02** — "Analyze entire game": a one-shot sweep of every played node with a
  fixed per-move budget (Lizzie "Auto analyze" / KaTrain "analyse all").
- **ANLZ-03** — persist per-node win% into the save-game file so re-opening a game
  keeps the graph (Sabaki/SGF `SBKV`).

## Cross-links

- [planning.md](planning.md) — open questions + sequencing
- [diagram/flow.md](diagram/flow.md) — state + sequence diagrams
- `docs/notes/2026-09-04-wingraph-analyze-mode-and-backfill.md` — the research
- `docs/todo/UI-13-wingraph-record-eval-regardless-of-side.md` — the prior fix
