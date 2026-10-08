# I18N-04 — UI language: palette titles from the catalog

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/ui/palette_catalog.cpp`, `src/ui/settings_registry.h`, `src/command/palette_search` display path
**Priority:** P3
**Source:** `features/ui-language/planning.md` Q7 (follow-up task)
**Design:** features/ui-language/
**Depends on / relates to:** I18N-01..03

## Problem

Palette entries carry duplicated hard-coded Vietnamese titles; the displayed title should follow the UI language while search keeps matching both languages (story L7).

## Scope (in order)

1. Display title via `tr()`; keep both-language search fields (lexicon) unchanged.
2. Remove duplicated vi strings from code, move into `vi.tsv`; benchmark rerun.

## Scope boundary

- No change to ranking or the lexicon; no new UI-language features.

## Reasoning

Single source of truth for Vietnamese strings; search accuracy must not depend on UI language.

## Knowledge

`features/ui-language/planning.md`; `docs/audit/2026-10-log.md` (PAL-01 benchmark).

## Acceptance criteria

- Title shown follows UI language; queries in either language return the same ranking as before (benchmark MRR unchanged).
- No Vietnamese literals left in `palette_catalog.cpp`/`settings_registry.h`.
