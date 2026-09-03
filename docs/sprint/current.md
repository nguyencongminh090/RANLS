# Current sprint

## Sprint 9

**Goal:** WinGraph coverage + app-wide RANLS rename. Make the win-rate graph record the engine's
returned win% for every analysed position regardless of side to move (no NaN gaps on the opponent's
plies during "Engine plays <side>"), and finish the `"Rapfi Analysis" → RANLS` rename everywhere it
still leaks (window title, CSS, GTK application id) that UI-11 left out of scope.
**Dates:** 2026-09-03 to — (open — no fixed end date set yet)

**Dependency graph:** UI-13 and NAME-01 are independent — different layers, no shared files.
- **UI-13** is a bug + product-design call: run `systematic-debugging` first. The
  `docs/todo/UI-13-*.md` detail file already carries a 2026-09-03 trace — the recording gate is in
  `GameState::setAnalysisData()` writing an eval only onto the `currentPath()` node, so positions
  that were never the current position during a search stay NaN (`evalHistory()` → visible gap).
  Candidate fixes A/B/C are listed there; **pick one with the user** before implementing. Must not
  touch UI-01 attribution, UI-09's SingleSide decision, or the eval→win% maths. Regression test is
  model-layer / gtkmm-free (extend `tests/test_ui01_winrate_attribution.cpp` or add a sibling).
- **NAME-01** is branding cleanup: `src/main_window.cpp:114` `set_title`, `src/resources/style.css`
  header comment, the GTK application id (check `SettingsStorage` paths first — an app-id change
  moves settings/storage keys), and CMake `project()`/`DESCRIPTION` (decide whether the build
  target renames too — ripples into `tests/`). Not the About dialog (done in UI-11). Not the engine
  binary names.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| UI-13 | WinGraph: record the returned win% for every analysed position, regardless of side to move — opponent plies stay NaN under "Engine plays <side>" | — | — | 🔲 Not started |
| NAME-01 | Consistent app-wide rename `"Rapfi Analysis" → RANLS` (window title, `style.css`, GTK app id, CMake) — split out of UI-11 | — | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–8).

**Lesson carried in from Sprint 8:** every "sticky-bottom" / scroll-follow path must scroll a
`Gtk::Scrollable` (`TextView`, `ListView`) *directly* — a non-scrollable wrapper as the
`ScrolledWindow` child makes GTK interpose a viewport and silently no-ops every `scroll_to`. Not
directly relevant to UI-13/NAME-01 but audit any new scroll code against it.

**Lesson carried in from Sprints 6–8:** whenever a reported defect is about what the user sees on
screen, reach for the `rapfi-gui-ui-tests` target (links gtkmm, asserts the rendered widget tree) —
`rapfi-gui-tests` links no gtkmm and structurally cannot see widget-level bugs. UI-13's own
regression test is model-layer, but any NAME-01 title/identity assertion belongs in the gtkmm
target.

**Lesson carried from Sprints 4–8:** update `docs/sprint/burndown.md` as soon as an Active item's
status changes, and close the sprint as soon as its last item lands ✅.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
