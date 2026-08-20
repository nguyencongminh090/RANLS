# UX-01 — Three panels render as blank rectangles instead of empty states

**Status:** open
**Area:** analysis panel / win graph / PV view / tree views / bottom panel
**Priority:** P2
**Source:** UI/UX review of the idle-state screenshot + codebase review, 2026-08-21

## Problem

On a fresh launch — before any analysis has run — the app shows three independent blank dark
rectangles. Each widget returns early or renders nothing when it has no data, with no placeholder
text:

| Widget | Site | Behaviour with no data |
|---|---|---|
| `WinGraphView` | `src/ui/win_graph_view.cpp:73` | `if (blackData_.empty() ...) return;` — draws nothing at all, not even axes |
| `PVView` | `src/ui/pv_view.cpp:48` | loop over an empty vector — empty `ListBox` |
| `TreeNodeView` | `src/ui/tree_node_view.cpp:155` | `if (nodes_.empty()) return;` |
| `TreeExplorer` | `src/ui/tree_explorer.cpp:72` | empty `ListStore` |
| Move Log / Engine Log | `src/ui/bottom_panel.cpp` | empty `TextView`s |

The `ui-ux-review` checklist calls this out directly ("Empty states need a message, not a blank
panel"). The practical effect, visible in the launch screenshot, is that the app reads as
half-loaded or broken rather than idle-and-ready.

This is systemic — five widgets share one missing pattern — so it should be fixed once, consistently,
rather than per widget.

## Acceptance criteria

- Each data-driven panel shows a brief, specific placeholder when empty — what it will show and what
  the user should do to populate it (e.g. "No analysis yet — press Analyze (F5)"), not a generic
  "No data".
- `WinGraphView` draws its axis scaffold (0/50/100% guides) even with no series, so it reads as an
  empty chart rather than a void.
- Placeholder text meets the 4.5:1 contrast minimum in both light and dark themes.
- Empty states disappear correctly as soon as real data arrives, and return after New Game (which
  depends on STATE-01 actually clearing and notifying).

## Scope boundary

- Do not redesign panel layout or the light-board/dark-panel colour split; this is about the empty
  case only.
- Clearing stale data so the empty state is reachable is STATE-01, a prerequisite.

## Related

- STATE-01 (data must actually clear for the empty state to appear), UX-03 (contrast/accessibility)
