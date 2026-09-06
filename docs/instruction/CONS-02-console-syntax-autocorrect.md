# Instruction — CONS-02: Engine Log command AutoCorrect

## Approach

Feature. Depends on **CONS-01** (`CommandDispatcher::registeredNames()` + the pure
`CommandCompleter` TU). If CONS-02 ships first, pull those two seams over from the CONS-01
scope and note it in both summaries.

`features/console-autocomplete/` Q3/Q4/Q5/Q9/Q10 are resolved (2026-09-07) — don't re-open.

Build order:

1. Add to `CommandCompleter` (pure): `normalizeCase(token, names)` → `optional<string>`;
   `didYouMean(token, names)` → `vector<string>` (≤ 3), Levenshtein ≤ 2 **and** ≤ ⌊len/2⌋;
   `missingBangFix(entryText, names)` → `optional<string>` (exact case-insensitive first-token
   match only; returns the `!`-prefixed rewrite).
2. Two small **read-only** helpers on `CommandDispatcher`: `nearestNames(token)` (wraps
   `didYouMean` over `registeredNames()`), and make the `handlers_.find(parsed.cmd.name)`
   lookup in `executeLine()` case-insensitive (Q9 defense-in-depth — normalise the key, not
   the routing).
3. Pre-dispatch hook in `BottomPanel::commandEntry_.signal_activate` — *before* it emits
   `signal_command_sent`: run `missingBangFix` then `normalizeCase` on the entry text; if
   either changed it, `begin_user_action`/`set_text`/`end_user_action` (so Ctrl+Z restores),
   append the `MESSAGE`-tag `corrected: …` log line, then emit the corrected string.
4. B4 wiring: replace the bare "Unknown internal command" branch in `executeLine()` so it
   calls `nearestNames()` and prints the "Did you mean" line when non-empty (keep the old
   text when empty).

## Pitfalls

- **B1 must never guess.** Exact case-insensitive first-token match against a registered name,
   nothing fuzzier. `analyse 10` (British spelling, not registered) stays a raw line. If you
   let edit-distance into B1 you'll "correct" a deliberate raw command into a `!` command and
   desync the engine — the exact failure `executeLine()`'s dangerous-command block exists to
   prevent.
- **Fullwidth `！` (B3) is already handled in `executeLine()`.** Your hook sees the raw entry
   text *before* `executeLine`. If it starts with `！`, treat the bang as present — only
   `normalizeCase` the command token, never touch or duplicate the bang. Test `！ANALYZE` →
   one correction line, not two, and the fullwidth bang preserved.
- **Order matters: `missingBangFix` before `normalizeCase`.** `analyze` (no bang, lowercase,
   registered) → `missingBangFix` → `!analyze`; `normalizeCase` then no-ops. `ANALYZE` (no
   bang) → `missingBangFix` matches case-insensitively and emits the registered spelling
   `!analyze` in one step — so `missingBangFix` returns the *registered* spelling, not the
   user's casing.
- **One "corrected" line max per submit**, even if both transforms fire (they shouldn't, given
   the above, but guard it).
- **Ctrl+Z scope.** `set_text` inside a single `begin/end_user_action` gives one undo step.
   Verify the entry actually restores (GTK4 `Gtk::Entry` undo is on by default; confirm it
   isn't disabled on this widget).
- **Don't double-log.** The final dispatched line is already logged as SEND by the
   `engine_.signal_line_sent` path. The `corrected:` line is *additional* context, tagged
   `MessageRecv`-style (`LogTagKind` — pick the closest existing tag; do not add a new tag
   kind for this).
- **`nearestNames` over the full live registry**, built-ins + `.ptc` — same `registeredNames()`
   seam as CONS-01, re-queried, not cached.

## Do not touch

- `executeLine()`'s `!` routing, the dangerous-external-command block, pos-session capture —
   only the *unknown-command error text* branch changes, and only to add the "did you mean".
- `builtinCommandNames()` in `protocol_extension.cpp` (R1 — no new name list).
- The CONS-01 popover/ghost-text — this is the submit path, orthogonal to live suggestion.
- Arg-value correction (coord format, rule spelling) — out of scope.

## Testing

- Pure `tests/test_cons02_autocorrect.cpp`: `normalizeCase("ANALYZE",…)→"analyze"`,
   `normalizeCase("analyze",…)→nullopt`; `didYouMean("anlyze",…)→["analyze"]`,
   `didYouMean("xyzzy",…)→[]`, tie case returns ≤ 3; `missingBangFix("analyze 10",…)→"!analyze 10"`,
   `missingBangFix("foo",…)→nullopt`, `missingBangFix("！analyze",…)→nullopt` (bang present).
- Dispatcher test: case-insensitive `handlers_` lookup runs `!ANALYZE`; `nearestNames("undoo")`
   → `["undo"]`.
- UI (headless if possible, else pure-only + note): submit `analyze 10` through a real
   `BottomPanel`, assert `signal_command_sent` carried `!analyze 10`, entry shows it, and a
   `corrected:` line was appended.
