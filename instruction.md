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

---

_Items without an entry here (RT-02/03/04, STATE-02/03, PROTO-02, NAV-01, UI-01/02/03, UX-01…04,
CLEAN-01) are self-contained enough that their `docs/todo/` detail file's "Scope boundary" section
is sufficient guidance. Add an entry here if one turns out to need it._
