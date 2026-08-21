# TODO

Index for tracked work. See `CLAUDE.md` ("Process model") and `.claude/rules/tracking-files.md`
for the full convention before editing this file.

- **Backlog** — prioritized, not yet committed to a sprint.
- **Active** — committed to the current sprint (see `docs/sprint/current.md`).

Each line links to its detail file at `docs/todo/<CODE>-<slug>.md`. `CODE` = a 2-5 letter
feature-area prefix + running number (e.g. `WALL-01`, `UI-07`). `✅` marks a finished item — see
`.claude/rules/tracking-files.md` for the index/detail sync rule this implies.

---

## Active
- ✅ **TEST-01.** No test infrastructure exists — blocks the regression tests STATE-01 and PROTO-01 require [Model: Sonnet 5] — [detail](docs/todo/TEST-01-test-infrastructure.md)
- ✅ **PROTO-01.** Harden the Gomocup parser: out-of-bounds `currentPVs_[-1]` in `onPVDone`, unbounded `NUMPV` resize, unvalidated database coords [Model: Sonnet 5] — [detail](docs/todo/PROTO-01-parser-hardening.md)
- ✅ **STATE-01.** Stale PV / engine status / board markers survive New Game, makeMove, and undo/redo [Model: Sonnet 5] — [detail](docs/todo/STATE-01-stale-analysis-after-position-change.md)
- ✅ **RT-01.** No throttle anywhere on the engine→UI analysis path; 6 emit sites drive a full UI rebuild per parsed line [Model: Sonnet 5] — [detail](docs/todo/RT-01-throttle-analysis-signal.md)
- ✅ **STATE-02.** Settings dialog silently resets `multiPV` and wipes `customParams`, then persists it — [detail](docs/todo/STATE-02-settings-dialog-drops-config-fields.md)
- ✅ **ENG-01.** Engine state is dishonest ("● ON" with no process, crash ≡ never-started, no "thinking" state) and stopping blocks the UI ~2.5s — [detail](docs/todo/ENG-01-engine-state-honesty-and-blocking-stop.md)
- ✅ **RT-02.** Engine log grows unbounded and writes per-line; gutter labels desync on wrap — [detail](docs/todo/RT-02-engine-log-unbounded.md)
- ✅ **RT-03.** PVView full rebuild destroys hover, breaking the board PV ghost-stone preview during analysis — [detail](docs/todo/RT-03-pvview-rebuild-breaks-hover.md)
- ✅ **PROTO-02.** Hardcoded board size 15 in coordinate parsing, `Best:` readout, and star points — breaks every non-15×15 board — [detail](docs/todo/PROTO-02-hardcoded-board-size-15.md)
- ✅ **STATE-03.** `currentPVs_` never shrinks and materialises empty PV slots rendered as garbage rows — [detail](docs/todo/STATE-03-currentpvs-never-shrinks.md)
- ✅ **RT-04.** Both tree views fully rebuild many times per second during analysis; `layoutTree` is O(n²) — [detail](docs/todo/RT-04-tree-views-full-rebuild.md)
- ✅ **NAV-01.** `undoAll`/`redoAll` send one database query and rebuild the whole UI per ply — [detail](docs/todo/NAV-01-undoall-floods-engine-and-ui.md)
- ✅ **UI-01.** Win-rate graph attributes evals to the wrong side (off by one ply); evals can go unrecorded — [detail](docs/todo/UI-01-winrate-attribution-errors.md)
- ✅ **UI-02.** Tree "Table" tab can't click-to-jump and shows no current path; the two tree views disagree — [detail](docs/todo/UI-02-tree-view-parity.md)
- ✅ **UI-03.** Selected rule (Renju/Standard) has no effect on what the board shows — [detail](docs/todo/UI-03-rule-not-visible-on-board.md)
- ✅ **UX-01.** Three panels render as blank rectangles instead of empty states — [detail](docs/todo/UX-01-empty-states.md)
- ✅ **UX-02.** Settings dialog accepts an invalid engine path with no feedback — [detail](docs/todo/UX-02-settings-validation.md)
- **UX-03.** Unlabelled icon buttons, no focus indication on custom-drawn widgets, no confirmation before destroying a game — [detail](docs/todo/UX-03-accessibility-and-destructive-actions.md)
- ✅ **UX-04.** Board rendering never verified at the extremes of the supported 5–22 range (investigation) — [detail](docs/todo/UX-04-board-size-ergonomics.md)
- ✅ **CLEAN-01.** Leaked dialogs, dead signals, leftover debug output, duplicated constant — [detail](docs/todo/CLEAN-01-dialog-leaks-and-dead-code.md)

## Backlog

Filed 2026-08-21 from a full read of `src/` (UI/UX + codebase review). Prefixes: `RT` realtime
pipeline · `STATE` state lifetime · `PROTO` engine protocol · `ENG` engine lifecycle ·
`NAV` navigation · `UI` display logic · `UX` usability · `TEST` harness · `CLEAN` hygiene.

- **UX-05.** `Gtk::Paned` divider position doesn't rescale when the window is resized back up after being shrunk, leaving the board squeezed — [detail](docs/todo/UX-05-paned-resize-does-not-restore.md)
