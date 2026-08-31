# Sprint 7 (closed 2026-08-31)

**Goal:** UI polish + release prep. Close out the post-UI-review follow-ups (drop the empty-state
placeholder text, stop engine auto-play from silently reverting to Off, make the win-rate graph
readable and its SingleSide mode always-Black) and stand up user-facing versioning (a
"Keep a Changelog" `CHANGELOG.md` backfilled through Sprint 6, a release checklist, and a
single-sourced version string) so `v0.1.0` could be tagged.
**Dates:** 2026-08-31 (opened and closed same day).

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| UI-08 | Remove empty-state placeholder text; keep panels clean/empty (partial reversal of UX-01) | ✅ DONE |
| ENG-02 | Interrupting engine auto-play reverted "Engine plays" to Off instead of staying on the assigned side | ✅ FIXED |
| UI-09 | WinGraph SingleSide always Black (drop UX-06's follow-engine-side coupling); thicker, higher-contrast win-rate line (WCAG pass) | ✅ DONE |
| REL-01 | Root `CHANGELOG.md` ("Keep a Changelog", SemVer 0.x), backfill Sprints 1–6, "cut a release" checklist, tag `v0.1.0`; doc/process only | ✅ DONE |
| REL-02 | Single-source the version string (`configure_file` → `version.h`), wire into About dialog + a pre-GTK `--version` flag | ✅ DONE |

Points were never estimated this sprint (same as Sprints 3–6).

## What shipped

- **UI-08:** removed the empty-state placeholder text added by UX-01 ("No moves yet", "No analysis
  yet", …); panels now render clean/empty.
- **ENG-02:** manual intervention (toolbar/hotkey Stop, analysis-panel Stop, Analyze on the
  engine's own turn, dispatcher `analyze`/`!play` on the engine's turn) now reverts
  `MatchConfig::enginePlays` to `Off` **in memory only** — no `SettingsStorage::save`, so the
  persisted side is restored next launch. Shared pure predicate `isEnginesTurn()` extracted to
  `model/config.h` (+`test_eng02_revert_predicate.cpp`, 3 cases).
- **UI-09:** `WinGraphMode::SingleSide` is now unconditionally Black's perspective (UX-06's
  `enginePlays` coupling removed). Win-rate line thickened (1.5→2.8 px; White 1.2→2.6) and
  recoloured blue `#1A73E8` + green `#1E8E3E` — clears WCAG 3:1 non-text contrast on both the light
  `#fafafb` and dark `#242424` Adwaita panel backgrounds; White line keeps its dash for CVD shape
  redundancy. Regression case rewritten.
- **REL-01:** doc/process only. Root `CHANGELOG.md` created ("Keep a Changelog" 1.1.0, SemVer 0.x):
  `[Unreleased]` + a single `## [0.1.0] - 2026-08-31` backfilled from `docs/sprint/archive/` +
  `docs/fix-log.md` as user-impact lines. "Cutting a release" checklist added to the `github` skill;
  `docs/audit/2026-08-31-changelog-and-release-process.md` records the decision. Tag `v0.1.0`
  created on `main` and pushed.
- **REL-02:** single source of truth is now `project(rapfi-gui VERSION 0.1.0)` in the top-level
  `CMakeLists.txt`; `configure_file` generates `build/generated/version.h` (`#define APP_VERSION`)
  from `src/version.h.in`. `src/main.cpp` scans argv for `--version`/`-v` at the very top of
  `main()` (before any GTK init); `onAbout()`'s `set_version("2.0")` → `set_version(APP_VERSION)`.
  Script-based ctest `rel02-version-single-source` runs the real binary headless and asserts output
  == CMake `PROJECT_VERSION`.

## Lessons

- Carried from Sprint 6: whenever a reported defect is about what the user sees on screen, reach for
  the `rapfi-gui-ui-tests` target (links gtkmm, asserts the rendered widget tree) — applied on
  UI-08 and UI-09.
- Carried from Sprints 4–6: update `docs/sprint/burndown.md` as soon as an Active item's status
  changes, and close the sprint as soon as its last item lands ✅ (Sprint 7 opened and closed the
  same day).

## Rolled over to Backlog

Nothing rolled over — all five committed items finished.

## Next sprint

Sprint 8 (goal "Engine-log sticky-bottom + About-window rewrite") pulls UI-10 and UI-11 from
Backlog into Active — see `docs/sprint/current.md`.
