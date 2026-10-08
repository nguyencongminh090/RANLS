# Command-palette dataset (PAL-01)

Shared by both retrieval models (our C++ `palette_search` and the Whoosh baseline). Draft v0, 2026-10-08.

- `entries.tsv` — `id, kind, title_en, title_vi, keywords_en, keywords_vi, group`. Keywords are `;`-separated.
  62 entries taken from the app's real actions (`MainWindow::buildMenuBar`), settings (`settings_dialog.cpp`)
  and `!` commands (`CommandDispatcher`). **Smaller than the planned 150–300**: that is the app's actual
  surface; a synthetic scale-up corpus is generated for latency tests only.
- `queries.tsv` — `category, query, relevant_ids, split`. `relevant_ids` is comma-separated (all acceptable
  answers; empty = no match expected, category `neg`). `split` alternates dev/test within each category;
  tune on `dev` only, report on `test`.
- Categories: exact, synonym, typo, prefix, nodiac (Vietnamese without diacritics), vi, mixed (en+vi), bang
  (`!` filter), neg.

**Status: draft — Vietnamese titles/keywords/queries await review by a native speaker (the user).**
Do not tune the lexicon against `test` rows. Regenerate nothing by hand-editing ids without re-checking that
every `relevant_ids` entry exists in `entries.tsv`.

**I18N-03.** `set.language` (the Language dropdown) added to `entries.tsv` with eight `queries.tsv` rows (all categories except neg/prefix/bang); existing rows untouched.

**PAL-04 (scope prefixes).** A query with no prefix searches actions + settings only and never returns `cmd.*`;
`!` = commands only, `>` = actions only, `@` = settings only. Category `scope` holds prefixed rows (added, existing rows untouched).
Eight pre-PAL-04 rows have only `cmd.*` answers and no `!` (`commands list`, `clear log`, `raw protocol`, `yxanlz`, `loadpoz`,
`getpoz`, `xem trợ giúp`, `xóa log`): by design they must now return no `cmd.*`; `palette_bench.py` excludes them from the metric
tables and lists them under "Rows moved to the `!` scope", and the golden-set unit test asserts they leak no `cmd.*`.
