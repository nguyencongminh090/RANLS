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
`RDB` `.rdb` save format · `NAME` app naming · `CONS` engine-log command console · `ARCH` architecture/layering refactors · `PAL` command palette (Ctrl+K) · `I18N` UI language / localisation.

---

## Active

Sprint 23 (opened 2026-10-08, goal "Command palette (Ctrl+K): bilingual classical-NLP search over actions, settings and ! commands, benchmarked against a Whoosh baseline") — pulled from Backlog:

- ✅ **PAL-01.** Pure en/vi search engine (BM25F) + shared dataset + Whoosh benchmark — [detail](docs/todo/PAL-01-palette-search-engine-and-benchmark.md) · [instruction](docs/instruction/PAL-01-palette-search-engine-and-benchmark.md)
- ✅ **PAL-02.** Ctrl+K overlay + action/`!`-command registries — [detail](docs/todo/PAL-02-palette-overlay-ui-and-registries.md) · [instruction](docs/instruction/PAL-02-palette-overlay-ui-and-registries.md)
- ✅ **PAL-03.** Declarative settings registry so settings are searchable — [detail](docs/todo/PAL-03-settings-registry-for-palette.md) · [instruction](docs/instruction/PAL-03-settings-registry-for-palette.md)

Sprint 24 (opened 2026-10-08, goal "Palette scopes (`!` `>` `@`) and a switchable UI language (English / Tiếng Việt)") — pulled from Backlog:

- 🔲 **PAL-04.** Palette: UI items vs console commands + scope prefixes `!` / `>` / `@` — [detail](docs/todo/PAL-04-palette-scoped-search-syntax.md) · [instruction](docs/instruction/PAL-04-palette-scoped-search-syntax.md)
- 🔲 **I18N-01.** GTK-free TSV catalog loader `i18n::tr` + lint tests — [detail](docs/todo/I18N-01-catalog-loader-and-lint.md) · [instruction](docs/instruction/I18N-01-catalog-loader-and-lint.md)
- 🔲 **I18N-02.** Wrap UI strings in `tr()` + Vietnamese catalog + live refresh — [detail](docs/todo/I18N-02-wire-ui-strings.md) · [instruction](docs/instruction/I18N-02-wire-ui-strings.md)
- 🔲 **I18N-03.** Language setting (System/English/Tiếng Việt), persistence, system default — [detail](docs/todo/I18N-03-language-setting-and-persistence.md) · [instruction](docs/instruction/I18N-03-language-setting-and-persistence.md)

## Backlog

PAL-04 (filed 2026-10-08 from the Sprint 23 follow-up discussion) — **pulled into Sprint 24 Active 2026-10-08** (see `docs/sprint/current.md`).


Filed 2026-10-08 from `features/ui-language/` (Q1–Q11 resolved the same day): I18N-01/02/03 — **pulled into Sprint 24 Active 2026-10-08**; I18N-04 stays here:

- 🔲 **I18N-04.** UI language: palette titles from the catalog (display follows UI language; search stays bilingual) — [detail](docs/todo/I18N-04-palette-titles-from-catalog.md)

Filed 2026-10-08 from `features/command-palette/` (user request; Q1–Q10 resolved the same day): PAL-01/02/03 — **pulled into Sprint 23 Active 2026-10-08** (see `docs/sprint/current.md`).

Filed 2026-10-06 from [board marks review audit](docs/audit/2026-10-06-board-marks-ux-review.md) — UI-16/17/18 shipped 2026-10-06 outside a sprint (PRs #46–#48, see Completed); UX-07 and UX-08 **pulled into Sprint 22 Active 2026-10-06** (see `docs/sprint/current.md`).

Filed 2026-10-05 from [architecture review audit](docs/audit/2026-10-05-architecture-review.md) — ARCH-01 and ARCH-03 **pulled into Sprint 20 Active 2026-10-05** (see `docs/sprint/current.md`). ARCH-02 design resolved 2026-10-06 (`features/extract-mainwindow-orchestration/`); split into ARCH-04 / ARCH-02 / ARCH-05 and **pulled into Sprint 21 Active 2026-10-06** (see `docs/sprint/current.md`).

Filed 2026-09-07 from `docs/notes/2026-09-07-pv-multipv-display-root-cause.md`: PROTO-05 — pulled into Sprint 17 Active 2026-09-07 (shipped, PR #32).
Filed 2026-09-08 from `docs/notes/2026-09-08-refyxb-multipv-rendering.md`: PROTO-06 — **pulled into Sprint 18 Active 2026-09-08** (see `docs/sprint/current.md`).
Filed 2026-09-09 from this session's YXANALZ feasibility discussion (Rapfi_V2 `docs/rules/YXANALZ-user-root-candidates.md`): PROTO-07 — design resolved 2026-09-09 (8 questions), **pulled into Sprint 19 Active 2026-09-09** (see `docs/sprint/current.md`).
Filed 2026-09-09 from this session (user request right after PROTO-07 merged): CONS-03 — `!help` doc for `!yxAnalz` / YXANALZ; **pulled into Sprint 19 Active 2026-09-09** (see `docs/sprint/current.md`).

## Completed

Shipped in Sprints 1–16 (all archived in `docs/sprint/archive/`):

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
- ✅ **CONS-01.** Engine Log command entry: AutoComplete (popover + ghost-text) — [detail](docs/todo/CONS-01-console-command-autocomplete.md) · [instruction](docs/instruction/CONS-01-console-command-autocomplete.md)
- ✅ **CONS-02.** Engine Log command entry: AutoCorrect (missing `!`, case, "did you mean") — [detail](docs/todo/CONS-02-console-syntax-autocorrect.md) · [instruction](docs/instruction/CONS-02-console-syntax-autocorrect.md)

Sprint 17:

- ✅ **PROTO-05.** enable engine incremental analysis stream (YXSHOWINFO + configurable `INFO SHOW_DETAIL`) — [detail](docs/todo/PROTO-05-enable-incremental-analysis-stream.md) · [instruction](docs/instruction/PROTO-05-enable-incremental-analysis-stream.md)

Sprint 18:

- ✅ **PROTO-06.** replace the analysis board overlay with the Yixin-Board live per-cell model (winrate/mate tags from `INFO PV DONE` + `REALTIME` `LOST`/`BEST`/`REFRESH`, live-only, two View toggles; deletes `candidateMoves` / `drawCandidateMoves`) — [detail](docs/todo/PROTO-06-port-realtime-feed-to-board.md) · [instruction](docs/instruction/PROTO-06-port-realtime-feed-to-board.md)

Sprint 19:

- ✅ **PROTO-07.** native `!yxAnalz` console command — analyze an explicit allow-list of root moves (Rapfi `YXANALZ`). First-class feature (not `.ptc`). Alphabetic move text only (numeric `x,y` deferred — `parseMovesText` out of bounds to change). — [detail](docs/todo/PROTO-07-yxanalz-root-move-allowlist.md) · [instruction](docs/instruction/PROTO-07-yxanalz-root-move-allowlist.md)
- ✅ **CONS-03.** `!help` documents `!yxAnalz` / YXANALZ as an engine-protocol extension (needs a supporting engine; real interruptible search; places no stone; alphabetic move text only). Help-text only, no behaviour change. Follow-on to PROTO-07. — [detail](docs/todo/CONS-03-help-doc-yxanalz-protocol-extension.md) · [instruction](docs/instruction/CONS-03-help-doc-yxanalz-protocol-extension.md)
- ✅ **DOC-02.** standardize `CLAUDE.md` — hard-rules section, GTK4 fix, bug-fix/sprint workflows moved to path-scoped `.claude/rules/`, GitHub model to the `github` skill (filed + done 2026-10-05, no sprint) — [detail](docs/todo/DOC-02-standardize-claude-md.md)

Sprint 20:

- ✅ **ARCH-01.** Break the model ↔ engine include cycle (move analysis data types into `model/`) [2 pts] — [detail](docs/todo/ARCH-01-break-model-engine-include-cycle.md)
- ✅ **ARCH-03.** Narrow `CommandContext` + split `registerBuiltins()` [3 pts] — [detail](docs/todo/ARCH-03-narrow-command-context-split-dispatcher.md)

Sprint 21:

- ✅ **ARCH-04.** Characterization tests for analyze / auto-move behavior (tests only, before any extraction) [2 pts] — [detail](docs/todo/ARCH-04-characterize-analyze-autoplay-behavior.md) · [instruction](docs/instruction/ARCH-04-characterize-analyze-autoplay-behavior.md)
- ✅ **ARCH-02.** Extract `AnalysisCoordinator` (analyze-restart / auto-move / ENG-02 revert) out of `MainWindow` into `src/engine/` [4 pts] — [detail](docs/todo/ARCH-02-extract-analysis-coordination-from-mainwindow.md) · [instruction](docs/instruction/ARCH-02-extract-analysis-coordination-from-mainwindow.md)
- ✅ **ARCH-05.** Extract GTK-free `GameFileService` (save/load logic) out of `MainWindow` [2 pts] — [detail](docs/todo/ARCH-05-extract-game-file-service.md) · [instruction](docs/instruction/ARCH-05-extract-game-file-service.md)

Shipped 2026-10-06 outside a sprint (bug fixes from the board marks review, PRs #46–#48):

- ✅ **UI-16.** hover and PV ghost stones draw over occupied cells — [detail](docs/todo/UI-16-hover-and-pv-ghost-draw-over-occupied-cells.md)
- ✅ **UI-17.** BoardRenderer layers leak Cairo state (font face) — [detail](docs/todo/UI-17-renderer-cairo-state-leak.md)
- ✅ **UI-18.** best-move mark hidden whenever a winrate tag is shown — [detail](docs/todo/UI-18-best-move-mark-hidden-by-winrate-tag.md)

Sprint 22:

- ✅ **UX-07.** redesign board mark language (engine / database / variant / best) — size L, design gate `features/board-marks/` first — [detail](docs/todo/UX-07-board-mark-visual-language.md) (PR #49)
- ✅ **UX-08.** board hover crosshair, margin highlight, mark tooltips (after UX-07) — [detail](docs/todo/UX-08-board-hover-crosshair-and-tooltips.md) (PR #50)
- ✅ **UI-21.** modern chrome: icon header bar, hamburger menu, CSS tokens — size L, design gate `features/modern-ui/` — [detail](docs/todo/UI-21-modern-chrome-icons-css.md)
