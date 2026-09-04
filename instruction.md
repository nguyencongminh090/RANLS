# Instruction — reviewer/planner execution guidance per TODO.md item

`TODO.md` lists *what* to do. This file holds the *how* — approach, specific pitfalls, and
boundaries ("do not touch") for the same items, one entry per `CODE`, matching `TODO.md`.

Before implementing any `TODO.md` item, read its matching entry here (missing entry is fine — not
every task needs one). If a fix deviates from what's written here, note why in the fix's summary.

---

## RT-01 — throttle-analysis-signal

Coalesce engine→UI updates without dropping the last one; do STATE-01 first.
[detail](docs/instruction/RT-01-throttle-analysis-signal.md)

## STATE-01 — stale-analysis-after-position-change

One shared reset path, not six copies; needs TEST-01.
[detail](docs/instruction/STATE-01-stale-analysis-after-position-change.md)

## PROTO-01 — parser-hardening

Trust-boundary hardening only — must not change how valid lines are interpreted.
[detail](docs/instruction/PROTO-01-parser-hardening.md)

## ENG-01 — engine-state-honesty-and-blocking-stop

Replace the state bools with an enum first; remove the re-entrant `g_main_context_iteration`.
[detail](docs/instruction/ENG-01-engine-state-honesty-and-blocking-stop.md)

## ENG-02 — engine-play-interrupted-reverts-to-manual

Additive on UI-06: a quiet `enginePlays -> Off` revert on Stop / Analyze-on-engine's-turn; no
persistence. Extract the "engine's turn" predicate as a pure function and unit-test it.
[detail](docs/instruction/ENG-02-engine-play-interrupted-reverts-to-manual.md)

## TEST-01 — test-infrastructure

Header-only framework, model/protocol only, no display server; prove the harness and stop.
[detail](docs/instruction/TEST-01-test-infrastructure.md)

## UI-06 — analysis-menu-duplicate-repurpose-to-player-assignment

Rename "Analysis" menu → "Engine plays" (Black/White/Off radio); auto-move semantics; new
`MatchConfig`. Design questions resolved with user 2026-08-30 — do not re-open.
[detail](docs/instruction/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md)

## UX-06 — settings-dialog-ui-section-broken-and-unclear

Fix Show Coordinates + theme wiring, relabel/reimplement WinGraph modes, remove `uiProfile`,
section the dialog. Depends on UI-06's `MatchConfig` for the WinGraph "Auto" perspective.
[detail](docs/instruction/UX-06-settings-dialog-ui-section-broken-and-unclear.md)

## STATE-04 — rule-and-board-size-not-persisted

New `GameSetupConfig` persistence struct; save/restore rule (global preference) + board size
(new-game default). No new UI. Watch the STATE-02 "save() rewrites the whole file" hazard.
[detail](docs/instruction/STATE-04-rule-and-board-size-not-persisted.md)

## REL-01 — changelog-and-release-checklist

Doc/process only. Resolve planning.md Q1 (starting version) with the user first; backfill as
user-impact lines, not `CODE` lists; release checklist goes in the `github` skill, not `CLAUDE.md`.
[detail](docs/instruction/REL-01-changelog-and-release-checklist.md)

## REL-02 — version-string-single-source

One literal in CMake `project(VERSION)` → `configure_file` `version.h`; `--version` handled before
`gtk_init`; test asserts CLI version == CMake version in the gtkmm-free target. Not `kFormatVersion`.
[detail](docs/instruction/REL-02-version-string-single-source.md)

## UI-10 — engine-log-not-sticky-to-bottom-during-analysis

Bug fix — run the `systematic-debugging` pipeline first; the todo file lists three suspect sites
(`isScrolledToBottom` / `scrollToEnd` / `flushPending`) and a stale-layout timing hypothesis, all
to be confirmed with instrumentation before fixing. Keep the "only stick if already at bottom" gate
(RT-02); don't touch the bounded buffer, the gutter (UI-05), or the flush cadence (RT-01/02).
[detail](docs/instruction/UI-10-engine-log-not-sticky-to-bottom-during-analysis.md)

## UI-11 — about-window-rewrite

New `src/ui/about_dialog.{h,cpp}` class (custom layout, not stock `Gtk::AboutDialog`); `onAbout()`
shrinks to construct + existing delete-on-hide lifetime. Version stays `APP_VERSION` (don't regress
REL-02). Build date / git short-hash via `configure_file`, guarded for no-`.git` builds. About-text
only — no app-wide `Rapfi Analysis → RANLS` rename here.
[detail](docs/instruction/UI-11-about-window-rewrite.md)

## UI-13 — wingraph-record-eval-regardless-of-side

Bug + design call — run `systematic-debugging` first (todo file carries a 2026-09-03 trace; start
from its open-questions list). Reproduce the NaN gaps, then pick candidate fix A/B/C with the user.
Don't touch UI-01 attribution, UI-09 SingleSide, or the eval→win% maths.
[detail](docs/instruction/UI-13-wingraph-record-eval-regardless-of-side.md)

## ANLZ-01 — continuous-analyze-mode

Feature (the "Lizzie way"). `features/analyze-mode/planning.md` Q1–Q8 resolved 2026-09-04 (all
defaults accepted). Pure
`MainWindow` orchestration: copy `maybeStartAutoMove()`'s idle-coalescing; reuse
`EngineController::analyze()`/`stopAnalysis()` unchanged; stay orthogonal to "Engine plays" /
ENG-02; leave UI-13 candidate A alone. `stopAnalysis()` before `analyze()` or the restart no-ops.
[detail](docs/instruction/ANLZ-01-continuous-analyze-mode.md)

## ANLZ-04 — wingraph-bridge-nan-gaps

Pure `WinGraphView::onDraw` rendering change — faint dashed connector across NaN runs, no model
or config change. Two-pass draw so `set_dash` never toggles mid-path; bridge style must sit clearly
below both the solid Black line and the UI-09 dashed White line. Keep the no-dot / "(no eval)"
behaviour for gap plies. Consider factoring a pure `computeGapBridges()` helper for testability.
Needs a `docs/audit/` entry for the UI-01 "disjoint segments" refinement.
[detail](docs/instruction/ANLZ-04-wingraph-bridge-nan-gaps.md)

## ANLZ-05 — analyze-mode-no-automove-allow-mid-search-moves

Refinement of ANLZ-01, `MainWindow`-layer only. Three edits: `maybeStartAutoMove()` bails when
`viewConfig().analyzeMode`; `scheduleAnalyzeModeRestart()` drops its `isEnginesTurn` bail (analyse
regardless of side); the `signal_move_clicked` handler calls `controller_.stopAnalysis()` before
`makeMove()` when `isAnalyzing()`. Don't weaken `GameState::makeMove()`'s guard; don't touch
ENG-02 / one-shot Analyze / auto-move-with-analyze-mode-off. Reverses planning Q6 — update the
ANLZ-01 test assertions that pinned the old behaviour.
[detail](docs/instruction/ANLZ-05-analyze-mode-no-automove-allow-mid-search-moves.md)

## ANLZ-06 — analyze-mode-search-plays-stray-move

Regression against shipped ANLZ-05. `src/engine/engine_controller.{h,cpp}` only. `analyze()`'s
`YXNBEST` search ends by emitting a bestmove coordinate line, which `EngineController` relays to
`signal_engine_move` unconditionally. Add a `SearchIntent { None, Analysis, Move }` member set by
`analyze()` / `requestEngineMove()`; in the `protocol_->signal_move` handler emit
`signal_engine_move` only for `Move` intent, else treat the coord as search-completion only; reset
the intent to `None` in `stopAnalysis()` / `stopEngine()` / `signal_process_died`. Don't change the
`YXNBEST` request, the ANLZ-05 `MainWindow` guards, or the ENG-02 / UI-06 / one-shot paths. Keep
the UI-13 flush ordering for the `Move` case. Regression test must feed **inbound** coordinate
lines (the gap in `test_anlz05_no_automove_action.cpp`). `/systematic-debugging` Phase 1–2 already
done — see the todo file.
[detail](docs/instruction/ANLZ-06-analyze-mode-search-plays-stray-move.md)

## ANLZ-07 — analyze-mode-restart-busy-loop

Regression against shipped ANLZ-06, same user transcript. Not yet pulled into a sprint — resolve
the open design question with the user first (skip-restart-if-unchanged vs. minimum restart
interval vs. both; recommended default in the detail file) before implementing. Once resolved:
`MainWindow`/`EngineController` only, no protocol change; regression test asserts only one
`YXBOARD` round-trip for two identical back-to-back completed results on the same position.
`/systematic-debugging` Phase 1–2 already done from the transcript alone — see the todo file.
[detail](docs/instruction/ANLZ-07-analyze-mode-restart-busy-loop.md)

## ANLZ-03 — persist-winrate-in-save-file — ⛔ SUPERSEDED by RDB-01/02/03

Original plan (extend `.yxgame` text schema) rejected by the user 2026-09-04. Replaced by the
binary `.rdb` format — see `features/rdb-save-format/` and the RDB-0x entries below.
[detail](docs/instruction/ANLZ-03-persist-winrate-in-save-file.md)

## RDB-01 — rdb-container-and-codec

Model-layer only, no UI, no `game_io.cpp` change. Build `src/model/rdb/`: `"RDB1"` container
framing + atomic write, `ICompressor` (Raw + DEFLATE over the already-present `zlib`), `GameGraph`
DTO, a **hand-rolled RFC 8949 subset** CBOR codec (string keys, skip unknown keys, total on
truncation), `VariationTree`↔`GameGraph` convert. NaN eval ⇒ no `winrate` written. `toGameGraph`
serialises only today's `TreeNode` fields (RDB-03 extends the struct). Add `ZLIB::ZLIB` to
`ranls-gui` + both test targets.
[detail](docs/instruction/RDB-01-rdb-container-and-codec.md)

## RDB-02 — wire-rdb-into-save-open

Depends on RDB-01. `IGameArchiveReader/Writer` + `RdbArchive` (both) + `YxgameReader` (read-only,
wraps `GameIO::loadGame` → linear all-NaN `GameGraph`) + extension factory. Rewire `onSaveGame` /
`onLoadGame` (`src/main_window.cpp` ~L754 / ~L721). Delete `GameIO::saveGame` + its dead test
cases. Branch replay must operate on `tree()` directly, not `makeMove` down branches. `docs/audit/`
entry for the format change. No `TreeNode` / `evalHistory` gate change — RDB-03.
[detail](docs/instruction/RDB-02-wire-rdb-into-save-open.md)

## RDB-03 — persist-restore-node-analysis

Depends on RDB-01+02. Closes the ANLZ-03 goal. **D1:** resolve the `evalHistory()`
`depth>0||nodes>0` gate — prefer `!std::isnan(node->eval)` **but** `TreeNode::eval` defaults to
`0.0` not NaN (riskiest point — either re-default to NaN and fix assumers, or take the "always
persist depth/nodes" fallback; test fresh game *and* loaded game). **D2:** extend `TreeNode` (prefer
`std::optional<NodeAnalysis>` — evalText/pv/glyph/engineRef/analyzedUtc). Full round-trip +
carried ANLZ-03 regression set + fresh-game-still-all-NaN test. Record D1/D2 in the fix-log.
[detail](docs/instruction/RDB-03-persist-restore-node-analysis.md)

## ENG-03 — orphaned-engine-on-crash-or-wm-close

Builds on ENG-01; must not regress ENG-02. Two small independent changes: **(1)** wire
`signal_close_request` in `MainWindow::connectSignals()` to the same graceful stop as `onQuit()`
(factor a shared `requestGracefulClose()`), veto the first close and re-`close()` from
`stopEngine()`'s callback, `closeInFlight_` flag for re-entrancy. **(2)** `PR_SET_PDEATHSIG(SIGKILL)`
on the engine child via `Gio::SubprocessLauncher` + `g_subprocess_launcher_set_child_setup` (only
async-signal-safe calls in the callback), `#ifdef __linux__` with the current
`Gio::Subprocess::create` as the fallback. Do **not** touch the ENG-01 enum / `stop()` vs
`stopAsync()` split / `~EngineController`; no crash-reporter; no `setsid`.
[detail](docs/instruction/ENG-03-orphaned-engine-on-crash-or-wm-close.md)

---

_Items without an entry here (RT-02/03/04, STATE-02/03, PROTO-02, NAV-01, UI-01/02/03, UX-01…04,
CLEAN-01) are self-contained enough that their `docs/todo/` detail file's "Scope boundary" section
is sufficient guidance. Add an entry here if one turns out to need it._
