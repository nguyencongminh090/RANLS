# NAME-01 — Consistent app-wide rename "Rapfi Analysis" → RANLS

**Status:** 🔲 BACKLOG
**Area:** app identity / branding
**Priority:** P3
**Source:** split out of UI-11 (about-window-rewrite) per user decision 2026-08-31

## Problem

The app's name is **RANLS**, but `"Rapfi Analysis"` is still hard-coded in
several places. UI-11 corrected only the About dialog's own displayed text
(explicitly scoped that way at dispatch); the rest is deferred here.

## Scope

- `src/main_window.cpp:114` — `set_title("Rapfi Analysis")`.
- `src/resources/style.css` — header comment `Rapfi Analysis GUI — Adaptive Theme`.
- GTK application id (`src/application.cpp` / `src/main.cpp` — currently
  `org.rapfi.gui` or similar) — changing it affects settings/storage keys and
  the WM identity, so check `SettingsStorage` paths before changing.
- CMake `project(rapfi-gui …)` / `DESCRIPTION` — decide whether the build
  target is renamed too (ripples into `tests/`, packaging scripts, install
  rules).
- A future `.desktop` file, if/when packaging adds one.

## Scope boundary

- Not the About dialog (done in UI-11).
- The engine binary names (`pbrain-rapfi`, etc.) are unrelated — do not touch.

## Acceptance criteria

- No user-visible `"Rapfi Analysis"` string remains.
- Settings/storage continue to load (app-id change handled or avoided).
- Full suite green.

## Related

- UI-11 — renamed the About dialog text only.
