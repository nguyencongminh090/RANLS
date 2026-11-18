# UX-10 — Design the analysis workspace for a new game and live analysis

**Status:** ✅ DONE (Active — Sprint 28) — five annotated mockups and an interaction spec delivered under `docs/design/` (see Result). Verified by a live baseline review, a mockup walkthrough and ctest 5/5. Fix-log: [detail](../fix-log/2026-11-18-ux-10-analysis-workspace-design.md)
**Area:** Main window layout, engine controls, analysis panel, move tree, and logs
**Priority:** P3
**Source:** User's main-window screenshot and UI design discussion, 2026-11-18
**Design:** [Interaction spec](../design/UX-10-analysis-workspace-spec.md) and five annotated mockups in `docs/design/`
**Depends on / relates to:** UI-08, UI-23, collapsible panels shipped in v0.12.0 (PR #66)

## Problem

The new-game screenshot shows a legible board beside a large, mostly empty analysis area and above
a full-width log pane. The compact engine controls are icon-only. The design pass should make it
easy to start analysis, read candidate moves once results arrive, and keep the board and current
variation in view while navigating.

## Scope (in order)

1. Review the current window in two states: a new game with the engine off and a mid-game position
   with live analysis, several candidate lines, and a branched move tree.
2. Mock up both states using the same layout. Show where the board, candidate moves and their
   continuations, win-rate graph, and search metrics sit. Specify how the controls distinguish
   starting the engine from requesting analysis and how the UI communicates engine state.
3. Show how the visual and table views identify the current move and how users find branches in
   the visual view. Specify what happens to the selection when switching between the two views.
4. Evaluate how users discover the existing View menu panel toggles, recover space for the board on
   narrower windows, and restore the Move Log or Engine Log when needed.

## Scope boundary

This is a design task for the main window. The deliverables are mockups and an interaction spec.
Use the existing board styling, GTK typography, engine actions, and panel toggles as the baseline.

## Reasoning

The new-game view tests whether the first action is discoverable. The live view tests whether users
can scan candidate lines and return to the current move. UI-08 recorded a preference for clean empty
panes. Explore control labels and panel sizing to make the workspace clearer while honoring that
preference.

## Knowledge

- [UI-08 — clean empty panes](UI-08-remove-empty-state-placeholder-text.md)
- [UI-23 — typography and layout polish](UI-23-typography-and-layout-polish.md)
- [v0.12.0 release notes — existing View menu toggles](../../CHANGELOG.md#0120---2026-11-12)
- [YixinBoard UI/UX review skill](../../.agents/skills/ui-ux-review/SKILL.md)
- [Usability walkthrough guidance](../knowledge/ux-research-usability.md)

## Acceptance criteria

- Two annotated mockups show the new-game and live-analysis states. The live mockup includes multiple
  candidate lines, a win-rate graph, engine metrics, and a selected move in a branched game.
- The interaction spec identifies the first available action when the engine is off and how the UI
  distinguishes off, idle, analyzing, and disconnected states without relying on color alone.
- The spec shows where the current move appears in each tree view and how switching views preserves
  the selected position.
- A narrow-window annotation shows how the board stays legible and how users reveal the logs again.

## Result (2026-11-18)

Deliverables, all under [docs/design/](../design/):

- [Interaction spec](../design/UX-10-analysis-workspace-spec.md) with the baseline findings B1 to B12, the shared layout, the engine actions and states, the tree-view and selection rules, and the panel proposals.
- Mockup 1 [new game, engine off](../design/new-game-engine-off.svg) and mockup 2 [live analysis, branched game](../design/live-analysis-branched.svg). Mockup 3 [Visual and Table, same selection](../design/visual-and-table-views.svg). Mockup 4 [engine states](../design/engine-states.svg) with a grayscale check. Mockup 5 [narrow window and panel restore](../design/narrow-window-and-panels.svg). Each has a PNG render beside it.
- [baseline/](../design/baseline/): 14 screenshots of the real built app that the review rests on.

Headline proposals: keep the blue **Analyze** as the first action and add a labelled secondary **Start engine**, rename Stop to **Stop analysis**, give each engine state a word plus a shape (Off, Idle, Analyzing, Disconnected), mark the selected node and fork points in Visual, mark the current row and forks in Table, and replace the full-width log pane with a log bar so the board keeps its space.

Earlier decisions touched: UI-08 is kept (no placeholder text, graph axes stay, only the dash metrics row is proposed to hide), UI-21 is partly reversed for the engine-strip buttons (text labels) and the session-only panel visibility decision is kept. The spec lists them in a table.

Findings worth a follow-up, none filed: a failed engine start stays at OFF with the error only in the Engine Log tab (B5), stale candidate rows stay after a crash (B6), and the panel toggles have no shortcut and are not in the command palette (B11).

## Verification record

**Data source.** The Release build of this worktree (v0.12.0, main `375532a`) ran under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset, from a copied binary with its own `rapfi-gui.settings` in `/tmp/ux10-run/app/` (`language=en`) so no user settings were touched. No Rapfi binary was available, so the engine was a **scripted fixture** at `/tmp/ux10-run/fixture_engine.py` (outside the repo). It speaks the Gomocup/Yixin subset the app uses (`START`, `YXBOARD`, `YXNBEST`, `STOP`) and streams `INFO` and `MESSAGE (n)` PV lines from a proximity heuristic, so the live numbers are illustrative. The game was entered by clicking the board with `xdotool`, screenshots by PIL `ImageGrab`.

**Live baseline walkthrough** (states seen in the real app, screenshots in `docs/design/baseline/`): new game with the engine off (01), analysis from Analyze with 4 candidate lines and a branched tree (02), after Stop (03), Table and Visual tabs (04, 05, 13, 14), engine killed mid-search (06), bad engine path (07, 08), View menu (09), narrow window with default panels, logs hidden, and both hidden (10, 11, 12).

**Reading and decisions checked.** [ux-research-usability](../knowledge/ux-research-usability.md) (heuristic walkthrough, not user evidence), UI-08, UI-21, UI-23, the 0.12.0 changelog entry and AGENTS.md were read. `python3.13 scripts/audit-search.py` was queried for the icon-first header, collapsible panels and empty-state decisions. The win-graph draw code was read to mark which graph cues exist today.

**Mockup walkthrough against the acceptance criteria**, done by opening each rendered PNG:

| Criterion | Result |
|---|---|
| Two annotated mockups, live one has several candidate lines, graph, metrics and a selected move in a branched game | Pass. Mockup 2 shows 4 lines with 5 to 7 move continuations, a two-line graph with the move 5 marker, six metrics, and 5. H6 selected on a branch of 4. G9. |
| New-game mockup makes the first engine action discoverable | Pass. Analyze is the only blue control, Start engine is a distinct labelled secondary, Stop analysis is dim. |
| Off, idle, analyzing and disconnected distinguishable without color | Pass. Mockup 4 and the spec table give a word, a shape and distinct enabled controls. The grayscale row keeps all four apart. |
| Current move in each tree view and selection preserved across views | Pass. Mockup 3 uses one position. The double-ringed node in Visual and the last, marked row in Table are both 5. H6, and the spec states the selection rules. |
| Narrow-window annotation shows a readable board and how to restore both logs | Pass. Mockup 5 gives 382 px and 26 px cells in the proposal against 290 px and 19 px today, with 1:1 crops. The bar restores each log in one click and the View path is annotated. |
| Board position, branch, selected move and candidate moves coherent across figures | Pass. Same stones in mockups 2, 3 and 5. Candidates H7, I8, G8, J8 and their continuations land on empty points only. |

**Tests.** `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` then `cmake --build build -j4`, then `env -u WAYLAND_DISPLAY GDK_BACKEND=x11 xvfb-run -a ctest --test-dir build --output-on-failure`: ctest 5/5 passed (`ranls-gui-tests` 310 cases and 5111 assertions, `ranls-gui-ui-tests` 142 cases and 968 assertions, 0 skipped). No source, tests, protocol or persistence changed, so the suites are a build-health check only.

**Not covered.** Light theme, board sizes other than 15, MultiPV above 4, Renju marks, languages other than English, screen readers. The estimated 395 px board with the log bar kept (mockup 5, panel D) is an estimate, the other sizes were measured.
