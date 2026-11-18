# UX-10 — Design the analysis workspace for a new game and live analysis

**Status:** 🔲 OPEN (Active — Sprint 28)
**Area:** Main window layout, engine controls, analysis panel, move tree, and logs
**Priority:** P3
**Source:** User's main-window screenshot and UI design discussion, 2026-11-18
**Design:** Two annotated main-window mockups and a brief interaction spec
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
