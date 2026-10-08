# PAL-02 — Command palette: Ctrl+K overlay + action/command registries

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/ui/command_palette.{h,cpp}`, `src/main_window.cpp` (action registration), `src/command/command_dispatcher.*` (entry provider), `data/style.css`
**Priority:** P3
**Source:** user request 2026-10-08
**Design:** features/command-palette/
**Depends on / relates to:** PAL-01 (engine), PAL-03 (settings entries), UI-21 chrome

## Problem

The search engine needs a keyboard-first front end reachable from anywhere in the main window.

## Scope (in order)

1. Window-level **Ctrl+K** accel opening a centred overlay (focused `SearchEntry` + result list, top 8, group/shortcut/kind, highlights); Esc closes and restores focus.
2. Entry providers: window actions registered with palette metadata at the `buildMenuBar` call sites (en/vi title, keywords, enabled predicate); `!` commands from `CommandDispatcher` specs (re-queried on open so `.ptc` sync is picked up).
3. Execute: action → activate; command → insert into Engine Log entry if `usage` has args, else run; disabled entries demoted/dimmed.
4. MRU (top-50 ids) in `model/config`.
5. `ranls-gui-ui-tests` coverage for open/close/run.

## Scope boundary

- No ranking logic here (PAL-01). No settings entries (PAL-03). Don't touch `executeLine()` routing.

## Reasoning

Explicit metadata per action beats deriving from `Gio::Menu` (labels lack vi/keywords). Centred overlay matches the user's VS-Code-style intent.

## Knowledge

`gtk-ui-design`, `software-architecture` skills; `docs/knowledge/` UX heuristics/a11y note (keyboard, focus).

## Acceptance criteria

- Ctrl+K works from any focused widget; no collision with existing accels.
- Selecting each kind of result performs the right action; UI tests pass headless-skipped/with display.
