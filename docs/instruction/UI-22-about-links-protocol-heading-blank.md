# Instruction — UI-22: About "Links & protocol" heading blank

## Approach
Invoke `systematic-debugging` and follow `.claude/rules/bugfix.md` before any change: reproduce, capture the warning, trace, then one fix at the source with a failing-first regression test.

## Pitfalls
- The `&`-in-markup cause is a hypothesis; verify before fixing.
- Do not change the English string or the `vi.tsv` key (the coverage lint ties them together).
- Check `makeSection` callers for the same defect, but only fix what is proven.

## Verification before done
Failing test first, then green; `RUN_TESTS=1 ./build.sh` under `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY`; screenshot of About in en and vi; fix-log row + detail.

## Boundaries
No About redesign, no other dialogs, no catalog edits.
