# 2026-08-31 — UI-08: Remove empty-state placeholder text; keep panels visually clean

**Task:** `docs/todo/UI-08-remove-empty-state-placeholder-text.md` (Sprint 7, P3). No
`docs/instruction/UI-08-*.md` entry exists — acceptable per project rules.

Partial, deliberate reversal of UX-01 (`docs/fix-log/2026-08-21-ux-01-empty-states.md`): UX-01 added
a specific instructional placeholder ("No analysis yet — press Analyze (F5)", "No moves yet — …",
etc.) to every data-driven panel's idle/no-data state. The user now wants an empty panel to just be
empty (clean), with no instructional text.

## Prompt

Implement tracked task UI-08. Remove every "No … yet" / "No data" / "press Analyze" style
placeholder string rendered in the idle/no-data state from the PV view, move log, engine log, both
tree views, the analysis panel, and the WinGraphView empty state. Resolved open question (confirmed
with the user): the WinGraphView 0/50/100% axis scaffold STAYS — it is structural, not
instructional; remove only the placeholder text. Text/visibility removal only — do not restyle
backgrounds/borders/layout, do not touch the STATE-01 clear/notify path, do not implement UI-09.

## Action

- **`src/ui/win_graph_view.cpp`** — the `if (blackData_.empty())` branch in `onDraw()` no longer
  calls `EmptyState::drawPlaceholder(..., "No analysis yet — press Analyze (F5)")`; it now just
  `return`s after the axis scaffold (50% line + 0/50/100% labels), which is drawn unconditionally
  as before and is untouched. Dropped the now-unused `#include "empty_state.h"`.
- **`src/ui/tree_node_view.cpp`** — the `if (nodes_.empty())` branch in `onDraw()` no longer draws
  "No moves yet — play or load a game to see the move tree"; it just `return`s (clean empty region).
  The `kMinEmptyWidth`/`kMinEmptyHeight` (220×90) size floor set in `update()` is structural and
  stays. Dropped the now-unused `#include "empty_state.h"`; added `(void)width; (void)height;` since
  `onDraw` no longer uses them in the empty path.
- **`src/ui/empty_state.h` / `.cpp`** — `EmptyState::drawPlaceholder()` (the Cairo text helper) is
  removed entirely (no more callers). `EmptyStateOverlay` is kept as a thin passthrough wrapper
  around its single content child (`setContent()` still calls `set_child()`), but no longer creates,
  adds, or measures any placeholder `Gtk::Label`; `setEmpty()` is now a no-op. Keeping the class and
  its call sites intact means the reversal touches no panel layout — an `Gtk::Overlay` with one
  child measures exactly as that child.
- **`src/ui/pv_view.h`, `src/ui/tree_explorer.h`, `src/ui/bottom_panel.h`** — the `EmptyStateOverlay`
  members are now constructed with an empty message string instead of the UX-01 placeholder copy;
  comments updated. Call sites (`overlay_.setEmpty(...)`, `updateMoveLogEmptyState()` /
  `updateEngineLogEmptyState()` and their invocations) are left in place and are now inert.
- **`src/resources/style.css`** — removed the dead `.empty-state-message { opacity: 0.65;
  font-style: italic; }` rule (no widget carries that class any more).
- **Analysis panel** (`src/ui/analysis_panel.*`) — inspected; it renders no placeholder string of
  its own (it is a container hosting `WinGraphView` + stat labels), so the WinGraphView change
  covers it. No edit needed.
- **STATE-01 clear/notify path** — not touched. The empty state is still reached exactly as before
  (widgets react to their own already-cleared data inside the same `update()`/`setData()`/append
  methods STATE-01 drives); only what those methods *render* in the empty case changed.

### Regression test

`tests/test_ui08_no_empty_state_text.cpp` (new), added to the `rapfi-gui-ui-tests` gtkmm-linking
target. Two cases, walking the real widget tree for visible `Gtk::Label` text:

- `EmptyStateOverlay` constructed with a placeholder message and `setEmpty(true)` exposes no visible
  label containing "yet" / "Analyze" / "No analysis" — only its content child.
- An empty `PVView` (`pv.update({}, 15)`) exposes no visible label containing "yet" /
  "press Analyze" / "principal variations".

Both self-skip when no display server is available (same pattern as the UI-07 UI tests, which own
`main()`).

The Cairo-drawn text removal in `WinGraphView` / `TreeNodeView` is not reachable through the widget
tree (custom `snapshot`/`draw_func` output, not child widgets) and is not unit-testable with this
harness — verified by code review that the two `EmptyState::drawPlaceholder` call sites are deleted
and the helper itself is gone, and by the clean build (any remaining reference would not compile).

## Verification

- **Clean build:** `rm -rf build_cmd && ./build.sh` (Ninja, Release, gtkmm-4.0) — `rapfi-gui`
  links successfully, zero new warnings (only the three pre-existing `-Wunused-function` warnings in
  `gomocup_protocol.cpp`, already noted in the UX-01 and later fix-log entries).
- **Full test suite:** `RUN_TESTS=1 ./build.sh` → `ctest`: `2/2` passed (`rapfi-gui-tests`,
  `rapfi-gui-ui-tests`). Running `rapfi-gui-ui-tests` directly with a display present: 6 cases / 51
  assertions, all passing — confirming the 2 new UI-08 cases actually executed rather than
  headless-skipping.
- **Lint:** `node scripts/check-task-structure.js` → "OK: TODO.md / instruction.md structure is
  consistent."; `node scripts/check-tracking-sync.js --full` → exit 0, no drift.
- **Manual/visual check performed:** `grep -rE` over `src/` confirms no "No … yet" / "press
  Analyze" / "play or load a game" / "moves will appear" / "start the engine to see" string, and no
  `drawPlaceholder` / `empty-state-message` reference, survives outside of explanatory comments.
- **Manual/visual check still owed (not blocking):** launching the app to eyeball that the PV list,
  move log, engine log, both tree tabs, and the win-graph render as clean empty regions on fresh
  launch, that the win-graph still shows its 0/50/100% axis, that real data still renders when
  analysis runs, and that panels return to clean-empty after New Game. Not done in this pass (no
  screenshot infra wired here beyond the headful ctest run); the widget-tree assertions + build
  cover the code paths.

## Summary

Idle/no-data panels now render with no instructional placeholder text. The `WinGraphView` axis
scaffold is retained (structural). `EmptyStateOverlay` is reduced to an inert passthrough and the
`EmptyState::drawPlaceholder` helper + its CSS class are removed. STATE-01's clear/notify path is
untouched; UI-09 (same widget, line-weight/contrast work) is explicitly out of scope. New
`test_ui08_no_empty_state_text.cpp` guards against the placeholder label being re-introduced.

**Left out of scope, as instructed:** no panel background/border/layout restyle; no STATE-01
change; no WinGraphView axis-scaffold removal; no UI-09 work (win-rate line thickness/contrast).
