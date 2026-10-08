# Current sprint

## Sprint 25

**Goal:** Palette titles follow the UI language, and the About dialog heading bug is fixed

**Dates:** 2026-10-08 to — (open — no fixed end date set yet)

**Dependency graph:**
- **I18N-04** — `src/ui/palette_catalog.cpp`, `src/ui/settings_registry.h`, `vi.tsv`; display titles via `tr()`, search data/lexicon untouched; benchmark MRR must not change; no design call open (planning Q7 resolved).
- **UI-22** — `src/ui/about_dialog.cpp`; **bug: needs `systematic-debugging` first** (the `&`-in-Pango-markup cause is a hypothesis), regression test + fix-log; must not change the string/key or other dialogs.
- Independent of each other; I18N-04 touches the palette and catalog, UI-22 only About.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| I18N-04 | Palette titles from the catalog | I18N-01..03 (done) | — | 🔲 Not started |
| UI-22 | About "Links & protocol" heading blank | — | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–24).

**Lesson carried in from Sprint 24:** Run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset; re-run build, ctest and the benchmark on the agent's branch instead of trusting its report.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `.claude/rules/sprint-cadence.md`.
