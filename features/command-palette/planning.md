# Command Palette — planning

See [user_story.md](user_story.md) · [diagram/flow.md](diagram/flow.md).

## Status

Design draft 2026-10-08. **All open questions resolved 2026-10-08 — user accepted the proposed default for Q1–Q10 and agreed the Whoosh-baseline benchmark + dataset-authoring plan.** Formalised into `docs/todo/PAL-01/02/03` + `docs/instruction/`, filed to `TODO.md` Backlog; audit in `docs/audit/2026-10-log.md`.

(Original status:) Decisions from the user (2026-10-08): name `command-palette`; scope = actions +
settings + `!` commands; languages = English + Vietnamese; **classical NLP only**, no neural net.
**Size L** (new cross-layer module + registries) → resolve
questions, then `docs/todo/` + `docs/instruction/`. Not authorization to implement.

## Proposed pipeline (knowledge: `nlp-semantic-search-embeddings`, `nlp-vietnamese-multilingual`)

The corpus is tiny (~100–300 short entries), so brute-force scoring is instant; no ANN, no chunking.

**Entry**: `{id, kind, title_en, title_vi, keywords_en/vi, group, shortcut, enabled()}` — hand-written
bilingual strings live in one data table, so Vietnamese support is *search vocabulary*, not UI i18n.

**Index/query normalisation (same function for both sides):**
1. Unicode NFC (`g_utf8_normalize`) + casefold — mixed composed/decomposed Vietnamese must match.
2. Build two forms: *exact* (diacritics kept) and *folded* (NFD, strip combining marks, `đ→d`). Match on
   folded; exact-diacritic match only adds a small bonus. Folding is deliberate robustness for users who type
   without a Vietnamese IME; the note warns it merges words, hence the bonus keeps true matches on top.
3. Tokenize on whitespace/punctuation. Vietnamese space ≠ word boundary: do **maximum matching against
   the lexicon's phrase list** (e.g. "cài đặt", "ván mới") so multi-syllable words stay one token; unknown
   syllables stay single tokens. No external segmenter (VnCoreNLP is Java — rejected).
4. Drop stopwords (en: the/a/to/open?; vi: các/của/là/hãy/cho/tôi/muốn…). Light English suffix
   stemming (s/es/ing/ed/ise→ize); none for Vietnamese.

**Retrieval/ranking:**
5. Expand each query token: synonym lexicon (weight 0.7); fuzzy vocabulary hits via Damerau–Levenshtein
   (≤1 for len 4–7, ≤2 for ≥8, weight 0.5; reuse CONS-02's edit-distance); the *last* token also prefix-matches
   (as-you-type); character-trigram Jaccard as a fallback when nothing matched.
6. Score with **BM25F** (fields: title ×3, keywords ×2, group ×1) — BM25 is the robust baseline and wins on
   exact terms; the synonym lexicon covers its vocabulary-mismatch weakness without embeddings.
7. Boosts: exact title / prefix of title; exact-diacritic; most-recently-used (small, persisted); disabled
   entries demoted; leading `!` filters to `kind=command`.
8. Return top 8 with matched-span highlights.

**Evaluation:** a golden set of 50+ bilingual queries → expected entry id, run as a doctest reporting
recall@1/@3/@8 and MRR; a regression gate (e.g. recall@3 ≥ 0.9) plus a latency smoke (<5 ms/query).

**Benchmark: our model vs. a Whoosh baseline (dev-only; clarified 2026-10-08).** Two retrieval models
are run over **one shared dataset that we author** and compared on accuracy and performance:

- *Model A* — ours: the C++ pipeline above (`palette_search`).
- *Model B* — baseline: Python 3.13 + Whoosh (BM25F, `StemmingAnalyzer`), same shared pre-tokenizer
  (NFC/fold/phrase max-match) and same stopword/synonym lists so the comparison isolates the ranking
  logic, plus a "stock Whoosh" variant with no Vietnamese handling to show what our NLP adds.

*Shared dataset* (`tests/data/palette/`, TSV, versioned, single source of truth for both models):
`entries.tsv` (~150–300 realistic entries: id, kind, title_en/vi, keywords, group — modelled on this
app's real actions/settings/`!` commands) and `queries.tsv` (200+ labelled queries: query → relevant
entry id(s), tagged by category: exact, synonym, typo, prefix, no-diacritics, Vietnamese, mixed-language,
`!`-filter, negative/no-match). Split dev/test so the lexicon is not tuned on the test queries.

*Accuracy*: recall@1/@3/@8, MRR, nDCG@8 (graded where several ids are relevant), overall and per category,
plus paired significance (bootstrap) on MRR. *Performance*: index build time, p50/p95 per-query latency
(warm, single thread, same machine), peak RSS, and per-keystroke cost for as-you-type. Output: a Markdown
report (`scripts/palette_bench.py` → `docs/audit/…` when decided on) with a per-query disagreement list.

Rules: dev-only, **not** a runtime dependency and not a CI gate (the doctest golden-set gate stays on
our model only). The C++ side exposes a tiny CLI/test mode reading `queries.tsv` and emitting ranked ids
+ timings, so the Python script just diffs two outputs. Caveats: Whoosh 2.7.4 is unmaintained (2016) and
may not import on Python 3.13 (unverified; fallback fork `Whoosh-Reloaded`); set Whoosh's BM25F `K1`/`B`
to ours; keep timing fair (cross-process timing excludes startup; Python's in-process timing for B,
C++ in-process for A); run in a venv outside the repo with `python3.13 -I`.

**Layering** (rule 8): pure `src/command/palette_search.{h,cpp}` (no GTK; unit-tested) ← `PaletteEntry`
providers in `command`/`ui` ← `src/ui/command_palette.{h,cpp}` (overlay + Ctrl+K accel). No `model/` changes.

## Open questions (all resolved — Resolution = the Proposed default column)

| # | Question | Proposed default |
|---|---|---|
| Q1 | Settings searchability: no settings registry exists today (`settings_dialog.cpp` builds widgets directly). Add a small declarative `SettingEntry` table, or search only top-level "Settings" page? | Declarative table; jump to page + focus widget |
| Q2 | Action discovery: actions are registered ad hoc in `MainWindow::buildMenuBar`. Register each with palette metadata at the same call site, or derive from the `Gio::Menu` model? | Explicit metadata at registration (menu labels lack vi/keywords) |
| Q3 | Command with arguments (`!analyze [n]`): run immediately, or insert into Engine Log entry and focus it? | Insert if `usage` has args, else run |
| Q4 | Overlay form: modal-ish popover/`Gtk::Window` centred on main window vs. inline header-bar `SearchEntry` (UI-21 chrome)? | Centred overlay (VS Code style) |
| Q5 | Lexicon authoring: who maintains vi/en synonyms — plain data file (`data/palette_lexicon.tsv`, installed) or compiled-in table? | Installed data file, loaded once |
| Q6 | MRU persistence: store in existing config (`model/config`)? Privacy is a non-issue (local, ids only). | Yes, top-50 ids |
| Q7 | Full UI i18n (labels in Vietnamese) is a separate feature — confirm out of scope here. | Out of scope; separate todo later |
| Q8 | Ctrl+K collision check with GTK entries/other accels (none found in `src/` today). | Window-level accel, works from any widget |
| Q9 | Split: `PAL-01` search engine + eval (pure), `PAL-02` overlay UI + registries, `PAL-03` settings registry? | Yes, in that order |
| Q10 | Dataset authoring: I draft `entries.tsv` + `queries.tsv` from the app's real actions/settings/commands and you review the vi labels/synonyms (native check, per `nlp-data-annotation-splits`)? Also OK for a dev/test split of ~50/50? | Yes: I draft, you review Vietnamese; 50/50 split |
