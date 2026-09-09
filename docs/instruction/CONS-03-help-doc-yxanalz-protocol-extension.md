# CONS-03 — help-doc-yxanalz-protocol-extension

## Approach

Help text only. The `!help` output is built entirely from the registered `CommandSpec`s
(`printHelp()` in `src/command/command_dispatcher.cpp` — group → sort → `usage` + " — " + `summary`),
plus a fixed 4-line preamble. Two levers, prefer the first:

1. **Denser `summary` on the `!yxAnalz` `CommandSpec`** (in `registerBuiltins`). One line, so keep
   it tight: name the protocol-extension dependency + "no stone placed" + "alphabetic moves".
2. **One extra preamble line in `printHelp()`** if (1) can't carry it — e.g. after the
   "raw protocol (restricted)" line, note that some `[analysis]` commands drive engine-specific
   protocol extensions (`!yxAnalz` → `YXANALZ`) and need an engine that implements them.

No new command, no new group, no `CommandSpec` field, no grouping/sort change.

## Pitfalls

- **`tests/test_proto07_yxanalz_console.cpp` pins the usage string exactly** —
  `CHECK(disp.commandUsage("yxanalz") == "!yxAnalz <moveText...>")` and a `!help` scan for
  `"!yxAnalz"`. Changing `usage` breaks the first; changing only `summary` is free. If you must
  move `usage`, update that assertion in the same commit.
- **Preamble lines are printed unconditionally** (no engine-running guard) — fine, but keep them
  short; the console log is narrow and RT-02-bounded.
- **Don't touch the `[analysis]` group membership** — `!yxAnalz` is correctly grouped; this is
  wording only.
- The PROTO-07 `CHANGELOG.md` `[Unreleased]` entry already describes `!yxAnalz` for users — don't
  duplicate it.

## Verification before done

- `./build.sh` clean, no new warnings.
- `ctest` 4/4 green; `test_proto07_yxanalz_console.cpp` in particular (the `!help`/`commandUsage`
  cases).
- Eyeball: run the app or a small harness, `!help`, confirm the `[analysis]` block / preamble now
  conveys (a) protocol-extension dependency, (b) real search, (c) no stone, (d) alphabetic input.

## Boundaries

- Only `src/command/command_dispatcher.cpp` in `src/` (+ the one test file if `usage` moves).
- No behaviour change: `parseMovesText`, `analyzeMoves`, `generateAnalyzeMovesRequest`,
  `isDangerousExternalEngineCommand`, the raw-passthrough path — all untouched.
- No numeric `x,y` console input. No `README.md` command reference. No `printHelp()` restructure.
