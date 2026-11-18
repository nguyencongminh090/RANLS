# UX-10 — Design the analysis workspace for a new game and live analysis

## Approach

1. Follow the [task scope](../todo/UX-10-analysis-workspace-design.md) in order. Use the [UI/UX review skill](../../.agents/skills/ui-ux-review/SKILL.md) to review the current main window with the engine off in a new game and in a mid-game position with live analysis, multiple candidate lines, and a branched move tree.
2. Put two annotated main-window mockups and a brief interaction spec under `docs/design/`. Use the same layout in both views. Show where the board, candidate lines, win-rate graph, and search metrics sit. Specify the engine-start and analysis actions and the visible feedback for each engine state.
3. Show the current move in both Visual and Table tree views. Annotate how to spot a branch in Visual view and specify how switching views preserves the selected position.
4. Annotate the narrow-window behavior and the View menu path for restoring the Move Log and Engine Log. Show how the board stays legible as panels are collapsed.

## Pitfalls

- Keep the engine-off analysis area visually quiet, following [UI-08](../todo/UI-08-remove-empty-state-placeholder-text.md). Convey the first available action through the controls.
- Give off, idle, analyzing, and disconnected states a text or icon cue that works without color. Keep starting the engine and requesting analysis distinguishable.
- Use one branched game position consistently across the live mockup and the two move-tree views. The current-move marker and selected position must agree.

## Verification before marking this task done

- Open both mockups and walk through the acceptance criteria in the task detail. Confirm that the new-game mockup makes the first engine action discoverable. Confirm that the live mockup shows several candidate lines with continuations, a graph, search metrics, and a selected move in a branched game.
- Verify against the interaction spec that all four engine states have a cue independent of color, Visual and Table retain the same selected position, and the narrow-window annotation shows a readable board and how to restore both logs.
- Build the app and run both ctest suites. Use `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset for widget tests on a headless machine. Record the visual walkthrough and test results in the task detail.

## Boundaries — do not touch

Deliver design artifacts and tracking updates only. Base the proposal on the existing board styling, GTK typography, engine actions, and View menu panel toggles. Application source, tests, engine protocol, and persistence are outside UX-10's scope.
