# CONS-02 — Engine Log command entry AutoCorrect, plus a latent `~BottomPanel` flush-timer UAF the tests exposed

**Status:** ✅ FIXED

Tracked feature `CONS-02` (Sprint 16). Branch `cons-02/console-syntax-autocorrect`. The feature
itself is specified in [docs/todo/CONS-02-console-syntax-autocorrect.md](../todo/CONS-02-console-syntax-autocorrect.md)
and [docs/instruction/CONS-02-console-syntax-autocorrect.md](../instruction/CONS-02-console-syntax-autocorrect.md);
this entry records the work and the one non-scope defect fixed along the way.

## Prompt

`/implement-task CONS-02` — add AutoCorrect (missing `!`, wrong case, "did you mean", fullwidth
`！`, visible + undoable rewrite) to the Engine Log command entry, per the resolved
`features/console-autocomplete/` design (Q3/Q4/Q5/Q9/Q10).

## Action — feature

- **`src/command/command_completer.h`** (pure, GTK-free): `levenshtein` (case-insensitive,
  two-row DP), `normalizeCase(token, names) → optional<string>` (registered spelling only when
  case differs), `didYouMean(token, names) → vector<string>` (Levenshtein ≤ 2 **and** ≤ ⌊len/2⌋,
  best-first, ≤ 3, exact match never suggested), `missingBangFix(entryText, names) →
  optional<string>` (exact case-insensitive first-token match **only** — no edit distance;
  returns the `!`-prefixed rewrite using the *registered* spelling; nullopt when a bang — ASCII
  or fullwidth — is already present), `autoCorrect(entryText, names)` (composes `missingBangFix`
  then `normalizeCase`, at most one rewrite).
- **`src/command/command_dispatcher.{h,cpp}`**: read-only `nearestNames(token)` wrapping
  `didYouMean` over the live `registeredNames()`; a case-insensitive fallback scan in the
  `handlers_.find(parsed.cmd.name)` lookup in `executeLine()` (normalises the key, not the
  routing — Q9 defense-in-depth); the bare `Unknown internal command` branch now emits
  `Unknown internal command '<tok>'. Did you mean: !x?` when `nearestNames()` is non-empty,
  keeping the old `(try: !help)` text when empty (B4). `!` routing, the dangerous-external-command
  block and pos-session capture are untouched.
- **`src/ui/bottom_panel.cpp`**: the `commandEntry_.signal_activate` handler runs `autoCorrect`
  on the entry text *before* emitting `signal_command_sent`; on a change it sets the entry text
  once (one Ctrl+Z step), appends a single `LogTagKind::RecvMessage` `corrected: …` line via the
  existing `appendLogLine`, then dispatches the corrected string. `commandNameProvider_` (from
  CONS-01) feeds the live registry; `suppressSuggest_` guards the programmatic `set_text`.

Shared CONS-01 seams (`CommandCompleter` TU, `CommandDispatcher::registeredNames()`) already
shipped in `04f70f6`; this task built on them, adding no new command-name list
(`builtinCommandNames()` untouched).

## Action — latent defect fixed (not in CONS-02 scope, required by the headless UI test)

`~BottomPanel()` never disconnected `flushTimerConn_`, the RT-02 batch-flush timeout connected in
the constructor with `sigc::mem_fun(*this, &BottomPanel::flushPending)`. `sigc::mem_fun` does not
track object lifetime, so after a `BottomPanel` is destroyed its 50 ms recurring timer stays
registered in the default main context and fires `flushPending()` on freed memory the next time
the loop iterates. In the app a single `BottomPanel` lives for the whole process, so it never
bit; the CONS-02 tests construct and destroy several panels in one process and pump the main
loop, which surfaced it as a SIGSEGV. Fix: `flushTimerConn_.disconnect()` at the top of the
destructor (same class of fix as the PORT-03 `sigc::track_obj` scroll-idle fix).

## Verification

Clean `./build.sh` (Ninja, Release) + `ctest --test-dir build_cmd`:

- `ranls-gui-tests` — 218 cases / 2532 assertions, 0 failed. Includes new pure
  `tests/test_cons02_autocorrect.cpp` (levenshtein, `normalizeCase`, `didYouMean` threshold +
  tie-cap + "exact never suggested", `missingBangFix` exact-match-only + bang-present cases,
  `autoCorrect` composition incl. fullwidth `！` preserved and near-miss `analyse 10` left alone).
- `ranls-gui-ui-tests` — all cases pass, 0 failed, 0 crashes. Includes new
  `tests/test_cons02_autocorrect_dispatch.cpp`: `nearestNames("undoo") → ["undo"]`,
  case-insensitive `!ANALYZE` routing (reaches the analyze handler), B4 `Did you mean: !analyze?`
  text vs. unchanged `!xyzzy` error, and the real-widget submit path (`analyze 10` → dispatched
  `!analyze 10`, entry shows it, `corrected: !analyze 10` logged `RecvMessage`; `!ANALYZE` →
  `!analyze`; fullwidth `！analyze` → unchanged, no `corrected:` line; `foo bar` → passed through).
- `port02-style-css-bundled`, `rel02-version-single-source` — pass.
- Live-app smoke not run (no engine binary on the build host); the submit path is covered by the
  headless real-`BottomPanel` test above.

## Related

- [docs/todo/CONS-02-console-syntax-autocorrect.md](../todo/CONS-02-console-syntax-autocorrect.md),
  [docs/instruction/CONS-02-console-syntax-autocorrect.md](../instruction/CONS-02-console-syntax-autocorrect.md)
- `features/console-autocomplete/planning.md` — Q3/Q4/Q5/Q9/Q10 resolution.
- `docs/fix-log/2026-09-06-port-03-msvc-build-and-portable-test-harness.md` — the earlier
  `BottomPanel` idle-callback UAF (deferred scroll idles); same root cause class as the flush
  timer here.
