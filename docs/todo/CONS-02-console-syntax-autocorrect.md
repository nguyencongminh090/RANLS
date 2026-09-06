# CONS-02 — Engine Log command entry: AutoCorrect (syntax fix-ups)

**Status:** 🔲 OPEN (Backlog) — filed 2026-09-07.

Feature B of `features/console-autocomplete/` (see
[planning.md](../../features/console-autocomplete/planning.md) Resolution table, 2026-09-07).
**Depends on CONS-01** for `CommandDispatcher::registeredNames()` and the pure
`CommandCompleter` module (both land with whichever of CONS-01/CONS-02 ships first).

## Source

Same user request as CONS-01 (2026-09-07): *"AutoCorrect text typing on syntax."* Q3/Q4/Q5/
Q9/Q10 resolved with the user the same day.

## Problem

The command entry today turns near-misses into bare errors in
[src/command/command_dispatcher.cpp:100-166](../../src/command/command_dispatcher.cpp#L100-L166):

- A known command typed **without** `!` → `ERR: This is an internal command. Prefix with '!'`.
- Wrong case (`!ANALYZE`) → `ERR: Unknown internal command`.
- A typo (`!anlyze`) → `ERR: Unknown internal command: anlyze (try: !help)`.

Each is a dead end the user has to fix by hand.

## Scope

Add these to `CommandCompleter` (pure, from CONS-01) and a pre-dispatch hook in `BottomPanel`
(in the `signal_activate` handler, before `signal_command_sent` is emitted — the UI rewrites
the entry text visibly, then dispatches the corrected line):

1. **B1 — missing bang.** On Enter, if the trimmed input does **not** start with `!`/`！` and
   its first whitespace token is an **exact case-insensitive** match to a `registeredNames()`
   entry → rewrite to `!<registeredSpelling> <rest>`, show it in the entry, then dispatch.
   Any other no-bang line passes through unchanged to the existing raw-line rules (R2/Q3).
2. **B2 — wrong case.** `normalizeCase(token, names)` → if `token` matches a registered name
   case-insensitively but not exactly, rewrite to the registered spelling. **Also** make the
   dispatcher's `handlers_` lookup case-insensitive as defense-in-depth (Q9) — a small change
   inside the existing lookup in `executeLine()`, not a routing change.
3. **B3 — fullwidth bang.** Already normalised in `executeLine()`. AutoCorrect must **not**
   double-handle or regress it — if the line starts with `！`, leave the bang alone, only
   touch the command token.
4. **B4 — did you mean.** `didYouMean(token, names)` → Levenshtein ≤ 2 **and** ≤ ⌊len/2⌋.
   On an unknown `!` command, emit one log line: `ERR: Unknown internal command 'anlyze'.
   Did you mean: !analyze?` (up to 3 names on a tie). **No auto-run** (Q4). This replaces the
   current bare "Unknown internal command" text for that case — a read-only
   `CommandDispatcher::nearestNames(token)` helper feeds it (HC3: new method, not a routing
   change).
5. **B5 — visible + undoable.** Every automatic rewrite updates the entry text *before*
   dispatch (so a `begin_user_action`/`end_user_action` pair makes Ctrl+Z restore the
   original), and when a correction changed the input, one `LogTagKind` `MESSAGE` line is
   appended: `corrected: !analyze` (Q10). Never a silent rewrite.

## Acceptance criteria

- `analyze 10` (no bang) + `analyze` is registered → dispatched as `!analyze 10`, entry shows
  `!analyze 10`, log shows `corrected: !analyze 10`, Ctrl+Z restores `analyze 10`.
- `foo bar` (no bang, `foo` not registered) → unchanged; existing raw-line / dangerous-command
  rules apply exactly as today.
- `!ANALYZE` → runs `analyze`, `corrected: !analyze` logged.
- `！analyze` (fullwidth) → runs, bang untouched, no spurious "corrected" line.
- `!anlyze` → `ERR: Unknown internal command 'anlyze'. Did you mean: !analyze?`, nothing run.
- `!xyzzy` (no near match) → today's `Unknown internal command` error, unchanged.
- `./build.sh` clean; `ctest` both suites green; `tests/test_cons02_autocorrect.cpp` pins
  `normalizeCase` / `didYouMean` / the B1 "exact first-token match" predicate (pure). A
  dispatcher test covers the case-insensitive `handlers_` lookup (Q9) and `nearestNames()`.

## Scope boundary

- Only the command entry's pre-dispatch step + two small read-only dispatcher helpers +
  the one case-insensitive-lookup change. `executeLine()`'s `!` routing, the
  dangerous-external-command block, and pos-session capture are untouched (HC3).
- **B1 never guesses.** It fires only on an *exact* case-insensitive first-token match.
  A no-bang typo (`analyse 10`) is left to the raw-line path, not corrected.
- No interactive "accept this correction?" prompt — v1 is auto-on-exact (B1/B2) or
  log-line-only (B4). Interactive accept is explicitly deferred (planning Q3/Q4).
- No arg-value correction (coord normalisation, rule-name spelling) — out of scope, same as
  CONS-01's no-arg-completion boundary.
- No new engine traffic (R4).
