# Instruction — I18N-03: language setting

## Approach
Storage key and parse first (test), then the registry row, then startup resolution and the signal.

## Pitfalls
- Unknown `language=` value must not crash or stick; fall back to `system`.
- Add dataset rows for `set.language` and rerun `scripts/palette_bench.py` (without `-I`).

## Verification before done
Round-trip test; launch with `LANG=vi_VN.UTF-8` and `LANG=fr_FR`.

## Boundaries
No string wrapping, no catalog content.
