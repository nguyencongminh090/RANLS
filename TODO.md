# TODO

Index for tracked work. See `CLAUDE.md` ("Process model") and `.claude/rules/tracking-files.md`
for the full convention before editing this file.

- **Backlog** — prioritized, not yet committed to a sprint.
- **Active** — committed to the current sprint (see `docs/sprint/current.md`).
- **Completed** — shipped items, kept only as links to their detail files. Per-sprint context
  (goal, committed `CODE`s, burndown, roll-overs) lives in `docs/sprint/archive/sprint-*.md`;
  per-fix detail in `docs/fix-log.md`; non-bug decisions in `docs/audit.md`.

Each line links to its detail file at `docs/todo/<CODE>-<slug>.md`. `CODE` = a 2-5 letter
feature-area prefix + running number. `✅` marks a finished item, `⛔` a superseded one — see
`.claude/rules/tracking-files.md` for the index/detail sync rule.

Prefix legend: `RT` realtime pipeline · `STATE` state lifetime · `PROTO` engine protocol ·
`ENG` engine lifecycle · `NAV` navigation · `UI` display logic · `UX` usability · `TEST` harness ·
`CLEAN` hygiene · `IO` game persistence · `DOC` documentation · `TOOL` repo tooling ·
`REL` release/versioning · `PORT` cross-platform portability · `ANLZ` analyze mode ·
`RDB` `.rdb` save format · `NAME` app naming · `CONS` engine-log command console.

---

## Active

Sprint 16 (opened 2026-09-07, goal "Engine Log command console: autocomplete + autocorrect") —
pulled from Backlog:

- 🔲 **CONS-01.** Engine Log command entry: AutoComplete (command suggestion — popover + ghost-text) — [detail](docs/todo/CONS-01-console-command-autocomplete.md) · [instruction](docs/instruction/CONS-01-console-command-autocomplete.md)
- 🔲 **CONS-02.** Engine Log command entry: AutoCorrect (missing `!`, wrong case, "did you mean") — depends on CONS-01 — [detail](docs/todo/CONS-02-console-syntax-autocorrect.md) · [instruction](docs/instruction/CONS-02-console-syntax-autocorrect.md)

## Backlog

No open items — everything filed to date is committed to a sprint or has shipped. New work enters
the Backlog via `docs/notes/` → `features/<slug>/` → `docs/todo/<CODE>-<slug>.md` + a Backlog line.

Filed 2026-09-07 from the `features/console-autocomplete/` design (Q1–Q11 resolved with the user
the same day) — **pulled into Sprint 16 Active 2026-09-07** (see `docs/sprint/current.md`).

## Completed

Shipped in Sprints 1–15 (all archived in `docs/sprint/archive/`):

- ✅ **RT-01.** throttle the engine→UI analysis signal — [detail](docs/todo/RT-01-throttle-analysis-signal.md)
- ✅ **RT-02.** engine log unbounded / per-line writes / gutter desync — [detail](docs/todo/RT-02-engine-log-unbounded.md)
- ✅ **RT-03.** PVView rebuild breaks board PV ghost-stone hover — [detail](docs/todo/RT-03-pvview-rebuild-breaks-hover.md)
- ✅ **RT-04.** tree views full-rebuild per line; O(n²) layoutTree — [detail](docs/todo/RT-04-tree-views-full-rebuild.md)
- ✅ **STATE-01.** stale analysis survives position change — [detail](docs/todo/STATE-01-stale-analysis-after-position-change.md)
- ✅ **STATE-02.** settings dialog drops multiPV / customParams — [detail](docs/todo/STATE-02-settings-dialog-drops-config-fields.md)
- ✅ **STATE-03.** `currentPVs_` never shrinks; garbage empty rows — [detail](docs/todo/STATE-03-currentpvs-never-shrinks.md)
- ✅ **STATE-04.** rule and board size not persisted across launches — [detail](docs/todo/STATE-04-rule-and-board-size-not-persisted.md)
- ✅ **PROTO-01.** Gomocup parser hardening (OOB / unbounded NUMPV / coords) — [detail](docs/todo/PROTO-01-parser-hardening.md)
- ✅ **PROTO-02.** hardcoded board size 15 in coord parsing / readout / stars — [detail](docs/todo/PROTO-02-hardcoded-board-size-15.md)
- ✅ **PROTO-03.** open protocol extension (`.ptc` runtime commands) — [detail](docs/todo/PROTO-03-open-protocol-extension.md)
- ✅ **ENG-01.** engine state honesty + non-blocking stop — [detail](docs/todo/ENG-01-engine-state-honesty-and-blocking-stop.md)
- ✅ **ENG-02.** interrupted engine auto-play reverts to manual analyze — [detail](docs/todo/ENG-02-engine-play-interrupted-reverts-to-manual.md)
- ✅ **ENG-03.** orphaned engine on crash / WM close (close-request + PDEATHSIG) — [detail](docs/todo/ENG-03-orphaned-engine-on-crash-or-wm-close.md)
- ✅ **NAV-01.** `undoAll`/`redoAll` flood engine and UI per ply — [detail](docs/todo/NAV-01-undoall-floods-engine-and-ui.md)
- ✅ **UI-01.** win-rate graph off-by-one attribution; unrecorded evals — [detail](docs/todo/UI-01-winrate-attribution-errors.md)
- ✅ **UI-02.** tree Table/Tree view parity (click-to-jump, current path) — [detail](docs/todo/UI-02-tree-view-parity.md)
- ✅ **UI-03.** selected rule not visible on the board — [detail](docs/todo/UI-03-rule-not-visible-on-board.md)
- ✅ **UI-04.** PV view appends lines across positions — [detail](docs/todo/UI-04-pv-view-appends-across-positions.md)
- ✅ **UI-05.** engine log direction tag → fixed-width non-copyable gutter — [detail](docs/todo/UI-05-engine-log-direction-gutter-column.md)
- ✅ **UI-06.** repurpose "Analysis" menu → "Engine plays" (`MatchConfig`) — [detail](docs/todo/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md)
- ✅ **UI-07.** PV panel still accumulates a stale row per position — [detail](docs/todo/UI-07-pv-panel-still-accumulates-across-positions.md)
- ✅ **UI-08.** remove empty-state placeholder text (partial UX-01 reversal) — [detail](docs/todo/UI-08-remove-empty-state-placeholder-text.md)
- ✅ **UI-09.** WinGraph SingleSide=Black; thicker high-contrast line — [detail](docs/todo/UI-09-wingraph-single-side-black-and-thicker-line.md)
- ✅ **UI-10.** engine log not sticky-to-bottom during analysis — [detail](docs/todo/UI-10-engine-log-not-sticky-to-bottom-during-analysis.md)
- ✅ **UI-11.** About window rewrite — [detail](docs/todo/UI-11-about-window-rewrite.md)
- ✅ **UI-12.** move log not sticky-to-bottom (Overlay breaks Scrollable) — [detail](docs/todo/UI-12-move-log-not-sticky-to-bottom.md)
- ✅ **UI-13.** WinGraph record eval regardless of side — [detail](docs/todo/UI-13-wingraph-record-eval-regardless-of-side.md)
- ✅ **UI-15.** move log sticky-bottom races GTK4 kinetic-scroll animation — [detail](docs/todo/UI-15-move-log-scroll-races-kinetic-animation.md)
- ✅ **UX-01.** empty states for blank panels — [detail](docs/todo/UX-01-empty-states.md)
- ✅ **UX-02.** settings dialog engine-path validation — [detail](docs/todo/UX-02-settings-validation.md)
- ✅ **UX-03.** accessibility + confirm before destroying a game — [detail](docs/todo/UX-03-accessibility-and-destructive-actions.md)
- ✅ **UX-04.** board rendering at the extremes of the 5–22 range (investigation) — [detail](docs/todo/UX-04-board-size-ergonomics.md)
- ✅ **UX-05.** `Gtk::Paned` divider doesn't restore on window regrow — [detail](docs/todo/UX-05-paned-resize-does-not-restore.md)
- ✅ **UX-06.** settings "UI Setting" section broken/unclear + reorganise — [detail](docs/todo/UX-06-settings-dialog-ui-section-broken-and-unclear.md)
- ✅ **TEST-01.** test infrastructure — [detail](docs/todo/TEST-01-test-infrastructure.md)
- ✅ **CLEAN-01.** leaked dialogs, dead signals, debug output, dup constant — [detail](docs/todo/CLEAN-01-dialog-leaks-and-dead-code.md)
- ✅ **CLEAN-02.** build artifacts + `.gitignore` — [detail](docs/todo/CLEAN-02-build-artifacts-and-gitignore.md)
- ✅ **IO-01.** implement Load/Save Game (were empty stubs) — [detail](docs/todo/IO-01-load-save-game.md)
- ✅ **DOC-01.** README GTK3→GTK4 mismatch — [detail](docs/todo/DOC-01-readme-gtk-mismatch.md)
- ✅ **TOOL-01.** wire `check-tracking-sync.js` as a Stop hook — [detail](docs/todo/TOOL-01-wire-tracking-sync-hook.md)
- ✅ **TOOL-02.** `check-task-structure.js` recognise `🔲`/`🚧` markers — [detail](docs/todo/TOOL-02-check-task-structure-markers.md)
- ✅ **TOOL-03.** `check-task-structure.js` handle `⛔`/SUPERSEDED lines — [detail](docs/todo/TOOL-03-superseded-marker-lines.md)
- ✅ **REL-01.** root `CHANGELOG.md` + release checklist + `v0.1.0` — [detail](docs/todo/REL-01-changelog-and-release-checklist.md)
- ✅ **REL-02.** single-source the version string (`configure_file` → `version.h`) — [detail](docs/todo/REL-02-version-string-single-source.md)
- ✅ **NAME-01.** app-wide rename → `RANLS` — [detail](docs/todo/NAME-01-app-wide-rename-ranls.md)
- ✅ **ANLZ-01.** continuous Analyze Mode (full WinGraph coverage) — [detail](docs/todo/ANLZ-01-continuous-analyze-mode.md)
- ⛔ **ANLZ-03. SUPERSEDED** by RDB-01/02/03 (goal carried into RDB-03) — [detail](docs/todo/ANLZ-03-persist-winrate-in-save-file.md)
- ✅ **ANLZ-04.** WinGraph dashed "bridge" segment across NaN gaps — [detail](docs/todo/ANLZ-04-wingraph-bridge-nan-gaps.md)
- ✅ **ANLZ-05.** Analyze Mode: never auto-move; accept mid-search clicks — [detail](docs/todo/ANLZ-05-analyze-mode-no-automove-allow-mid-search-moves.md)
- ✅ **ANLZ-06.** Analyze Mode search plays a stray move (`SearchIntent` gate) — [detail](docs/todo/ANLZ-06-analyze-mode-search-plays-stray-move.md)
- ✅ **ANLZ-07.** Analyze Mode restart busy-loop (`analysisConverged()`) — [detail](docs/todo/ANLZ-07-analyze-mode-restart-busy-loop.md)
- ✅ **RDB-01.** `.rdb` container framing + codec + `GameGraph` DTO — [detail](docs/todo/RDB-01-rdb-container-and-codec.md)
- ✅ **RDB-02.** wire `.rdb` into Save/Open; `.yxgame` import-only — [detail](docs/todo/RDB-02-wire-rdb-into-save-open.md)
- ✅ **RDB-03.** persist + restore per-node analysis end-to-end (closes ANLZ-03) — [detail](docs/todo/RDB-03-persist-restore-node-analysis.md)
- ✅ **PORT-01.** Tier-1 Windows build/runtime fixes (guarded) — [detail](docs/todo/PORT-01-cross-platform-build-and-runtime.md)
- ✅ **PORT-02.** bundle `style.css` via GResource — [detail](docs/todo/PORT-02-bundle-style-css-via-gresource.md)
- ✅ **PORT-03.** native MSVC build + portable test harness (`mock_engine`) — [detail](docs/todo/PORT-03-msvc-build-and-portable-test-harness.md)
