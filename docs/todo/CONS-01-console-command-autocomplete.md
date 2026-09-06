# CONS-01 — Engine Log command entry: AutoComplete (command suggestion)

**Status:** ✅ DONE (2026-09-07, branch `cons-01/console-command-autocomplete` — not merged;
orchestrator drives the PR). Filed 2026-09-07, pulled into Sprint 16 the same day.

**Summary:** `CommandDispatcher::registeredNames()` + `commandUsage(name)` added as read-only
accessors over the existing `specs_` (no change to `executeLine()` routing, `handlers_`, the
dangerous-raw-command block, or pos-session). New GTK-free header `src/command/command_completer.h`
(header-only, mirroring `src/ui/engine_log_model.h` per the instruction — no `.cpp`): `matches()`
(case-insensitive, sorted, de-duped), `longestCommonPrefix()`, `shouldSuggest(text, caret)`
(trimmed text starts with `!`/`！` and caret within the first token), `currentPrefix()`. UI glue is
confined to `src/ui/bottom_panel.cpp`: a custom `Gtk::Popover` (`set_autohide(false)` so key events
keep reaching the entry, `≤ 8` `!name   usage` rows, `Gtk::ListBox` selection) anchored under
`commandEntry_`, plus an inline dim ghost-text `Gtk::Label` in a `Gtk::Overlay` over the entry.
Refresh runs on `notify::text` (`property_text().signal_changed()`), guarded by `suppressSuggest_`
against re-entry from our own `set_text`. The registry is re-queried through a
`std::function` provider on every refresh — never cached (HC4). The existing `EventControllerKey`
lambda is extended (not duplicated): `suggestOpen_ && handleSuggestionKey(keyval)` at the top —
popover open → Tab (complete-to-LCP then cycle) / Up / Down / Enter (accept into entry, no submit) /
Esc (hide, text unchanged) consume the key and return `true`; popover closed → falls through to the
unchanged `commandHistory_` logic byte-for-byte. `MainWindow` wires the two providers to the
dispatcher. No argument-level completion (Q6), no AutoCorrect (CONS-02), no `Gtk::EntryCompletion`,
no libadwaita, `builtinCommandNames()` in `protocol_extension.cpp` untouched (R1).

**Verification (2026-09-07):**
- `./build.sh` — clean, no new warnings.
- `ctest` (build_cmd) — 4/4 green: `ranls-gui-tests`, `ranls-gui-ui-tests`,
  `rel02-version-single-source`, `port02-style-css-bundled`.
- `tests/test_cons01_command_completer.cpp` (pure, no GTK, in `ranls-gui-tests`) — pins `matches`
  (`"an"` → `{"analyze"}`, sort, case-insensitivity), `longestCommonPrefix` real output, the
  `shouldSuggest` truth table (`!an`@3 → true; `an` → false; `!analyze foo`@10 → false; `  !a` → true;
  raw `YXBOARD` → false; fullwidth `！`), and `currentPrefix`.
- `tests/test_cons01_command_autocomplete.cpp` (in `ranls-gui-ui-tests`, self-skips headless) —
  `registeredNames()` contains every `protoext::builtinCommandNames()` entry and is sorted;
  `commandUsage` exact strings; `syncExtensionCommands()` with no engine is a quiet no-op (R5);
  after `startEngine()` with a stub `.ptc` on disk, `registeredNames()` grows to include the
  extension command. BottomPanel driven against a real widget: `!an` opens the popover with model
  `{"analyze"}`, Tab rewrites the entry to `!analyze `; a raw line opens nothing and the key
  handler is inert; Esc leaves `!an` in place; Enter accepts into the entry without emitting
  `signal_command_sent`.


Feature A of `features/console-autocomplete/` (design draft + open questions resolved
2026-09-07 — see [planning.md](../../features/console-autocomplete/planning.md) Resolution table).
Feature B (AutoCorrect) is **CONS-02**. This item carries the shared, GTK-free
`CommandCompleter` module + `CommandDispatcher::registeredNames()`; CONS-02 depends on both.

## Source

User request 2026-09-07: *"Scope: Engine Log, User Input. Support AutoComplete command
(suggestion) and AutoCorrect text typing on syntax."* Worked through
`features/console-autocomplete/`; Q1–Q11 resolved with the user the same day.

## Problem

The Engine Log command entry ([src/ui/bottom_panel.cpp:109-156](../../src/ui/bottom_panel.cpp#L109-L156))
has no completion. A user must remember the exact `!` command name or run `!help` and read
it back in the log. Built-in commands (`registerBuiltins()`) and `.ptc` extension commands
(`syncExtensionCommands()`) share one `!` namespace but there is no way to discover them at
the point of typing.

## Scope

1. **`CommandDispatcher::registeredNames()`** — new **read-only** accessor returning every
   currently-registered command name (built-ins + currently-synced `.ptc` extensions), and
   `commandUsage(name)` / reuse of `CommandSpec::usage` for the arg hint. No change to
   `executeLine()` routing (HC3). Re-queried each time the suggestion UI opens so a
   post-`syncExtensionCommands()` set is picked up (HC4).
2. **`src/command/command_completer.{h,cpp}`** — pure, no GTK type (mirror the
   "usable from a pure-logic unit test, no display server" note on `engine_log_model.h`):
   - `matches(prefix, names)` → names sharing the prefix, case-insensitive, sorted.
   - `longestCommonPrefix(matches)` → for Tab-to-complete.
   - (no arg-level completion — Q6.)
3. **Suggestion UI in `BottomPanel`** — Q1 says **both**:
   - a `Gtk::Popover` anchored under `commandEntry_`, ≤ 8 rows of `name — summary`,
     keyboard-navigable;
   - inline grey **ghost-text** showing the completion of the current token.
   Activates only when the trimmed entry text starts with `!` / `！` and the caret is in the
   first token (R2). Live on `notify::text` (Q2).
4. **Key handling** (extends the existing `EventControllerKey` at
   [bottom_panel.cpp:124-153](../../src/ui/bottom_panel.cpp#L124-L153)):
   - **Tab** → complete current token to `longestCommonPrefix`; Tab again → cycle candidates.
   - **Esc** → hide popup, entry text unchanged (A5/R5).
   - **Up/Down** → move popover selection *while the popover is open*; **Enter** accepts the
     selection into the entry (does **not** submit). Popover closed → Up/Down stay history
     navigation exactly as today (HC2/Q8).
   - **Enter** with popover closed → submit unchanged (R3).
5. Once a command name is complete, show its `usage` string as the arg hint (A3).

## Acceptance criteria

- Typing `!an` shows a popover with `analyze` (+ any `.ptc` match) and a ghost-text
  completion; Tab fills `!analyze `.
- A loaded `.ptc` file's commands appear in the same popover after the engine starts /
  reloads, without restarting the app.
- Engine never started → built-in names still complete (`registeredNames()` non-empty from
  `registerBuiltins()`), no error (R5).
- Raw protocol line (no `!`) → no popover, no ghost-text, Tab/Enter behave as today (R2/A6).
- Popover closed → Up/Down still navigate `commandHistory_` byte-for-byte as before (HC2).
- Esc with the popover open leaves the half-typed text in the entry (A5).
- `./build.sh` clean; `ctest` both suites green; `tests/test_cons01_command_completer.cpp`
  pins `matches` / `longestCommonPrefix` / the `!`-only activation predicate (pure, no GTK).
  UI wiring gets a `ranls-gui-ui-tests` case if it can be driven without a display (mirror
  the ANLZ-05 UI test pattern); if not, say so explicitly and cover the pure layer only.

## Scope boundary

- **No argument-level completion** (`!rule <TAB>` → `freestyle/standard/renju`, coord args).
  Q6: command-name completion + the `usage` hint is the whole feature. Do not add an
  `ArgSpec`-driven arg completer or file a follow-up for one.
- **No AutoCorrect** — missing `!`, wrong case, "did you mean" are all **CONS-02**.
- No change to `CommandDispatcher::executeLine()` routing, the dangerous-raw-command block,
  or the pos-session capture (HC3).
- No new engine traffic (R4) — pure local string work.
- No second hardcoded command list — `registeredNames()` is the single source (R1); do not
  touch `builtinCommandNames()` in `protocol_extension.cpp` (that copy is the loader's, Q3 of
  PROTO-03).
- No libadwaita; `Gtk::EntryCompletion` does not exist in GTK4 — custom popover + ghost-text
  only (HC1).
