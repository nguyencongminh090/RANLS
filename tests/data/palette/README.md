# Command-palette dataset (PAL-01)

Shared by both retrieval models (our C++ `palette_search` and the Whoosh baseline). Draft v0, 2026-10-08.

- `entries.tsv` — `id, kind, title_en, title_vi, keywords_en, keywords_vi, group`. Keywords are `;`-separated.
  61 entries taken from the app's real actions (`MainWindow::buildMenuBar`), settings (`settings_dialog.cpp`)
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
