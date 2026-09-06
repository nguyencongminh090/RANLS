# CONS-01: Engine Log command entry — AutoComplete (command suggestion)

**Status:** ✅ DONE

Feature work (not a bug fix), recorded here because CLAUDE.md's tracking convention routes every
tracked `TODO.md` `CODE` through the fix-log on completion. Design questions Q1–Q11 were resolved
with the user 2026-09-07 (`features/console-autocomplete/planning.md` Resolution table) before any
code.

## Prompt

Implement tracked task CONS-01 end-to-end on a branch: `CommandDispatcher::registeredNames()` +
`commandUsage(name)`; a pure GTK-free `command_completer` module; and the popover + ghost-text UI
glue in `bottom_panel.cpp`, extending the one existing `EventControllerKey` lambda. No
argument-level completion, no AutoCorrect (CONS-02), no `Gtk::EntryCompletion`, no second command
list. Do not push / open a PR / touch `main`.

## Action

- **`src/command/command_dispatcher.{h,cpp}`** — added `registeredNames() const` (every `specs_`
  name, sorted + de-duped) and `commandUsage(const std::string&) const` (the spec's `usage`, or
  `""`). Read-only; `executeLine()` routing, `handlers_`, the dangerous-external-command block and
  the pos-session capture are untouched.
- **`src/command/command_completer.h`** — new, header-only (mirrors `src/ui/engine_log_model.h`'s
  "pure logic, no GTK type, unit-testable with no display server" style; the instruction file
  explicitly permits header-only over a `.cpp`). `namespace command_completer`: `matches(prefix,
  names)` (case-insensitive, sorted, unique), `longestCommonPrefix(matches)` (case-insensitive
  compare, keeps first element's casing), `shouldSuggest(text, caretPos)` (trimmed text starts with
  `!` or fullwidth `！`, caret within the first whitespace-delimited token), `currentPrefix(text)`
  (the fragment after the bang), plus `startsWithBang` / `fullwidthBang` helpers reused by the UI.
- **`src/ui/bottom_panel.{h,cpp}`** — all GTK confined here:
  - `Gtk::Overlay` wrapping `commandEntry_` with a dim, non-targetable `Gtk::Label` for inline
    ghost-text; margin-start measured from a `Pango::Layout` of the current entry text.
  - `Gtk::Popover` (`set_parent(commandEntry_)`, `set_autohide(false)` so the entry keeps receiving
    keys, `set_has_arrow(false)`, `TOP`) holding a `Gtk::ListBox` of `≤ 8` `!name   usage` rows.
  - `refreshSuggestions()` on `commandEntry_.property_text().signal_changed()`, guarded by
    `suppressSuggest_` against re-entry from our own `set_text` (Tab-complete / history nav /
    accept). The command-name set comes from a `std::function` provider, re-queried every refresh —
    never cached (HC4).
  - The existing key-controller lambda gained one top branch: `if (suggestOpen_ &&
    handleSuggestionKey(keyval)) return true;`. `handleSuggestionKey` consumes Tab
    (complete-to-LCP, then cycle), Up/Down (move selection), Enter (drop selection into the entry
    with a trailing space — **does not** submit), Esc (hide, entry text unchanged) and returns
    `true` only for those. Popover closed → the branch is inert and the `commandHistory_` Up/Down
    logic below runs byte-for-byte as before (HC2).
  - `~BottomPanel()` added solely to `unparent()` the popover before `commandEntry_` (a
    later-declared member) is destroyed.
- **`src/main_window.cpp`** — wires `setCommandNameProvider` / `setCommandUsageProvider` to
  `commandDispatcher_->registeredNames()` / `commandUsage()` right after the dispatcher is built.
- **Tests** — `tests/test_cons01_command_completer.cpp` (pure, `ranls-gui-tests`) and
  `tests/test_cons01_command_autocomplete.cpp` (dispatcher accessors + real-widget BottomPanel
  drive, `ranls-gui-ui-tests`, self-skips headless). Both registered in `tests/CMakeLists.txt`.

Out of scope, deliberately not done: argument-level completion (Q6), AutoCorrect / missing-bang /
"did you mean" (CONS-02), any change to `builtinCommandNames()` in `protocol_extension.cpp` (R1),
`Gtk::EntryCompletion` (gone in GTK4), libadwaita.

## Verification

- `./build.sh` — clean, no new warnings.
- `ctest` (`build_cmd`) — 4/4 pass: `ranls-gui-tests`, `ranls-gui-ui-tests`,
  `rel02-version-single-source`, `port02-style-css-bundled`.
- Pure suite pins `matches` / `longestCommonPrefix` / `shouldSuggest` truth table / `currentPrefix`
  with real expected values (not "function exists"). Dispatcher suite pins `registeredNames()` ⊇
  every built-in + sorted, exact `commandUsage` strings, quiet no-op `syncExtensionCommands()` with
  no engine (R5), and growth to include a stub `.ptc` command after `startEngine()`. Widget suite:
  `!an` → popover model `{"analyze"}` + Tab rewrites entry to `!analyze `; raw `YXBOARD` → no
  popover, key handler inert; Esc keeps `!an`; Enter accepts without emitting `signal_command_sent`.

Manual live-engine / display smoke (popover placement, ghost-text alignment, IME preedit) not run —
no engine binary / display server on the build host; covered structurally by the widget test.

Branch `cons-01/console-command-autocomplete` — not merged (orchestrator drives the PR).
