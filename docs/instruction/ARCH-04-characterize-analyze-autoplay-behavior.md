# Instruction — ARCH-04

Detail: [docs/todo/ARCH-04-characterize-analyze-autoplay-behavior.md](../todo/ARCH-04-characterize-analyze-autoplay-behavior.md)

## Approach

Read the existing ANLZ/ENG probes first and extend them; list covered vs new cases before writing tests.

## Pitfalls

The tests must pass on current `main` unchanged. If a test exposes a real bug, stop and follow `systematic-debugging` / `.claude/rules/bugfix.md` — do not fix it here. UI tests need a display; they auto-skip without one, so say whether they actually ran.

## Verification before done

`RUN_TESTS=1 ./build.sh` green; `git diff --stat` shows only `tests/` and tracking files.

## Boundaries

No `src/` changes. Do not retire existing probes.
