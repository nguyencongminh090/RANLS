# Changelog

All notable user-facing changes to YixinBoard are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
(currently in the `0.x` pre-1.0 series: MINOR = new user-visible feature, PATCH = fix/polish bundle).

Entries describe user-visible impact. A task `CODE` in parentheses (e.g. `UI-07`) points back to the
internal tracking files for traceability.

## [Unreleased]

### Changed

- **Analyze Mode** is now a pure study mode: while it is on the engine only ever analyses the
  current position and never auto-plays a move — not even on its own turn under "Engine plays
  &lt;side&gt;". Pressing Stop or turning Analyze Mode off just stops the search. With Analyze Mode
  off, "Engine plays" auto-move is unchanged (ANLZ-05).
- Clicking the board **during** an Analyze-Mode search now places the stone: the search stops, the
  move is applied, and analysis restarts on the new position (previously the click was silently
  ignored) (ANLZ-05).

## [0.3.0] - 2026-09-04

Sprint 11 — a new binary save format (`.rdb`) that keeps the whole analysis, not just the moves.

### Added

- Saved games now keep their **win-rate graph**: analyse a game, save it, re-open it later and the
  graph is back exactly as you left it — no need to re-run the engine (RDB-03).
- Saved games now keep the **whole variation tree** — every branch you explored and every comment,
  not only the line you played (RDB-01, RDB-02).

### Changed

- Games save as **`.rdb`** (Ranls Database — a compact binary format). Older `.yxgame` files still
  open and can be re-saved as `.rdb`; there is no `.yxgame` save any more (RDB-02).

## [0.2.0] - 2026-09-04

Sprint 10 — Analyze Mode: the engine can now keep the win-rate graph filled in as you review a game.

### Added

- **Analyze Mode**: a new toggle (in the "Engine plays" menu and as an "∞" button in the engine
  status bar) that keeps the engine re-analysing each position as you navigate or play into it, so
  the win-rate graph gets a real, measured point for every position you visit — without pressing
  Analyze on each one. Off by default, remembered between sessions, and independent of "Engine
  plays" (the two can both be on). Positions you never analyse still show as gaps — no guessed
  values are ever plotted (ANLZ-01).

### Changed

- Win-rate graph: a stretch of un-analysed moves no longer breaks the line into disconnected
  pieces. The gap is now spanned by a faint dashed bridge so the trace reads as one continuous
  line, while those moves still get no dot and still read "(no eval)" on hover (ANLZ-04).

## [0.1.2] - 2026-09-04

Sprint 9 — a win-rate-graph coverage fix plus completion of the app rename.

### Fixed

- The win-rate graph now plots a point for every position the engine evaluates, including the reply
  move it recommends. Previously, with "Engine plays <side>" active, one side's plies were left as
  gaps on the line (UI-13).

### Changed

- The application is now consistently named **RANLS** everywhere — window title, application
  identity, and internal resources — completing the rename that started with the About dialog
  (NAME-01).

## [0.1.1] - 2026-09-03

Sprint 8 — a UI polish bundle: the Engine Log and Move Log now follow along as new content arrives,
and the About window was rebuilt.

### Changed

- Help → About rebuilt: a deliberate custom layout showing the correct app name (RANLS), the
  version, developer credit, build information (toolkit versions, build date, commit), and links to
  the project and the engine protocol — replacing the bare stock dialog (UI-11).

### Fixed

- Engine Log now stays scrolled to the newest line while the engine is analysing, instead of
  leaving fresh output off-screen; scrolling up to read back still holds your position (UI-10).
- Move Log now auto-scrolls to the latest move as moves are played or a game is loaded (UI-12).
- Engine Log direction tags (SEND / MESSAGE / …) now render in the same font as the log text.

## [0.1.0] - 2026-08-31

First tagged release. Covers everything shipped through Sprint 6 — the initial hardening pass over
the engine pipeline, state lifetime, board rendering, and the analysis/settings UI.

### Added

- Load Game / Save Game: open and save a match to a versioned plain-text file, with a discard
  confirmation before loading over an in-progress game and a clear error message on a corrupt file
  (IO-01).
- "Engine plays" selector (Black / White / Off): let the engine take a side and move automatically
  instead of only analysing on demand (UI-06).
- Renju/Standard rule is now visible during play: a persistent header-bar rule indicator and
  forbidden-point marks for Black, with win detection that matches the selected rule (UI-03).

### Changed

- Settings dialog rebuilt as a resizable, tabbed dialog. The engine path is validated live with
  inline feedback (and Apply is disabled while it is invalid), a MultiPV control was added, and the
  Light/Dark theme and Show Coordinates options now actually take effect (STATE-02, UX-02, UX-06).
- Board size and rule now persist between launches; starting a new game keeps the current board size
  instead of snapping back to the default (STATE-04).
- Win-rate graph: thicker, higher-contrast line meeting contrast guidelines on both light and dark
  panels; SingleSide mode now always shows Black's perspective; evaluations are attributed to the
  correct side and unevaluated positions are distinguishable from a true 50% (UI-01, UI-09).
- Engine status now reports honest states (starting / thinking / idle / crashed), a crash is
  announced inline, and stopping the engine no longer freezes the UI for ~2.5s (ENG-01).
- Engine Log: direction tags moved into a non-copyable gutter column so copied rows contain only raw
  engine text; the log is now bounded and written in batches for smoother performance (RT-02, UI-05).
- Icon-only navigation buttons gained tooltips and accessible labels; New Game and board-size
  changes confirm before discarding a non-empty game (UX-03).
- Board rendering corrected across the full 5×5–22×22 size range: coordinate-label clipping at small
  sizes, move-number overflow at large sizes, star-point placement, and coordinate parsing on
  non-15×15 boards (PROTO-02, UX-04).
- Analysis updates are coalesced and throttled for a smoother UI during deep multi-PV searches;
  move-tree and PV views update incrementally rather than fully rebuilding (RT-01, RT-03, RT-04).
- Bulk navigation (undo-all / redo-all / jump to move) no longer floods the engine and UI with
  per-move updates (NAV-01).
- Split-pane divider positions are restored correctly after the window is shrunk and expanded again
  (UX-05).
- Tree "Table" tab now supports click-to-jump and highlights the current position; the two tree
  views are labelled to show they render different data (UI-02).
- Empty analysis, PV, move-tree, and log panels now render as a clean empty state (UX-01, UI-08).

### Fixed

- PV list no longer accumulates stale rows across positions or shows duplicate entries at MultiPV=1
  (UI-04, UI-07).
- Stale PV, engine-status, and board markers are cleared correctly on New Game, on making a move,
  and on undo/redo (STATE-01).
- Interrupting engine auto-play (Stop, or analysing manually on the engine's turn) now reverts
  "Engine plays" to Off for the session instead of leaving it on the assigned side; the persisted
  side is restored next launch (ENG-02).
- Engine-output parsing hardened against malformed data: out-of-range PV indices, oversized MultiPV
  values, stale PV slots, and invalid board coordinates (PROTO-01, STATE-03).
- Contrast fixes for coordinate labels against the board background and for database markers (UX-03).
- Leaked dialog windows and several dead menu/signal wirings fixed (CLEAN-01).

### Removed

- Redundant "Analysis" menu (it duplicated the toolbar) — replaced by the "Engine plays" selector
  (UI-06).
- "UI Profile" setting removed from Settings — it was undefined and had no effect (UX-06).
- Instructional placeholder text in empty panels removed in favour of a plain empty state (UI-08).

[Unreleased]: https://github.com/nguyencongminh090/RANLS/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/nguyencongminh090/RANLS/releases/tag/v0.3.0
[0.2.0]: https://github.com/nguyencongminh090/RANLS/compare/v0.1.2...v0.2.0
[0.1.2]: https://github.com/nguyencongminh090/RANLS/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/nguyencongminh090/RANLS/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/nguyencongminh090/RANLS/releases/tag/v0.1.0
