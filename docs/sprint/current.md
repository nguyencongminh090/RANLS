# Current sprint

## Sprint 23

**Goal:** Command palette (Ctrl+K): bilingual classical-NLP search over actions, settings and ! commands, benchmarked against a Whoosh baseline

**Dates:** 2026-10-08 to — (open — no fixed end date set yet)

**Dependency graph:**
- **PAL-01** — `src/command/palette_search.*`, `palette_lexicon.*`, `tests/data/palette/`, `scripts/palette_bench.py`. Pure (no GTK, no `model/`); dataset (Vietnamese labels reviewed by the user) is written before the engine; Whoosh is dev-only. Not a bug fix. Read `docs/instruction/PAL-01-*.md` first. Design resolved (`features/command-palette/planning.md`, Q1–Q10).
- **PAL-02** — `src/ui/command_palette.*`, `src/main_window.cpp` action metadata, dispatcher spec provider. Depends on PAL-01; one key controller per widget; verify Ctrl+K is free first.
- **PAL-03** — `src/ui/settings_dialog.*` + `SettingEntry` table. Behaviour-preserving refactor; independent of PAL-01, feeds PAL-02.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| PAL-01 | Pure en/vi search engine (BM25F) + shared dataset + Whoosh benchmark | — | — | 🔲 Not started |
| PAL-02 | Ctrl+K overlay + action/`!`-command registries | PAL-01 | — | 🔲 Not started |
| PAL-03 | Declarative settings registry so settings are searchable | — | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–…).

**Lesson carried in from Sprint 22:** Visual checks are still human-owed (offscreen render / Xvfb only), so verify the palette overlay with a screenshot in a live window; and file a CODE before any work ships outside the committed list so the Active table stays honest.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `.claude/rules/sprint-cadence.md`.
