# Current sprint

## Sprint 6

**Goal:** Fix the analysis-panel and settings UI defects surfaced by a 2026-08-30 UI review: the PV
list accumulating stale lines across positions, the Engine Log mixing direction tags into copyable
text, a Settings "UI Setting" section whose controls (coordinates, theme, win-graph mode, UI
profile) don't work or aren't clearly defined, and a redundant "Analysis" menu.
**Dates:** 2026-08-30 to — (open — no fixed end date set yet)

**Dependency graph:** UI-04/UI-05 are independent. UI-06's design questions were resolved with the
user 2026-08-30 (rename "Analysis" menu → "Engine plays" Black/White/Off, auto-move semantics, new
`MatchConfig`); it is no longer blocked. **UX-06 now depends on UI-06** — the WinGraph "SingleSide
Auto" perspective reads `MatchConfig::enginePlays`, so UI-06 is dispatched first and UX-06 after it.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| UI-04 | PV view appends lines across positions instead of replacing; multiple `PV #1` rows at MultiPV=1 | — | — | ✅ Fixed 2026-08-30 |
| UI-05 | Engine Log: move the direction tag into a fixed-width non-copyable gutter column | — | — | ✅ Fixed 2026-08-30 — payload-only `Gtk::TextView` + sibling fixed-width `engine-gutter` `DrawingArea` painting each tag at its line's live y; row copies now yield raw engine text |
| UI-06 | "Analysis" menu → "Engine plays" (Black/White/Off) auto-move selector; new `MatchConfig` | — | — | ✅ Fixed 2026-08-30 — implemented via `/implement-task` (new `MatchConfig` + "Engine plays" radio menu + `generateMoveRequest`/`requestEngineMove` auto-move path); closed by the user after a live-engine smoke pass, which also surfaced the separate STATE-04 persistence gap |
| UX-06 | Settings "UI Setting": coordinates + Light/Dark dead, WinGraph Mode unclear/misrendering, UI Profile undefined; organise the dialog | UI-06 | — | Active — implementation complete, 123/123 tests pass; theme + coordinates + tabbed dialog visually verified (screenshots); WinGraph mode-with-data check still needs a human (2026-08-30) |
| UI-07 | PV panel still accumulates a stale row per analysed position (real `MESSAGE depth …` format; UI-04's fix missed this) | — | — | ✅ Fixed 2026-08-30 (second pass) — real cause was `PVView::update()` passing the row's inner `Gtk::Box` to `Gtk::ListBox::remove()`, which GTK 4 ignores ("Tried to remove non-child"), leaking one orphan row widget per clear. Now wraps/removes an explicit `Gtk::ListBoxRow`. Reproduced *and* verified against the real `pbrain-rapfi` driving the real `AnalysisPanel` widget tree; new `rapfi-gui-ui-tests` binary (4 cases) asserts rendered rows |
| STATE-04 | Rule + board size never persisted — reset to Freestyle / size 15 on every launch | — | — | ✅ Fixed 2026-08-30 — new `GameSetupConfig` (rule + boardSize) threaded through `SettingsBundle` + a 4th `save()` param; `load()`/`save()` round-trip `rule` + `board_size` with validate-or-fallback; `MainWindow` restores both at startup, persists via `persistGameSetup()` from `onSetRule()` + Board Size Apply, `onNewGame()` keeps current size; all 3 `save()` call sites thread the full four-block state. +4 round-trip tests; build clean, 129/129 + UI tests pass. Live app launch consumes a hand-written settings file; menu click-through not scriptable in this env |

STATE-04 pulled into Active 2026-08-30 — a pre-existing persistence gap (not a UI-06 regression)
surfaced by UI-06's smoke pass; `engine_plays` itself round-trips fine.

Points not yet estimated. UI-06 + UX-06 code-complete (awaiting human smoke); UI-07 dispatched
2026-08-30 after the user's UX-06/UI-06 smoke test surfaced the still-broken PV accumulation, and
re-dispatched the same day after the first fix proved insufficient — see the lesson below.

**Lesson from UI-07 (carry into the next sprint):** the first UI-07 pass patched the layer the
symptom pointed at (an `AnalysisPanel` signal handler) and shipped with 125/125 green tests, because
every test in the suite asserted the *model* (`GameState::pvLines()`) while the defect lived in a
*widget's* own bookkeeping. `rapfi-gui-tests` links no gtkmm by construction, so it structurally
could not see it. The new `rapfi-gui-ui-tests` target closes that blind spot — reach for it whenever
a reported defect is about what the user can see on screen.

**Lesson carried over from Sprint 5** (which itself carried it from Sprint 4): update
`docs/sprint/burndown.md` as soon as an Active item's status changes, and close a sprint as soon as
its last item lands ✅.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
