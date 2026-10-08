# PAL-01 — Command palette: pure search engine + shared dataset + Whoosh benchmark

**Status:** ✅ DONE (2026-10-08, PR #56 squash `dccc5ca`) — engine + shared dataset (61 entries / 199 queries, dev/test) + doctest gate + `ranls-palette-eval` + `scripts/palette_bench.py`. Test split (98 q): MRR A 0.980 / Whoosh+shared prep 0.950 / stock Whoosh 0.719; p50 3 µs vs ~1 ms. Caveats: dataset is small and author-written; kept at 61 real entries (synthetic scale corpus for latency not built); queries 199 (< 200). Vietnamese labels reviewed by the user.
**Area:** `src/command/palette_search.{h,cpp}`, `src/command/palette_lexicon.*`, `tests/data/palette/`, `tests/test_pal01_*.cpp`, `scripts/palette_bench.py`
**Priority:** P3
**Source:** user request 2026-10-08 (Ctrl+K search to browse functions faster; classical NLP, en/vi)
**Design:** features/command-palette/
**Depends on / relates to:** PAL-02, PAL-03; reuses CONS-02 edit-distance (`command_completer`)

## Problem

Users cannot quickly find app functions (actions, settings, `!` commands) without knowing where they live. Need a GTK-free, unit-tested ranking engine for English + Vietnamese queries, using classical NLP only (no neural net).

## Scope (in order)

1. Shared dataset first: `tests/data/palette/entries.tsv` (~150–300 entries) + `queries.tsv` (200+ labelled, categorised, dev/test 50/50 split). Drafted by Claude, Vietnamese labels/synonyms reviewed by the user.
2. `palette_search`: NFC+casefold, diacritic folding (`đ→d`), lexicon phrase max-match tokenizer, stopwords, light English stemming, synonym/fuzzy/prefix expansion, BM25F (title×3, keywords×2, group×1), boosts (exact, diacritic, MRU, enabled, `!` filter), top-8 with highlight spans.
3. Lexicon as installed data file (`palette_lexicon.tsv`), loaded once.
4. Doctest golden-set gate (our model only) + latency smoke; a CLI/test mode that reads `queries.tsv` and prints ranked ids + timings.
5. `scripts/palette_bench.py` (Python 3.13 + Whoosh, venv outside repo): Model B (Whoosh, same pre-tokenizer + lists) and "stock Whoosh"; reports recall@1/3/8, MRR, nDCG@8 per category, bootstrap on MRR, build time, p50/p95 latency, RSS, disagreement list.

## Scope boundary

- No GTK, no UI, no `model/` changes. Whoosh is dev-only: not linked, not a CI gate.
- No embeddings/transformers. No UI i18n (separate feature).

## Reasoning

BM25 is the robust baseline for exact terms; the synonym lexicon covers vocabulary mismatch on a ~300-entry corpus. Rejected: VnCoreNLP (Java), embeddings (user excluded), Whoosh at runtime (Python dep in a C++ GUI).

## Knowledge

`nlp-semantic-search-embeddings`, `nlp-vietnamese-multilingual` (SKILLS_TREE knowledge/nlp); `docs/knowledge/principle-layering-dependency-rule.md`; CONS-01/02 audit trail.

## Acceptance criteria

- Dataset reviewed by the user; test split untouched while tuning.
- Golden-set recall@3 ≥ 0.9 (threshold confirmed after the first benchmark run), < 5 ms/query.
- `palette_bench.py` produces the A-vs-B report; results recorded in an audit entry.
