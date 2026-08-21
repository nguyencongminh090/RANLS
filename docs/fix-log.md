# Fix log

Append-only index of bug fixes, whether or not they started as a tracked `TODO.md` item. One new
`docs/fix-log/<date>-<slug>.md` detail file per fix, one new row here. Never edit, reorder, or
delete an existing row — a wrong past entry gets a new correcting entry, not a rewrite.

| Timestamp | Summary | Detail |
|---|---|---|
| 2026-08-21 | Stale PV/engine-status/board markers survived New Game, makeMove, undo, redo, gotoMove, gotoPath (STATE-01) | [detail](fix-log/2026-08-21-state-01-stale-analysis-after-position-change.md) |
| 2026-08-21 | Hardened Gomocup parser against malformed engine output: OOB `currentPVs_[-1]`, unbounded `NUMPV` resize, unvalidated database coords (PROTO-01) | [detail](fix-log/2026-08-21-proto-01-parser-hardening.md) |
| 2026-08-21 | Coalesced `GameState::signal_engine_analysis` onto a ~75ms UI timer with flush-on-search-completion instead of emitting once per parsed engine line (up to 8x/depth with multiPV=8); cached `evalHistory()` (RT-01) | [detail](fix-log/2026-08-21-rt-01-throttle-analysis-signal.md) |
| 2026-08-21 | `PVView::update()` now reuses row widgets in place instead of destroying/recreating every row (and its hover controller) per analysis update, fixing the PV-hover ghost-stone preview flicker (RT-03) | [detail](fix-log/2026-08-21-rt-03-pvview-rebuild-breaks-hover.md) |
| 2026-08-21 | Engine Log now uses one tagged-prefix `TextView` (removing the gutter/content wrap desync) backed by a bounded, batched `EngineLogModel` instead of unbounded per-line inserts (RT-02) | [detail](fix-log/2026-08-21-rt-02-engine-log-unbounded.md) |
| 2026-08-21 | `SettingsDialog::onApply` now copies from the config it was opened with instead of default-constructing, so `customParams`/`showDatabase` survive Apply; added a `multiPV` control and disk persistence for `customParams` (STATE-02) | [detail](fix-log/2026-08-21-state-02-settings-dialog-drops-config-fields.md) |
| 2026-08-21 | Replaced `EngineController`'s `started_`/`analyzing_` bool pair with an explicit `EngineState` enum (honoring `EngineProcess::start()`'s return value, distinct crashed/thinking states, active crash banner) and made engine shutdown fully non-blocking, removing both `g_usleep` waits and the re-entrant `g_main_context_iteration` pump from the stop path (ENG-01) | [detail](fix-log/2026-08-21-eng-01-engine-state-honesty-and-blocking-stop.md) |
| 2026-08-21 | `GameState::setAnalysisData` no longer emits `signal_tree_updated` synchronously per parsed engine line; coalesced onto RT-01's tick/flush via a new `treeDirty_` flag, `TreeExplorer::update` diffs rows into its `Gio::ListStore` with `splice()` instead of `remove_all()`+re-append, and `TreeNodeView::layoutTree` threads the parent path/index down through recursion instead of scanning `nodes_` for it, making layout O(n) instead of O(n²) (RT-04) | [detail](fix-log/2026-08-21-rt-04-tree-views-full-rebuild.md) |
