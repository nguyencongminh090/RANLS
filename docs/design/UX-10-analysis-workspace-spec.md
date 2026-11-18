# UX-10 — Analysis workspace: interaction spec

Task: [UX-10](../todo/UX-10-analysis-workspace-design.md). Review date 2026-11-18, app v0.12.0 (main `375532a`), dark theme, English.
This is a design only. Nothing here is implemented. Every item is tagged **Current** (observed in the built app or read from source) or **Proposal**.

## Deliverables

| Figure | File | Shows |
|---|---|---|
| Mockup 1 | [new-game-engine-off.svg](new-game-engine-off.svg) ([png](new-game-engine-off.png)) | New game, engine off, with the first action marked |
| Mockup 2 | [live-analysis-branched.svg](live-analysis-branched.svg) ([png](live-analysis-branched.png)) | Mid-game, analyzing, 4 candidate lines, graph, metrics, branched tree, 5. H6 selected |
| Mockup 3 | [visual-and-table-views.svg](visual-and-table-views.svg) ([png](visual-and-table-views.png)) | Visual and Table for the same position and selection |
| Mockup 4 | [engine-states.svg](engine-states.svg) ([png](engine-states.png)) | Off, idle, analyzing, disconnected, plus a grayscale check |
| Mockup 5 | [narrow-window-and-panels.svg](narrow-window-and-panels.svg) ([png](narrow-window-and-panels.png)) | 926×560 window, board legibility, restoring the logs |

The SVGs are standalone vector files and open in any browser. The PNGs are renders of the same files. Mockups 1 and 2 share one layout (below).

## Baseline: what the current app does

Method: the Release build of this worktree, run under Xvfb with an isolated settings file, English UI. The engine was a **scripted fixture** (a small Python program speaking the Gomocup/Yixin protocol, not a real engine), so every number in the live shots is illustrative. Screenshots are in [baseline/](baseline/). The fixture is not committed, see the task detail for how it was driven.

| # | Observation | Evidence |
|---|---|---|
| B1 | A blue **Analyze** header button and an unlabelled ▶ icon in the engine strip can both start the engine. Header **Stop** (stops analysis) and the strip's ■ icon (stops the engine process) look alike. Only tooltips tell them apart. | [01](baseline/01-new-game-engine-off.png), source `main_window.cpp` (`onStartAnalysis`, `signal_start`, `signal_stop`) |
| B2 | While the engine thinks, **Analyze** keeps its solid blue look. File and navigation icons dim. | [02](baseline/02-live-analysis-thinking.png) |
| B3 | The state chip reads OFF, ON, THINKING, CRASHED. The glyph is the same round dot, only its color changes. "ON" does not say idle. | [01](baseline/01-new-game-engine-off.png), [02](baseline/02-live-analysis-thinking.png), [03](baseline/03-after-stop-idle.png), [06](baseline/06-engine-crashed.png), `engine_status.cpp` |
| B4 | The engine-off analysis area is not fully quiet. It shows `D: - N: - NPS: -` dashes and an empty 100/50/0% graph. | [01](baseline/01-new-game-engine-off.png) |
| B5 | With a bad engine path, Analyze leaves the chip at OFF and shows `D: 0 N: 0 NPS: 0/s Eval: 50.0%`. The only message, `Failed to start engine`, is in the Engine Log tab, which is hidden behind the Move Log tab. | [07](baseline/07-bad-engine-path-stays-off.png), [08](baseline/08-bad-engine-path-engine-log.png) |
| B6 | After a crash the chip says CRASHED and a red dismissible banner names the engine. The candidate rows and metrics from before the crash stay on screen, unmarked. | [06](baseline/06-engine-crashed.png) |
| B7 | At 1280×800 the board is 462 px wide. The full-width log pane is 208 px high and mostly empty. | [01](baseline/01-new-game-engine-off.png) |
| B8 | The Visual tree is vertical, unlabelled, and clipped by a 120 px pane. Every node on the current path has the same blue ring, so the selected node is not distinct. A branch is a second column with a short stub. | [05](baseline/05-visual-tab.png), `tree_node_view.cpp` |
| B9 | The Table pane showed 2 rows of 5. The selected row 5 was below the fold until scrolled. It is a blue bar when visible. | [13](baseline/13-table-tab-current-row-below-fold.png), [14](baseline/14-table-tab-scrolled-current-row.png) |
| B10 | Switching Visual and Table left the board and the selected move unchanged. | [02](baseline/02-live-analysis-thinking.png) vs [04](baseline/04-table-tab.png) |
| B11 | Panel toggles: main menu → View → **Analysis Panel** and **Move/Engine Log Panel**. That is 3 clicks, with no shortcut shown. One checkbox covers both logs. The command palette lists only Search Overlay and Search Winrate Tags. Hidden panels leave no cue. The state is session-only. | [09](baseline/09-view-menu.png), [12](baseline/12-narrow-both-hidden-view-menu.png), `main_window.cpp` (`makeViewToggle`, `buildMenuModel`), `palette_catalog.cpp` |
| B12 | The narrowest window the app allowed was 926 px wide (a request for 820 was clamped). There the board is 290 px wide with move numbers about 8 px. Hiding the logs gives 382 px and hiding both panels gives 427 px. | [10](baseline/10-narrow-926x560.png), [11](baseline/11-narrow-logs-hidden.png), [12](baseline/12-narrow-both-hidden-view-menu.png) |

## Layout (same in both states)

```
┌ header: menu · New/Open/Save · rule chip · [Analyze][Stop analysis] · first/undo/redo/last ─┐
│ board column (left)            │ analysis column (right)                                   │
│  coordinates                    │  1 engine strip: state chip · Start/Stop engine · Restart │
│  board + candidate tags         │    · Analyze mode                                         │
│                                 │  2 search metrics row (live only)                         │
│  status line under the board    │  3 win-rate graph                                         │
│                                 │  4 candidate lines with continuations                     │
│                                 │  5 Visual / Table tabs (move tree)                        │
│                                 │  6 log bar (Move Log · Engine Log)                        │
└─────────────────────────────────┴───────────────────────────────────────────────────────────┘
```

Differences from today (all **Proposal**): labelled engine strip, metrics with words, candidate rows with continuation chips, status line, and the log bar inside the analysis column so the board column keeps the full height. Board, header icons, colors and the existing view tabs are unchanged.

## Engine actions

| Control | Label | Does | Today |
|---|---|---|---|
| Header primary | **Analyze** (F5) | Starts the engine if it is not running, then analyzes the selected position. Action `win.analyze`. | **Current** and unchanged. Proposal adds the F5 hint and the tooltip "Starts the engine, then analyzes this position". |
| Header secondary | **Stop analysis** (Esc) | Stops the search only. The engine stays running. Action `win.stop`. | Today labelled "Stop". Proposal: rename. |
| Strip | **Start engine** / **Stop engine** | Launch or terminate the engine process. Never starts a search. | Today unlabelled ▶ and ■ icons with tooltips. Proposal: text labels, and only one of the two is shown at a time. |
| Strip | **Restart** | Reloads the engine. | Today ↻ icon. |
| Strip | **Analyze mode** | Continuously analyzes each position. | Today ∞ icon. |

**First available action with the engine off** is **Analyze** in the header, the only blue control. **Start engine** is the quiet secondary option for someone who wants the engine ready without searching. The two never share a label or an icon with the stop controls.

## Engine states

The figure [engine-states.svg](engine-states.svg) shows each state in color and in grayscale.

| User-facing state | Code state (today's chip) | Text | Shape | Header | Strip |
|---|---|---|---|---|---|
| Off | `NotStarted` (OFF) | "Off" | dashed ring, dashed chip border | Analyze enabled, Stop analysis disabled | Start engine, Restart disabled |
| Idle | `Idle` (ON) | "Idle" | check mark in a disc | Analyze enabled, Stop analysis disabled | Stop engine, Restart |
| Analyzing | `Analyzing` (THINKING) | "Analyzing" | open spinner arc | Analyze reads "Analyzing…" and is outlined, Stop analysis enabled | Stop engine, Restart |
| Disconnected | `Crashed`, or a failed start | "Disconnected" plus the message | warning triangle, red inline message | Analyze enabled (retries) | "Reconnect" replaces Start engine |
| Starting, Stopping | `Starting`, `Stopping` | "Starting…", "Stopping…" | dashed ring, small square | Analyze inactive | button disabled |

Each state therefore has a word, a distinct shape and a different set of enabled controls, so color is decoration only. The grayscale row in the figure is the check. A failed start (B5) should land in **Disconnected** with the inline message, not stay at Off with a hidden log line. Stale candidate rows after a crash (B6) are an open question, see below.

## Quiet engine-off panes (UI-08)

**Proposal:** with the engine off the metrics row, graph contents, candidate lines and tree stay empty with no placeholder text. That means hiding the dash metrics and the empty axes from B4. The first action is carried by the controls only. A failure is not an empty state, so the Disconnected message is allowed.

## Live analysis view

Mockup 2 carries one position: 1 H8, 2 I9, 3 I7, then the selected branch 4 G7, 5 H6, with White to move. The other branch from 3. I7 is 4 G9, 5 H9, 6 J8.

- **Candidates** show rank, first move, eval, win rate with a bar, depth and the full continuation as chips colored by the side that plays them. Board tags carry the same rank number inside a black dot, so a tag finds its row without relying on its hue. Row 1 is 71.1 percent, equal to White's point in the graph.
- **Graph** (`WinGraphMode::BothSide`, the default): Black is the solid line with filled dots, White the dashed line with hollow dots. A red rule and a ringed dot mark move 5. The graph follows the current line, moves 1 to 5.
- **Metrics** use words: Depth, Nodes, Speed, Time, Eval, Best.
- Numbers are the ones the fixture produced in the live baseline ([02](baseline/02-live-analysis-thinking.png)) apart from the graph history and tree columns, which were invented to match.

## Visual and Table views

See [visual-and-table-views.svg](visual-and-table-views.svg).

| | Visual (All Branches) | Table (Current Line) |
|---|---|---|
| Shows | Whole tree, every branch | Root to the selected move only |
| Current move | **Proposal:** double ring, bold white label and a "selected" tag. Today all path nodes share one ring (B8). | It is always the last row. **Proposal:** play marker, bold text, scrolled into view when the tab is shown. Today it can sit below the fold (B9). |
| Branches | **Proposal:** a fork badge on the parent "branch point · 2 lines", the other line gray with dashed rings. Today a stub with no label. | **Proposal:** a "Line" column marks the fork row and the first row of a side line. |

**Selection when switching views (Proposal, consistent with today's behavior B10):**

1. There is one selection, the game's current node, defined by its move path. Both views are projections of it.
2. Switching tabs never changes it, the board, the graph marker or the candidate lines.
3. Clicking a Visual node or a Table row selects that path. The other view then re-renders around it.
4. Selecting an earlier row in Table, for example the fork row 3. I7, keeps the later moves visible in Visual as off-path nodes. **Proposal:** Table keeps the rows after the selection, dimmed, so a click does not shorten it. Today the table ends at the selected move, which was seen once after an Undo and not tested further.

## Narrow windows and panel toggles

See [narrow-window-and-panels.svg](narrow-window-and-panels.svg).

**Discovery (Current):** the toggles exist only under main menu → View, as one "Analysis Panel" entry and one "Move/Engine Log Panel" entry. There is no shortcut, no palette command and no on-screen cue once a panel is hidden (B11).

**Board space (Current, measured at 926×560):** default 290 px, logs hidden 382 px, both hidden 427 px. At the default size the numbers on the stones are about 8 px and the 4 candidate lines show 2.

**Proposal:**

1. Replace the full-width log pane with a 28 px **log bar** in the analysis column that shows the "Move Log" and "Engine Log" tabs. One click on either opens a drawer in that column on that tab. The board stays 382 px at 926×560.
2. The bar shows counts as text ("5 moves · 0 errors", "Engine Log · 1 error") so a failed start is not hidden (B5).
3. Split the View entry into **Analysis Panel**, **Move Log** and **Engine Log**, and add the same commands to the Ctrl+K palette.
4. A hidden Analysis panel leaves a 28 px rail labelled "Analysis". Clicking it restores the panel. Board about 395 px with the bar kept (an estimate, 427 px was measured with nothing below the board).
5. Below about 1000 px wide or 700 px high, start with the bar folded. A manual choice wins for the session.

## Open questions

- Should stale candidate rows after a crash or Stop be dimmed or cleared? Not decided here (B6).
- Should Visual grow left to right to fit long games? The mockup keeps today's top to bottom layout to limit change.
- Accelerators for the panel toggles are not chosen.
- Not reviewed: light theme, board sizes other than 15, MultiPV above 4, Renju forbidden-point marks, other languages, screen-reader behavior.

## Suggested follow-ups (not filed)

1. Labelled engine strip, header relabelling and the four-state chip.
2. Visual and Table current-move and branch cues.
3. Log bar and panel toggle commands.
