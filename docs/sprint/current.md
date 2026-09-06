# Current sprint

## Sprint 16

**Goal:** Engine Log command console: autocomplete + autocorrect

**Dates:** 2026-09-07 to — (open — no fixed end date set yet)

**Dependency graph:**

- **CONS-01** (AutoComplete) — new pure TU `src/command/command_completer.{h,cpp}` (no GTK,
  unit-tested like `engine_log_model.h`) + read-only `CommandDispatcher::registeredNames()` /
  `commandUsage()` + UI glue in `src/ui/bottom_panel.cpp` (custom `Gtk::Popover` **and** inline
  ghost-text — GTK4 has no `Gtk::EntryCompletion`). Extend the *one* existing
  `EventControllerKey` lambda; branch on "popover visible?"; leave `commandHistory_` Up/Down nav
  untouched when the popover is closed. Re-query the registry per popover-open (`.ptc` re-sync).
  Must not touch `executeLine()` routing, the dangerous-raw-command block, `builtinCommandNames()`
  in `protocol_extension.cpp` (R1 — no third name list), or the RT-01/RT-02/UI-05/UI-10/UI-15
  Engine-Log machinery. No argument-level completion (Q6). No `systematic-debugging` gate — feature.
- **CONS-02** (AutoCorrect) — **depends on CONS-01** (shared `CommandCompleter` + `registeredNames()`
  land with whichever ships first). Adds pure `normalizeCase` / `didYouMean` (Levenshtein ≤ 2 and
  ≤ ⌊len/2⌋) / `missingBangFix` (exact case-insensitive first-token match **only** — never fuzzy)
  + read-only `CommandDispatcher::nearestNames()` + a case-insensitive `handlers_` lookup + a
  pre-dispatch hook in `commandEntry_.signal_activate` (visible rewrite inside
  `begin/end_user_action` so Ctrl+Z restores; one `corrected:` MESSAGE log line). `！` fullwidth
  bang is already handled in `executeLine()` — do not double-handle. Same "do not touch" list as
  CONS-01. Design call already made (Q3/Q4/Q9/Q10) — no user re-consult.

Design resolved in `features/console-autocomplete/` (Q1–Q11, 2026-09-07). Detail:
`docs/todo/CONS-01-*.md` / `CONS-02-*.md`; execution guidance: `docs/instruction/CONS-01-*.md` /
`CONS-02-*.md`.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| CONS-01 | Engine Log command entry: AutoComplete (popover + ghost-text) | — | — | ✅ Done (branch `cons-01/console-command-autocomplete`) |
| CONS-02 | Engine Log command entry: AutoCorrect (missing `!`, case, did-you-mean) | CONS-01 | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–15).

**Lessons carried in from Sprint 15:**

- Keep divergent copies of the same mechanism in sync. `scrollMoveLogToEnd()` vs
  `scrollEngineLogToBottom()` drifted at UI-14 and cost UI-15 to reconcile. CONS-01/02 both edit
  `bottom_panel.cpp` and add to one new `command_completer` TU — land the shared pieces once,
  from one place (R1), and don't fork a second command-name list.
- A green guard can still be a false positive (`port02-style-css-bundled` passed for a release
  while broken). The CONS pure-completer tests must assert on real behaviour — the `!`-only
  activation predicate, actual LCP output, actual "did you mean" hits — not just "function exists".

See `docs/sprint/burndown.md` for the daily remaining-items table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
