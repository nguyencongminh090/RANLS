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
- ✅ **UX-03.** Unlabelled icon buttons, no focus indication on custom-drawn widgets, no confirmation before destroying a game — [detail](docs/todo/UX-03-accessibility-and-destructive-actions.md)
- ✅ **UX-04.** Board rendering never verified at the extremes of the supported 5–22 range (investigation) — [detail](docs/todo/UX-04-board-size-ergonomics.md)
- ✅ **CLEAN-01.** Leaked dialogs, dead signals, leftover debug output, duplicated constant — [detail](docs/todo/CLEAN-01-dialog-leaks-and-dead-code.md)
- ✅ **UX-05.** `Gtk::Paned` divider position doesn't rescale when the window is resized back up after being shrunk, leaving the board squeezed — [detail](docs/todo/UX-05-paned-resize-does-not-restore.md)
- ✅ **IO-01.** `onLoadGame()`/`onSaveGame()` are empty stubs — Load/Save Game silently do nothing [Model: Sonnet 5] — [detail](docs/todo/IO-01-load-save-game.md)
- ✅ **DOC-01.** README.md claims GTK3; project actually targets GTK4 [Model: Haiku 4.5] — [detail](docs/todo/DOC-01-readme-gtk-mismatch.md)
- ✅ **TOOL-01.** `check-tracking-sync.js` still isn't wired as a `Stop` hook [Model: Haiku 4.5] — [detail](docs/todo/TOOL-01-wire-tracking-sync-hook.md)
- ✅ **CLEAN-02.** Uncommitted `build.sh` mode change; `build/`/`build_dist/` untracked and ungitignored [Model: Haiku 4.5] — [detail](docs/todo/CLEAN-02-build-artifacts-and-gitignore.md)
- ✅ **UI-04.** PV view appends lines across positions instead of replacing; shows multiple `PV #1` rows with MultiPV=1 — [detail](docs/todo/UI-04-pv-view-appends-across-positions.md)
- ✅ **UI-05.** Engine Log: move the direction tag (`[SEND]`/`[MESSAGE]`/…) into a fixed-width non-copyable gutter column so row copies contain only engine text — [detail](docs/todo/UI-05-engine-log-direction-gutter-column.md)
- ✅ **UX-06.** Settings "UI Setting" section: Show Coordinates and Light/Dark do nothing, WinGraph Mode unclear/misrendering, UI Profile undefined; plus organise the dialog — [detail](docs/todo/UX-06-settings-dialog-ui-section-broken-and-unclear.md)
- ✅ **UI-06.** Rename the redundant "Analysis" menu to "Engine plays" (Black/White/Off auto-move selector); new `MatchConfig`. Design resolved with user 2026-08-30 — [detail](docs/todo/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md)
- ✅ **UI-07.** PV panel still accumulates a stale row per analysed position (real `MESSAGE depth …` format; UI-04's fix missed this) — [detail](docs/todo/UI-07-pv-panel-still-accumulates-across-positions.md)
- ✅ **STATE-04.** Rule and board size are never persisted — reset to Freestyle / default on every launch (found during UI-06 smoke); design resolved with user 2026-08-30 [Model: Sonnet 5] — [detail](docs/todo/STATE-04-rule-and-board-size-not-persisted.md)

Sprint 7 (opened 2026-08-31, goal "UI polish + release prep") — pulled from Backlog:

- ✅ **UI-08.** Remove the empty-state placeholder text ("No moves yet", "No analysis yet", …); keep panels clean/empty — partial reversal of UX-01 — [detail](docs/todo/UI-08-remove-empty-state-placeholder-text.md)
- ✅ **ENG-02.** Interrupting engine auto-play (Stop / manual analyze on the engine's turn) reverts "Engine plays" to Off (manual analyze) — builds on UI-06 — [detail](docs/todo/ENG-02-engine-play-interrupted-reverts-to-manual.md)
- ✅ **UI-09.** Win-rate graph: SingleSide is always Black (drop the follow-engine-side coupling from UX-06, write notes); keep BothSide; make the win-rate line thicker + higher-contrast (colour-theory / WCAG pass) — [detail](docs/todo/UI-09-wingraph-single-side-black-and-thicker-line.md)
- ✅ **REL-01.** No user-facing version history: create root `CHANGELOG.md` ("Keep a Changelog", SemVer 0.x), backfill Sprints 1–6, add a "cut a release" checklist + tag `v0.1.0`; doc/process only — [detail](docs/todo/REL-01-changelog-and-release-checklist.md)
- ✅ **REL-02.** Version string disagrees across CMake (`1.0.0`), About dialog (`"2.0"`), and git (no tags): single-source it via `configure_file` → `version.h`, wire into About + a pre-GTK `--version` flag. Depends on REL-01 — [detail](docs/todo/REL-02-version-string-single-source.md)

## Backlog

Filed 2026-08-21 from a full read of `src/` (UI/UX + codebase review). Prefixes: `RT` realtime
pipeline · `STATE` state lifetime · `PROTO` engine protocol · `ENG` engine lifecycle ·
`NAV` navigation · `UI` display logic · `UX` usability · `TEST` harness · `CLEAN` hygiene ·
`IO` game persistence · `DOC` documentation · `TOOL` repo tooling · `REL` release/versioning.

Filed 2026-08-30 from a follow-up UI review request and from `features/versioning-and-changelog/`
(UI-08, ENG-02, UI-09, REL-01, REL-02) — **all pulled into Sprint 7's Active section 2026-08-31**
(see above and `docs/sprint/current.md`). Backlog is otherwise empty.

Earlier: IO-01/DOC-01/TOOL-01/CLEAN-02 (filed 2026-08-30, leftover-task sweep) shipped in Sprint 5.
UI-04/UI-05/UX-06/UI-06 (filed 2026-08-30, UI review session) were committed straight into Sprint
6's Active section above (see `docs/sprint/current.md`). STATE-04 (filed 2026-08-30 from UI-06's
smoke pass) was likewise pulled straight into Sprint 6 Active.

