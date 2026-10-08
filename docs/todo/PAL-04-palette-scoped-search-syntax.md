# PAL-04 — Command palette: separate UI items from console/protocol commands + scope prefix syntax

**Status:** 🔲 OPEN (Active — Sprint 24)
**Area:** `src/command/palette_search.{h,cpp}` (scope filter replaces the hard-coded `!` check), `src/ui/command_palette.{h,cpp}` (scope hint), `src/ui/palette_catalog.*` (kinds), tests + dataset (`queries.tsv` scope rows)
**Priority:** P3
**Source:** user request 2026-10-08 (after Sprint 23): "separate UI commands from protocol/... commands; you can add syntax."
**Design:** features/command-palette/ (addendum below); small, no new feature folder
**Depends on / relates to:** PAL-01, PAL-02, PAL-03

## Problem

Today actions, settings and `!` console/protocol commands are ranked together. A plain query such as "cai dat" returns `!info`/`!db` among the UI results (seen in the Sprint 23 live check), and console commands (`!send`, `!engine`, `.ptc` extensions) are a different mental category from "things the app can do". Users also have no way to restrict a search to one category except the existing `!` prefix.

## Scope (in order)

_Decided 2026-10-08 (user delegated): prefixes `!` `>` `@` as listed; a default query with no UI match stays empty and the empty state hints at `!` (no fallback to commands, so the category stays clean)._

1. **Default (no prefix) = UI only:** actions + settings. Console/protocol commands no longer appear unless asked for.
2. **Scope prefixes** (VS Code-style, first character of the query):
   - `!` — console / protocol commands (built-in `!` commands + `.ptc` extensions; existing behaviour, unchanged);
   - `>` — actions only;
   - `@` — settings only.
3. `palette_search`: replace the `bang` special case with a general scope filter (a set of `kind`s); keep `search(query, limit)` behaviour for `!` and add the prefixes; scope parsing is pure and unit-tested.
4. Palette UI: placeholder text and the empty state list the prefixes; the active scope is shown (e.g. a small chip or the row group) so the filter is visible.
5. Dataset/benchmark: add scope rows to `queries.tsv` (default query must **not** return `cmd.*`; `>`/`@`/`!` return only their kind); rerun `scripts/palette_bench.py`.
6. `CHANGELOG`/palette empty-state text updated.

## Scope boundary

- No change to what `!` commands do or to `CommandDispatcher::executeLine()`.
- Not the UI-language work (`features/ui-language/`).
- No new engine NLP stages.

## Reasoning

Separating by kind fixes the noise at its source (a category error, not a scoring problem) and is cheap because every entry already carries a `kind`. Rejected: down-ranking commands instead of excluding them (still pollutes short queries; the earlier 0.9× penalty is a patch for exactly this). Resolved 2026-10-08: (a) are `>`/`@` the right characters, or only `!` plus scope chips/Tab-to-cycle; (b) should a default query that has **no UI match** fall back to showing commands, or stay empty?

## Knowledge

`features/command-palette/planning.md`; `docs/knowledge/` UX heuristics note (discoverability, recognition over recall); `nlp-semantic-search-embeddings` (evaluate on own queries).

## Acceptance criteria

- A query without a prefix never returns `cmd.*`; `!…` returns only `cmd.*`; `>…` only `act.*`; `@…` only `set.*` — unit-tested.
- Benchmark rerun: no regression in MRR/recall on the existing categories (bang rows still pass); noise from `cmd.*` in default results is gone.
- Palette placeholder/empty state documents the prefixes; scope visible while typing.

**Resolution of open points (2026-10-08):** (a) keep `!` `>` `@` only; no Tab-cycling in this task. (b) stay empty, with a hint line suggesting `!` when the query has no UI match.
