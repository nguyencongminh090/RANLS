# Current sprint

## Sprint 24

**Goal:** Palette scopes (`!` `>` `@`) and a switchable UI language (English / Tiếng Việt)

**Dates:** 2026-10-08 to — (open — no fixed end date set yet)

**Dependency graph:**
- **PAL-04** — `src/command/palette_search.*`, `src/ui/command_palette.*`, dataset; must not touch `CommandDispatcher::executeLine()`; no debugging needed; decisions made (keep `!` `>` `@`, empty default miss + `!` hint).
- **I18N-01** — new GTK-free `src/i18n/`, resources, tests; must not include gtk/engine; first of the chain.
- **I18N-02** — depends on I18N-01; touches `src/ui/*`, `main_window.cpp`; text-only; user reviews drafted `vi.tsv`.
- **I18N-03** — depends on I18N-01 (pairs with I18N-02); settings registry + `SettingsStorage` + startup.
- I18N-04 (palette titles from catalog) stays in Backlog.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| PAL-04 | Palette scope prefixes, UI-only default | — | — | 🔲 Not started |
| I18N-01 | Catalog loader `i18n::tr` + lint | — | — | 🔲 Not started |
| I18N-02 | Wire UI strings + vi catalog + live refresh | I18N-01 | — | 🔲 Not started |
| I18N-03 | Language setting + persistence + system default | I18N-01 | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–23).

**Lesson carried in from Sprint 23:** Run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset; keep drift guards between registries and the benchmark dataset (new `set.language` needs dataset rows).

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `.claude/rules/sprint-cadence.md`.
