# Instruction — PAL-04: palette scope prefixes

## Approach
Pure scope parser (prefix -> set of kinds) in `palette_search` with tests first; replace the `bang` special case and remove the 0.9x `!` penalty only if the benchmark does not regress.

## Pitfalls
- Default query must never return `cmd.*`; empty state hints at `!`.
- Add scope rows to `queries.tsv` without touching the test split's existing rows; rerun the bench.

## Verification before done
Unit tests + `RUN_TESTS=1 ./build.sh` + bench rerun (MRR/recall no regression).

## Boundaries
No change to `CommandDispatcher::executeLine()` or what `!` commands do.
