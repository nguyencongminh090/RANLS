# 2026-08-21 — Custom-drawn widgets have no keyboard focus indicator or accessible role (accepted limitation)

## Decision

Per `docs/todo/UX-03-accessibility-and-destructive-actions.md`'s acceptance criterion #2, `BoardView`
(`src/ui/board_view.cpp`), `TreeNodeView` (`src/ui/tree_node_view.cpp`), and `WinGraphView`
(`src/ui/win_graph_view.cpp`) are recorded here as an **accepted, not-yet-fixed limitation** rather
than given a focus ring + accessible role in this pass.

## Reason

All three widgets are plain `Gtk::DrawingArea`s wired up with only `Gtk::GestureClick` and
`Gtk::EventControllerMotion` (see `BoardView::BoardView`, `TreeNodeView::TreeNodeView`,
`WinGraphView::WinGraphView`) — none of them calls `set_focusable(true)`, adds a
`Gtk::EventControllerKey`, or implements `Gtk::Accessible`. There is, today, **no keyboard focus
mechanism at all** to draw an indicator for.

UX-03's own scope boundary excludes adding full keyboard board navigation ("this item requires
either the focus indicator for whatever focus exists, or an explicit recorded decision"). Since the
"whatever focus exists" clause resolves to nothing, drawing a focus ring or wiring up
`Gtk::Accessible::Role`/`Property::LABEL` on these three widgets now would mean inventing a new
keyboard-navigable focus model from scratch — which is exactly the out-of-scope feature UX-03
explicitly defers. Doing that as a side effect of an accessibility-labelling pass risks a half-built,
untested keyboard interaction model shipped without the deliberate design work (input mapping,
wrap-around behavior, focus-vs-hover interplay with the existing mouse hover state) that a real
keyboard-navigation feature needs.

## What this leaves undone

- Screen-reader users get no semantic role or label for the board, the move-tree, or the win-rate
  graph — they read as bare, unlabelled drawing surfaces.
- There is no way to interact with any of the three widgets without a pointing device.

## Path forward

Track "full keyboard board/tree/graph navigation + accessible roles" as a follow-on feature (not a
UX-03 sub-fix) — it needs its own `features/<slug>/` design pass per this repo's process model
(user stories for arrow-key/Tab semantics, a state diagram for focus-vs-hover, and open questions
around whether a keyboard focus cursor should coexist with mouse hover) before it becomes a
`docs/todo/` item. Not filed as a `TODO.md` Backlog line in this pass since that decision belongs to
the user/sprint-planning step, not to this fix.
