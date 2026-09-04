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

Sprint 8 (opened 2026-08-31, goal "Engine-log sticky-bottom + About-window rewrite") — pulled from Backlog:

- ✅ **UI-10.** Engine Log doesn't stay scrolled to the end while the engine is analysing — new streamed lines land off-screen; expected sticky-bottom during analysis — [detail](docs/todo/UI-10-engine-log-not-sticky-to-bottom-during-analysis.md)
- ✅ **UI-11.** Rewrite the About window: custom deliberate layout (logo + info column), add developer credit (Nguyen Minh), tech/build info, links & protocol; correct the app name to `RANLS`; keep `APP_VERSION` single-sourced (REL-02) — [detail](docs/todo/UI-11-about-window-rewrite.md)
- ✅ **UI-12.** Move Log doesn't auto-scroll to the newest move — same `Gtk::Overlay`-breaks-`Gtk::Scrollable` no-op that UI-10's second pass fixed for the Engine Log — [detail](docs/todo/UI-12-move-log-not-sticky-to-bottom.md) — pulled into Sprint 8 Active 2026-08-31

Sprint 9 (opened 2026-09-03, goal "WinGraph coverage + app-wide RANLS rename") — pulled from Backlog:

- ✅ **UI-13.** WinGraph skips one side's plies: per-node evals are written only for the position at `currentPath()` during a search, so with "Engine plays <side>" the opponent's plies stay NaN. Graph should record the returned win% for every analysed position regardless of side. Trace + candidate fixes in detail file (filed 2026-09-03 from user report) — [detail](docs/todo/UI-13-wingraph-record-eval-regardless-of-side.md) · [instruction](docs/instruction/UI-13-wingraph-record-eval-regardless-of-side.md)
- ✅ **NAME-01.** Consistent app-wide rename `"Rapfi Analysis"` → `RANLS`: window title (`src/main_window.cpp:114`), `style.css` header comment, GTK application id, and a future `.desktop` file. Split out of UI-11 (which renamed only the About dialog's own text) per user decision 2026-08-31 — [detail](docs/todo/NAME-01-app-wide-rename-ranls.md)

Sprint 10 (opened 2026-09-04, goal "Analyze Mode — continuous background analysis for full WinGraph coverage") — pulled from Backlog:

- ✅ **ANLZ-01.** Analyze Mode — continuous background analysis so WinGraph fills a real point for every position the user visits (the "Lizzie way"); no formula backfill on the plotted line. Orthogonal to "Engine plays". Design resolved — `features/analyze-mode/planning.md` Q1–Q8 accepted 2026-09-04. Supersedes the `GRAPH-xx` "evaluate the whole played line" idea. [Model: Sonnet 5] — shipped 2026-09-04 (PR #9, squash `0ae2b8a`); build clean, ctest 3/3, +`test_anlz01_analyze_mode_coverage.cpp` / `test_anlz01_analyze_mode_action.cpp` / +1 settings case; manual live-engine smoke still needs a human — [detail](docs/todo/ANLZ-01-continuous-analyze-mode.md) · [instruction](docs/instruction/ANLZ-01-continuous-analyze-mode.md) · [fix-log](docs/fix-log/2026-09-04-analyze-mode.md) · design `features/analyze-mode/`
- ✅ **ANLZ-04.** WinGraph: draw a faint dashed "bridge" segment connecting the two nearest evaluated plies across a NaN run, instead of breaking the line into disjoint segments. Always on, no gap-length cap; gap plies still get no dot and hover still reads "(no eval)". Deliberate refinement of UI-01's "disjoint segments" rule — needs a `docs/audit/` entry. [Model: Sonnet 5] — pulled from Backlog into Sprint 10 Active 2026-09-04 (mid-sprint, after ANLZ-01 shipped) — [detail](docs/todo/ANLZ-04-wingraph-bridge-nan-gaps.md) · [instruction](docs/instruction/ANLZ-04-wingraph-bridge-nan-gaps.md)

Sprint 11 (opened 2026-09-04, goal "New `.rdb` binary save format — persist the full variation tree + per-node analysis so a reloaded game keeps its WinGraph") — pulled from Backlog. Integration branch `feat/rdb-save-format` (sub-PRs merge into it; one PR back to `main`):

- ⛔ **ANLZ-03. SUPERSEDED** by RDB-01/02/03 (user decision 2026-09-04: reject extending `.yxgame`, introduce binary `.rdb` instead). Its goal (reloaded game keeps its WinGraph) + regression-test intent carry into RDB-03. — [detail](docs/todo/ANLZ-03-persist-winrate-in-save-file.md) · design [features/rdb-save-format/](features/rdb-save-format/)
- ✅ **RDB-01.** `.rdb` container framing (`"RDB1"` magic + header) + `ICompressor` (Raw / DEFLATE-over-zlib) + `GameGraph` serialisation DTO + hand-rolled CBOR payload codec + `VariationTree`↔`GameGraph` convert. Model-layer only, no UI. [Model: Sonnet 5] — [detail](docs/todo/RDB-01-rdb-container-and-codec.md) · [instruction](docs/instruction/RDB-01-rdb-container-and-codec.md)
- ✅ **RDB-02.** Wire `.rdb` into Save/Open via `IGameArchiveReader/Writer` + `RdbArchive` + `YxgameReader` (import-only) + extension factory; retire `GameIO::saveGame`; dialog filters. Depends on RDB-01. [Model: Sonnet 5] — [detail](docs/todo/RDB-02-wire-rdb-into-save-open.md) · [instruction](docs/instruction/RDB-02-wire-rdb-into-save-open.md)
- ✅ **RDB-03.** Persist + restore per-node analysis end-to-end (extend `TreeNode`, resolve the `evalHistory()` gate, full save→reopen→WinGraph-identical path) — **closes the original ANLZ-03 goal** + carries its NaN-round-trip / legacy-import / out-of-range regression tests. Depends on RDB-01+02. [Model: Sonnet 5] — [detail](docs/todo/RDB-03-persist-restore-node-analysis.md) · [instruction](docs/instruction/RDB-03-persist-restore-node-analysis.md)

## Backlog

- 🔲 **ANLZ-05.** Analyze Mode refinement (user report 2026-09-04 against shipped ANLZ-01): (1) while Analyze Mode is on the engine must **never** auto-move — not even on its own turn under "Engine plays &lt;side&gt;" — it only analyses; Stop just stops the search. **Reverses `features/analyze-mode/planning.md` Q6.** (2) A board click during an in-flight Analyze-Mode search stops the search, places the stone, and restarts analysis (today `makeMove()`'s `analyzing_` guard silently swallows it). `MainWindow`-layer only, orthogonal to ENG-02; one-shot Analyze / "Engine plays" with Analyze Mode off unchanged. — [detail](docs/todo/ANLZ-05-analyze-mode-no-automove-allow-mid-search-moves.md) · [instruction](docs/instruction/ANLZ-05-analyze-mode-no-automove-allow-mid-search-moves.md)
- 🔲 **ENG-03.** Engine subprocess can be orphaned when the window is closed via the WM close button ("X") or when the GUI crashes: `signal_close_request` is unwired (only the Quit menu/hotkey stops the engine), the heap `MainWindow` is never `delete`d, and there is no `PR_SET_PDEATHSIG`. Relies on the engine self-exiting on stdin EOF — a mid-search engine lingers, a non-compliant one leaks. Wire close-request → graceful stop + add PDEATHSIG. Builds on ENG-01, must not regress ENG-02. [Model: Sonnet 5] — [detail](docs/todo/ENG-03-orphaned-engine-on-crash-or-wm-close.md) · [instruction](docs/instruction/ENG-03-orphaned-engine-on-crash-or-wm-close.md)
- 🔲 **TOOL-02.** `check-task-structure.js` regexes (`BULLET_START_RE` / `TODO_LINE_RE`) only recognise `✅` or no marker — a `🔲` open-marker line is silently skipped, so an open Backlog/Active item with a detail file is falsely reported as an orphan. Add `🔲` (and `🚧`) to the marker alternation. [Model: Haiku 4.5] — _detail TBD_

Filed 2026-09-04 from the WinGraph-coverage discussion (`docs/notes/2026-09-04-wingraph-analyze-mode-and-backfill.md`)
after web/GitHub research into how Lizzie/LizzieYZY, Sabaki, KaTrain and En Croissant handle it —
user chose the continuous-analysis ("Lizzie way") approach. ANLZ-01's
`features/analyze-mode/planning.md` Q1–Q8 were resolved with the user 2026-09-04 (all 8 proposed
defaults accepted verbatim) — **ANLZ-01 pulled into Sprint 10 Active 2026-09-04** (see
`docs/sprint/current.md`).

The old **ANLZ-02** ("Analyze entire game" one-shot sweep) and the briefly-considered
"Toggle Ponder" idea were both **dropped 2026-09-04** — a CPU alpha-beta engine (Rapfi) is too
expensive to keep pondering the way Lizzie can with GPU KataGo, and ANLZ-04's connected graph
covers the discontinuity that motivated them. `ANLZ-02` is a retired code, not reused. ANLZ-04 was
filed to Backlog then **pulled into Sprint 10 Active 2026-09-04** (mid-sprint, after ANLZ-01 shipped —
see `docs/sprint/current.md`). ANLZ-03 (follows ANLZ-01) had its
`docs/todo/ANLZ-03-persist-winrate-in-save-file.md` + `docs/instruction/` detail files scaffolded
2026-09-04, then was **pulled from Backlog into Sprint 11 Active 2026-09-04** (see
`docs/sprint/current.md`) — Sprint 11 goal: make the per-position win% durable across save/load.
**Superseded 2026-09-04 during implementation discussion**: user rejected extending the `.yxgame`
text schema and chose a new binary `.rdb` (Ranls Database) format — CBOR payload + DEFLATE
container, whole variation tree + per-node analysis, open/versioned structure (tree not DAG,
single-game — reasoning in `features/rdb-save-format/planning.md`). ANLZ-03 → `⛔ SUPERSEDED`;
work re-split into `RDB-01` (container/codec/DTO), `RDB-02` (Save/Open wiring, `.yxgame`
import-only), `RDB-03` (per-node analysis persistence — closes ANLZ-03's goal). Sprint 11 re-planned
around `RDB-01..03` on integration branch `feat/rdb-save-format`.

Filed 2026-09-04 (ANLZ-05) from a user report against the shipped ANLZ-01: in Analyze Mode the
engine should never auto-move and a click should be accepted mid-search. Reverses planning Q6
(see `features/analyze-mode/planning.md` "Revision 2026-09-04"). Not yet pulled into a sprint.

Filed 2026-09-04 (ENG-03) from a user safety question — "if the program crashes / the user closes
normally or while analyzing, does the engine subprocess terminate correctly?" — plus a trace of the
engine lifecycle: only the Quit menu/hotkey and the C++ destructors guarantee a kill; the WM close
button is unwired and a GUI crash skips both, leaving termination to rely on the engine self-exiting
on stdin EOF. Not yet pulled into a sprint.

Filed 2026-09-04 (TOOL-02) — surfaced while scaffolding ANLZ-03: `check-task-structure.js` doesn't
recognise the `🔲` open-marker, so an open item with a detail file trips its orphan check. Not a
blocker for `check-tracking-sync.js` (the sprint-command gate), which passes.

Filed 2026-08-21 from a full read of `src/` (UI/UX + codebase review). Prefixes: `RT` realtime
pipeline · `STATE` state lifetime · `PROTO` engine protocol · `ENG` engine lifecycle ·
`NAV` navigation · `UI` display logic · `UX` usability · `TEST` harness · `CLEAN` hygiene ·
`IO` game persistence · `DOC` documentation · `TOOL` repo tooling · `REL` release/versioning.

Filed 2026-08-30 from a follow-up UI review request and from `features/versioning-and-changelog/`
(UI-08, ENG-02, UI-09, REL-01, REL-02) — all shipped in Sprint 7 (archived
`docs/sprint/archive/sprint-7.md`).

Filed 2026-08-31 from user bug report / request (UI-10, UI-11) — **both pulled into Sprint 8's
Active section 2026-08-31** (see above and `docs/sprint/current.md`).

Filed 2026-09-03 from a user report (UI-13, WinGraph coverage) — **pulled from Backlog into Sprint 9
Active 2026-09-03**, alongside NAME-01 (see above and `docs/sprint/current.md`).

Filed 2026-08-31 from UI-10's second pass (the Engine Log fix surfaced the same latent no-op in
the Move Log): UI-12 — **pulled from Backlog into Sprint 8 Active 2026-08-31** (see
`docs/sprint/current.md`).

Earlier: IO-01/DOC-01/TOOL-01/CLEAN-02 (filed 2026-08-30, leftover-task sweep) shipped in Sprint 5.
UI-04/UI-05/UX-06/UI-06 (filed 2026-08-30, UI review session) were committed straight into Sprint
6's Active section above (see `docs/sprint/current.md`). STATE-04 (filed 2026-08-30 from UI-06's
smoke pass) was likewise pulled straight into Sprint 6 Active.

