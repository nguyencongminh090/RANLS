# Instruction — PROTO-03: Open Protocol Extension (`.ptc` user-defined commands)

Read `docs/todo/PROTO-03-open-protocol-extension.md` first (scope, acceptance criteria, scope
boundary), then `features/protocol-extension/` in full — `user_story.md` (Overview + Rules + Hard
constraints), `planning.md` (Q1–Q9, all resolved 2026-09-06 — read the "Why this one" column before
deviating from any of them), and `examples/yxAnalyzeOne.ptc` (a worked, non-loadable illustration of
the target schema). This is a **feature**, not a bug fix — no `systematic-debugging` pass needed —
but every design choice below was already argued through with the user; do not re-litigate Q1–Q9
without a concrete reason, and if one comes up, note the deviation in the fix-log/PR description.

## Build order (follow planning.md's 8 steps — dependencies run in this direction)

1. **`.ptc` schema + loader** — new `src/engine/protocol_extension.{h,cpp}`. Pure data structures
   (command table: `name`, `group`, `args`, `send` variant, `on_reply` list) + a TOML parse pass +
   validation. No `EngineProcess`/`EngineController`/GTK dependency — unit-testable standalone, like
   `GomocupProtocol`'s own parsing already is. Enforce Q5's limits and Q3's fail-closed
   name-collision check (against `CommandDispatcher`'s **full built-in registry** — `analyze`,
   `play`, `stop`, `getpos`, `loadpos`, `new`, `pos`, `redo`, `rule`, `start`, `undo`, `info`, `db`,
   `clear`, `engine`, `send`, `about`, `help` — not the Gomocup wire-keyword set, that was the wrong
   list per Q3's rationale) at load time. A `send` block's `repeat_source` may be either a declared
   `repeat(...)` arg name or the literal `$currentPath` — the loader only validates the *shape*
   here; `$currentPath` itself is resolved later at send time (step 4), since it needs a live
   `GameState`, which this file must not depend on.
2. **Mini-DSL interpreter** — same file or a sibling. Grammar is exactly Q2's: `if <expr> then
   <stmt>* (elif ... then ...)* (else ...)? end`, expressions are comparisons (`== != < > <= >=`)
   combined with `and`/`or`/`not` only. No arithmetic, no loops, no variables beyond the typed
   fields a matching `on_reply.pattern` captured. The only callable "functions" are the 3
   whitelisted sinks (`set_status_field`, `toast`, `log`) — implement them as a fixed dispatch
   table (a `map<string, SinkFn>` is fine; do not hand-roll a switch that would need editing per
   sink — small point, but keeps this extensible later without reopening the interpreter). Bound
   the interpreter itself (depth, statement count) as defense-in-depth even though the grammar has
   no loops — a `.ptc` is semi-trusted (Q5's rationale), not fully trusted.
3. **`GomocupProtocol` integration** — accept an optional loaded command table (e.g. constructor
   overload or a `setExtension(...)` call from `EngineController`). In `parseLine()`, **try
   built-in parsing first** (unchanged — do not reorder or interleave); only on no match, try each
   loaded `on_reply.pattern` in declaration order. This is the one point where PROTO-01's hardening
   and this feature meet — do not weaken PROTO-01's bounds checks to make room for this.
4. **`ICustomCommandSource`** — a new, minimal interface (not a method added to `IEngineProtocol`
   — see the Hard Constraints in user_story.md). One method is enough: something like
   `std::vector<std::string> generateCustom(const std::string& name, const std::vector<std::string>&
   args, const GameState& gameState)` — the `GameState` reference is what lets `$currentPath`
   resolve at send time without `GomocupProtocol` needing to store a `GameState&` itself.
   `GomocupProtocol` implements it only when a table is loaded (`dynamic_cast` from
   `EngineController` returns null otherwise — that null path is exactly how a `.ptc`-less run stays
   byte-identical to today). Add `EngineController::sendCustomCommand(name, args)`.
5. **`CommandDispatcher` wiring** — after `EngineController` loads its `.ptc` (i.e. after
   `startEngine()` succeeds), enumerate the loaded command names and `registerCommand()` each into
   the **same registry** built-ins use (Q7 — not a separate `!ext` namespace). `!help` needs a new
   `[extension]` group in its grouped output. Every extension command's `group` field (Q9) is
   **informational/classification only in v1** — do not wire it to any `EngineState` transition;
   every extension command, regardless of declared `group`, behaves like today's instant commands
   (`!getpos`, `!db query`, ...): send, wait for one `on_reply` match, done. This is the load-bearing
   scope cut — see "Do not" below.
6. **UI wiring** — new `EngineController::signal_custom_action` (a tagged struct: which sink, its
   args) connected in `main_window.cpp`. Three destinations: (a) `EngineStatusView` gets one new
   small dynamic container — a name→`Gtk::Label` pair map, adding a pair the first time a name is
   seen (do **not** try to reuse/repurpose the 6 fixed `labelDepth_`-style members — they stay
   exactly as-is); (b) `toast()` reuses the existing crash-banner mechanism
   (`AnalysisPanel::showEngineCrashBanner()` is the precedent — no `Adw::Toast`, this build has no
   libadwaita); (c) `log()` passes straight through to `EngineLogModel` like any other Engine Log
   line. No `BoardRenderer`/`BoardViewModel` touched anywhere in this feature.
7. **`EngineConfig` + `SettingsDialog`** — `std::string protocolExtensionPath` (empty = none),
   persisted via `SettingsStorage` like other `EngineConfig` fields (watch the STATE-02
   "save() rewrites the whole file" hazard — pass every config block through, same as every other
   config feature here has had to). File-chooser row filtered to `*.ptc`. Takes effect only via
   `startEngine()`/`reloadEngine()` — no file-watching, no live reload (Q6).
8. **Regression tests** — new `tests/test_proto03_*.cpp` in the existing header-only harness
   (`ranls-gui-tests`, no display server, matching how `GomocupProtocol`/PROTO-01/PROTO-02 are
   tested today). Minimum cases: valid `.ptc` loads and its command is registered; a name collision
   against a built-in is rejected with the file not loaded at all; each Q5 limit individually
   rejects when exceeded; a `send` single-line and a `send` open/repeat/close block (both with a
   `repeat(...)` arg and with `$currentPath`) produce the exact expected line sequence, including
   `i.color` alternation matching `generateAnalyzeRequest`'s own formula; a well-formed `on_reply`
   fires the right sink with the right args; a type-mismatched reply line is skipped (Debug log, no
   crash) rather than crashing the interpreter.

## Do not

- Add any method to `IEngineProtocol` for this feature — the custom-command send path is
  `ICustomCommandSource`, reached via `dynamic_cast`, precisely so the core protocol interface
  stays untouched (hard constraint, user_story.md).
- Wire `group` to `EngineState`/`SearchIntent`/`pendingStopFlush_`/Stop in any way. **v1 has no
  `group = "analysis"` support at all** — every extension command is an instant fire-and-forget
  request regardless of its declared group. This was an explicit scope cut (Q9) after tracing
  `EngineController::analyze()`'s reliance on a bare-coordinate `signal_move` as its only
  search-completion signal — do not "just add a flag" to bypass this; it needs its own design pass
  (see planning.md's Follow-ups) if ever taken on.
- Add `highlight_cell`/`clear_highlights` or any other board-drawing sink. Dropped from v1
  deliberately (`BoardViewModel`/`BoardRenderer` would need new fields/layers for no concrete need
  beyond what `set_status_field` already covers).
- Reorder or weaken PROTO-01's bounds-checked parsing in `GomocupProtocol::parseLine()` — built-in
  parsing must still run, and win, before any extension pattern is tried.
- Add arithmetic, loops, or free-variable access to the DSL. If a real need for arithmetic surfaces
  during implementation (e.g. to make some example work), stop and raise it with the user rather
  than quietly extending Q2's grammar — it was a deliberate trust-boundary decision.
- Implement hot-reload / file-watching for `.ptc` (Q6) or multi-line `on_reply` accumulation (Q8) —
  both explicitly deferred.
- Let `$currentPath` resolution or any other `.ptc`-triggered code path touch `GameState` mutably —
  it is read-only context (Q9's rationale: host-trusted data, not a new mutation surface).

## Tests

See step 8 above for the minimum case list. Follow this codebase's existing pattern of testing the
protocol layer standalone (no GTK, no real subprocess) — `tests/test_rt01_throttle.cpp` /
`tests/test_ui04_pv_reset.cpp` and PROTO-01/PROTO-02's own tests are the closest precedent for
harness shape. `CommandDispatcher` registration (step 5) can likely reuse whatever fake-`CommandContext`
scaffolding existing dispatcher tests already have.

## Manual smoke (needs a human — engine binary + display)

1. No `.ptc` configured: `!help` output and all built-in behavior byte-identical to before this
   feature existed.
2. Load `examples/yxAnalyzeOne.ptc`-equivalent (adapted to a real engine's actual custom command, if
   one exists) via Settings; start the engine; run `!yxAnalyzeOne <x> <y>` from the console — the
   declared sink(s) fire correctly on the real reply line.
3. A deliberately broken `.ptc` (name collision with `!play`, or an `if` nested 5+ deep) is rejected
   at engine start with a clear, visible error — engine still starts with only built-in behavior.
4. Confirm Stop/analysis/Analyze Mode are completely unaffected by having a `.ptc` loaded — this is
   the "no `group = "analysis"` integration" scope cut showing up as "nothing changed" in practice.
