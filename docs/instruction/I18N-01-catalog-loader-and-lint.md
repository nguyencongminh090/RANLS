# Instruction — I18N-01: catalog loader + lint

## Approach
Write the parser and `tr()` as a GTK-free unit with tests first; loader takes text so tests need no GResource. Make the lint a ctest that scans `tr("…")` literals, not a hand-kept list.

## Pitfalls
- Escapes (`\t`, `\n`) and `%`/`{n}` placeholders must survive round-trip; compare placeholder multisets, not order.
- Empty translation means "untranslated", not blank UI.

## Verification before done
`RUN_TESTS=1 ./build.sh`; negative lint fixtures fail as intended.

## Boundaries
No gtk/engine includes in `src/i18n/`; no UI edits.
