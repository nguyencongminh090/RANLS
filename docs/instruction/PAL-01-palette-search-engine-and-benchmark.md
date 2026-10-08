# Instruction — PAL-01: palette search engine + benchmark

## Approach

Feature. `features/command-palette/` Q1–Q10 resolved 2026-10-08 — don't re-open. Order: (1) dataset, (2) engine,
(3) doctest gate, (4) Whoosh bench. Write the dataset **before** the engine so the engine isn't fit to hand-picked queries.

## Pitfalls

- Normalise NFC first; fold diacritics on a *copy* — keep exact form for the bonus.
- Vietnamese: never whitespace-split into "words"; max-match phrases from the lexicon, fall back to syllables.
- Don't tune on the test split; report per-category numbers (an aggregate hides a broken Vietnamese path).
- Fair benchmark: same pre-tokenizer/lists for Whoosh; set Whoosh `K1`/`B` to ours; time in-process, warm.
- Whoosh dev-only (`python3.13 -I`, venv outside repo); never link or ship it.

## Do not touch

- `executeLine()` routing; `model/`; the CONS-01/02 completer semantics (reuse edit-distance only).
