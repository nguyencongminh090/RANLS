# UX-10 — Analysis workspace: interaction spec

Task: [UX-10](../todo/UX-10-analysis-workspace-design.md). Review date 2026-11-18, app v0.12.0 (main `375532a`), dark theme, English. This is a design only and nothing is implemented. Each item is tagged **Current** (observed in the built app or read from source) or **Proposal**. The method is an expert heuristic walkthrough ([ux-research-usability](../knowledge/ux-research-usability.md)), not evidence from users.

## Figures

| Figure | File | Shows |
|---|---|---|
| Mockup 1 | [new-game-engine-off.svg](new-game-engine-off.svg) | New game, engine off, first action marked |
| Mockup 2 | [live-analysis-branched.svg](live-analysis-branched.svg) | Analyzing, 4 candidate lines, graph, metrics, branched tree, 5. H6 selected |
| Supporting figure A | [supporting-visual-and-table.svg](supporting-visual-and-table.svg) | Visual and Table for the same selection |
| Supporting figure B | [supporting-engine-states.svg](supporting-engine-states.svg) | Four engine states with a grayscale check |
| Supporting figure C | [supporting-narrow-window.svg](supporting-narrow-window.svg) | 926×560 window, board legibility, restoring the logs |

The SVGs are standalone vector files and open in any browser. Mockups 1 and 2 share the layout below.

## Baseline method and limits

The Release build of this worktree ran under Xvfb with an isolated settings file. No Rapfi binary was available, so the engine was a **scripted fixture** speaking the Gomocup/Yixin subset, and all live numbers are illustrative. The fixture is not committed, see the [task detail](../todo/UX-10-analysis-workspace-design.md). Two real screenshots are kept: [new game, engine off](baseline/01-new-game-engine-off.png) and [live analysis on a branched game](baseline/02-live-analysis-thinking.png). Not reviewed: light theme, board sizes other than 15, MultiPV above 4, Renju marks, other languages, screen readers.

## Current behavior observed

- **Controls.** The blue header **Analyze** and an unlabelled ▶ in the engine strip can both start the engine. Header **Stop** stops analysis and the strip's ■ stops the engine process. Only tooltips tell them apart.
- **State chip.** OFF, ON, THINKING and CRASHED share one round dot that only changes color, and "ON" does not say idle. While thinking, Analyze keeps its solid blue look and the file and navigation icons dim.
- **Failures.** With a bad engine path the chip stays OFF and the only message, `Failed to start engine`, is in the Engine Log tab behind Move Log. After a crash the CRASHED chip and a red banner appear, but the old candidate rows stay on screen.
- **Quiet panes.** Empty panes show no placeholder text, the graph keeps its 0/50/100% scaffold ([UI-08](../todo/UI-08-remove-empty-state-placeholder-text.md) kept it), and the metrics row shows `D: -` dashes.
- **Trees.** Visual is vertical, unlabelled and clipped. Every node on the current path has the same ring, and a branch is a second column joined by a stub. Table showed 2 of 5 rows, so the selected row was below the fold. Switching tabs left the board and the selected move unchanged.
- **Panels.** The toggles are main menu → View → **Analysis Panel** and **Move/Engine Log Panel**, 3 clicks with no shortcut, not in the Ctrl+K palette, one checkbox for both logs, no cue when hidden, and session-only. Session-only was deliberate and shortcut and palette ideas are unfiled in the [2026-11-12 audit entry](../audit/2026-11-log.md).
- **Sizes.** At 1280×800 the board is 462 px wide and the full-width log pane is 208 px high. The narrowest window the app allowed was 926 wide. There the board measured 290 px, 382 px with the logs hidden and 427 px with both panels hidden.

## Layout (same in both mockups)

```
header: menu · New/Open/Save · rule chip · [Analyze][Stop analysis] · first/undo/redo/last
board column                        | analysis column
  coordinates, board, candidate tags |  1 engine strip: state chip · Start/Stop engine · Restart · Analyze mode
  status line under the board        |  2 search metrics row (live only)
                                     |  3 win-rate graph
                                     |  4 candidate lines with continuations
                                     |  5 Visual / Table tabs
                                     |  6 log bar: Move Log · Engine Log
```

**Proposal** changes: labelled engine strip, metric words, candidate chips, status line, and a log bar inside the analysis column so the board column keeps the full height. The board, header icons, colors and tab names are unchanged.

## Engine actions

| Control | Label | Does |
|---|---|---|
| Header primary | **Analyze** (F5) | Starts the engine if needed, then analyzes the selected position. Current, plus a tooltip proposal. |
| Header secondary | **Stop analysis** (Esc) | Cancels the search and disarms Engine plays. The engine keeps running. Today labelled "Stop". |
| Strip | **Start engine** / **Stop engine** | Launches or terminates the engine process. Start engine does not itself request a search. A search starts from Analyze, or from Analyze mode or Engine plays when either is armed and the engine is Idle. Today unlabelled icons. |
| Strip | **Restart**, **Analyze mode** | Reload the engine, and analyze each position continuously. Today icons. |

With the engine off, the first available action is **Analyze**, the only blue control. **Start engine** is the quiet secondary option. Labelling the strip reverses part of [UI-21](../todo/UI-21-modern-chrome-icons-css.md), which made those buttons icons. The header keeps its two text labels.

## Engine states (supporting figure B)

| State | Code state (today) | Text | Shape | Header | Strip |
|---|---|---|---|---|---|
| Off | `NotStarted` (OFF) | "Off" | dashed ring | Analyze on, Stop analysis off | Start engine, Restart off |
| Idle | `Idle` (ON) | "Idle" | check in a disc | Analyze on, Stop analysis off | Stop engine, Restart |
| Analyzing | `Analyzing` (THINKING) | "Analyzing" | open spinner arc | "Analyzing…" outlined, Stop analysis on | Stop engine, Restart |
| Disconnected | `Crashed` or failed start | "Disconnected" and a message | warning triangle | Analyze on (retries) | "Reconnect" |

Starting and Stopping are transient, shown as "Starting…" and "Stopping…" with controls disabled. Every state has a word, a shape and a different set of enabled controls, so color is decoration only. A failed start should land in Disconnected with the message in the strip. Whether stale candidate rows should dim after a crash is left open.

## Quiet panes and live view

Engine-off panes follow UI-08. The only **Proposal** is hiding the dash-filled metrics row until the engine reports. The status line says only the side to move and the rule.

Mockup 2 uses one position: 1 H8, 2 I9, 3 I7, 4 G7, 5 H6 selected, White to move. The fork is after 3. I7, 4. G7 is an alternate to 4. G9, and the other line is 4 G9, 5 H9, 6 J8.

- **Candidates.** Current: text rows and win-rate tags on the board. Proposal: a win-rate bar, continuation chips colored by side, and a rank number on each board tag. Row 1 is 71.1 percent, equal to White's point in the graph.
- **Graph.** Current: Black solid blue and White dashed green, a red rule and one dot at the selected move. Points for moves 1 to 4 assume earlier analysis, otherwise they are gaps.
- **Metrics.** Proposal: words (Depth, Nodes, Speed, Time, Eval, Best) that wrap like today's stats ([UI-23](../todo/UI-23-typography-and-layout-polish.md)).

## Visual and Table (supporting figure A)

| | Visual (All Branches) | Table (Current Line) |
|---|---|---|
| Shows | Whole tree | Root to the selected move only |
| Selected move | Proposal: double ring, bold label, "selected" tag | Last row. Proposal: play marker, bold text, scrolled into view when the tab is shown |
| Fork | Proposal: badge "fork · 2 lines" after 3. I7, the other line gray and dashed | Proposal: a "Line" column marks 3. I7 as the fork and 4. G7 as "alternate to 4. G9" |

Selection rules: one selection exists, the game's current node, and both views are projections of it. Switching tabs never changes the position, the board or the graph marker. Clicking a node or row selects that path and the other view re-renders around it.

## Narrow windows and panels (supporting figure C)

Board sizes at 926×560: 290 px today (**measured**), 382 px with the logs hidden (**measured**, and the expected size for the proposal), 427 px with both panels hidden (**measured**), and about 395 px with the log bar kept and the Analysis panel hidden (**estimate**).

**Proposal:**

1. Replace the full-width log pane with a **32 px log bar** at every window size, showing "Move Log" and "Engine Log". One click on either opens a drawer on that tab in the analysis column.
2. Show counts as text ("5 moves · 0 errors", "Engine Log · 1 error") so a failed start is not hidden.
3. With the Analysis panel hidden, the bar spans the full window width, so the same click still restores either log.
4. Split the View entry into **Analysis Panel**, **Move Log** and **Engine Log** and add them to the Ctrl+K palette. The current path stays valid meanwhile.
5. A hidden Analysis panel leaves a 28 px rail labelled "Analysis". Below about 1000 px wide or 700 px high, start with the bar folded and let a manual choice win for the session.

## Decisions touched

[UI-08](../todo/UI-08-remove-empty-state-placeholder-text.md) is kept. [UI-21](../todo/UI-21-modern-chrome-icons-css.md) is partly reversed in the engine strip. [UI-23](../todo/UI-23-typography-and-layout-polish.md) wrapping is kept. Session-only panel visibility is kept.

## Suggested follow-ups (not filed)

1. Labelled engine strip, "Stop analysis", and the four-state chip with a failed-start message.
2. Selected-node and fork cues in Visual, and selected-row and fork cues in Table.
3. Log bar, split panel toggles with palette entries, and the analysis rail.
