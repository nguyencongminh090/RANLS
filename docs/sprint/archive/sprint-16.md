# Sprint 16 (closed 2026-09-07)

**Goal:** Engine Log command console: autocomplete + autocorrect
**Dates:** 2026-09-07 to 2026-09-07.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| CONS-01 | Engine Log command entry: AutoComplete (command suggestion — popover + ghost-text) | ✅ DONE |
| CONS-02 | Engine Log command entry: AutoCorrect (missing `!`, wrong case, "did you mean") | ✅ DONE |

Both items were committed at sprint open (pulled from Backlog, filed the same day from the
`features/console-autocomplete/` design — Q1–Q11 resolved with the user 2026-09-07). No mid-sprint
pulls. CONS-02 depended on CONS-01 for the shared `command_completer` TU + `registeredNames()`;
CONS-01 shipped first so those seams landed with it. Points not estimated (consistent with
Sprints 3–15). Single-day sprint.

## What shipped

- **CONS-01** (PR #30 squash `04f70f6`): Engine Log command-entry AutoComplete. New read-only
  `CommandDispatcher::registeredNames()` / `commandUsage(name)` over the existing `specs_` (no
  `executeLine()` routing / `handlers_` / dangerous-command-block change). New header-only,
  GTK-free `src/command/command_completer.h` (`matches`, `longestCommonPrefix`, `shouldSuggest`,
  `currentPrefix` + bang helpers), mirroring `engine_log_model.h`'s "unit-testable with no display
  server" style. UI glue confined to `src/ui/bottom_panel.{h,cpp}`: a custom `Gtk::Popover`
  (`set_autohide(false)`, ≤ 8 `!name   usage` rows) under the entry plus inline dim ghost-text in a
  `Gtk::Overlay`; the one existing `EventControllerKey` lambda gained a single top branch
  (`suggestOpen_ && handleSuggestionKey` — Tab complete-to-LCP then cycle / Up / Down / Enter
  accept-not-submit / Esc), leaving the `commandHistory_` Up/Down path byte-for-byte unchanged when
  the popover is closed. `builtinCommandNames()` untouched (R1 — no second command list). No
  argument-level completion (Q6). Tests: `test_cons01_command_completer.cpp` (pure) +
  `test_cons01_command_autocomplete.cpp` (dispatcher accessors + real-widget drive, self-skips
  headless).

- **CONS-02** (PR #31 squash `8f90303`): Engine Log command-entry AutoCorrect, building on
  CONS-01's seams. Pure `command_completer` gained `levenshtein`, `normalizeCase` (registered
  spelling only when case differs — B2), `didYouMean` (Levenshtein ≤ 2 **and** ≤ ⌊len/2⌋,
  best-first, ≤ 3, exact never suggested — B4), `missingBangFix` (**exact** case-insensitive
  first-token match only, never fuzzy — B1/B3), `autoCorrect` (composes the two, one rewrite max).
  `CommandDispatcher` gained read-only `nearestNames(token)` + a case-insensitive fallback in the
  `handlers_` lookup inside `executeLine()` (Q9 — key normalised, routing untouched); the bare
  "Unknown internal command" branch now appends `Did you mean: !x?` when `nearestNames()` is
  non-empty. `BottomPanel`'s `signal_activate` handler runs `autoCorrect` pre-dispatch — on a
  change it rewrites the entry visibly (one Ctrl+Z step), appends one `MESSAGE`-tag `corrected: …`
  line, then dispatches the corrected string; fullwidth `！` and non-matching raw lines pass
  through. `!` routing / dangerous-command block / `builtinCommandNames()` untouched.
  **Also fixed a latent `~BottomPanel()` UAF** the headless test exposed: the RT-02 batch-flush
  timeout (`flushTimerConn_`, connected with `sigc::mem_fun`, which does not track lifetime) was
  never disconnected, so a destroyed panel's 50 ms timer fired `flushPending()` on freed memory on
  the next main-loop iteration — harmless for the single app-lifetime panel, but the CONS-02 tests
  build and tear down several. Now `flushTimerConn_.disconnect()` in the destructor (same class as
  the PORT-03 `sigc::track_obj` scroll-idle fix). Tests: `test_cons02_autocorrect.cpp` (pure) +
  `test_cons02_autocorrect_dispatch.cpp` (`nearestNames`, case-insensitive `!ANALYZE` routing, B4
  text, real-widget submit path). CONS-02's dispatched subagent was interrupted twice by process
  exits; the orchestrator finished verification + PR from its worktree.

Both PRs re-verified independently by the orchestrator on-branch before merge: clean `./build.sh`,
`ctest` 4/4 (`port02-style-css-bundled`, `ranls-gui-tests`, `rel02-version-single-source`,
`ranls-gui-ui-tests`). CONS-02's final numbers: `ranls-gui-tests` 218 cases / 2532 assertions,
`ranls-gui-ui-tests` all green with 0 crashes.

## Lessons

- **A `by-value` accessor + a reference binding is a dangling reference.** CONS-02's first UI-test
  crash was `const auto &line = p.pending().back();` where `pending()` returns
  `std::vector<EngineLogLine>` by value — `line` outlived the temporary. It surfaced as a memcpy
  SIGSEGV deep inside doctest's stringifier, not at the bind site. When a headless widget test
  crashes in the harness rather than in product code, suspect a lifetime bug in the test glue
  first.
- **`sigc::mem_fun` on a recurring `Glib::signal_timeout`/`signal_idle` needs `sigc::track_obj`
  (or an explicit `disconnect()` in the destructor).** CONS-02 found the third instance of this in
  `BottomPanel` (`flushTimerConn_`), after PORT-03's four deferred scroll idles and the UI-12
  flake. Latent whenever the widget lives for the whole app run; a real UAF the moment anything
  builds and tears down more than one in a process — which the growing headless real-widget test
  suite now routinely does. Worth an audit sweep of every `signal_timeout`/`signal_idle` connect
  in `src/ui/` for the next `CLEAN` item.
- **Carried from Sprint 15 and still true:** keep divergent copies of the same mechanism in sync
  (CONS-01/02 correctly landed the shared `command_completer` pieces once, R1, no forked name
  list); a green guard can still be a false positive (the CONS pure tests assert real behaviour —
  actual LCP output, actual "did you mean" hits, the `!`-only predicate — not "function exists").
- **Carried from Sprint 15, unchanged:** the build host has no engine binary and no display for
  the interactive path, so the live-engine console smoke (popover placement, ghost-text
  alignment, IME composition, the visible `corrected:` rewrite in a running app) is still an
  outstanding **human** step for both CONS-01 and CONS-02.

## Rolled over to Backlog

Nothing rolled over — both committed items finished.

## Next sprint

Sprint 17 — run `/sprint open 17 "<goal>" <CODE...>` to commit its Backlog items. The Backlog is
currently empty; new work enters via `docs/notes/` → `features/<slug>/` → `docs/todo/`. Release
**`v0.5.0`** cut at this close (MINOR — CONS-01/02 are new user-visible features).
