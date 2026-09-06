# Instruction — CONS-01: Engine Log command AutoComplete

## Approach

Feature, not a bug fix. `features/console-autocomplete/` Q1–Q11 are **resolved** (2026-09-07,
see planning.md Resolution table) — do not re-open them with the user.

Build order:

1. **`CommandDispatcher::registeredNames()` + `commandUsage(name)`** first — trivial, read-only,
   iterate `specs_` (it already holds `group`/`name`/`usage`/`summary`). No change to
   `executeLine()`, `handlers_`, or the routing. This is the seam everything else hangs off.
2. **`src/command/command_completer.{h,cpp}`** — pure. Could be header-only like
   `src/ui/engine_log_model.h`; match that file's "no GTK type, unit-testable with no display
   server" comment style. Functions: `matches(prefix, names)`, `longestCommonPrefix(matches)`,
   and a `shouldSuggest(entryText, caretPos)` predicate (trimmed text starts with `!`/`！` and
   caret is within the first token). Case-insensitive compare; return sorted.
3. **UI glue in `bottom_panel.cpp`** — the popover + ghost-text. Keep all GTK here.
4. Tests last-but-verified-throughout: `tests/test_cons01_command_completer.cpp` (pure).

## Pitfalls

- **`Gtk::EntryCompletion` is gone in GTK4.** Don't reach for it. The popover is a plain
  `Gtk::Popover` with a `Gtk::ListView`/`Gtk::Box` of rows, `set_parent(commandEntry_)` and
  positioned with `set_pointing_to` on the entry's allocation. Ghost-text: either a second
  dim `Gtk::Label` overlaid after the entry's text, or (simpler) render it via the entry's
  `Pango::Layout` — spike both, pick the one that doesn't fight IME preedit.
- **The existing key controller already owns Up/Down/Tab semantics.** Extend the *same*
  `EventControllerKey` lambda at [bottom_panel.cpp:124-153](../../src/ui/bottom_panel.cpp#L124-L153);
  don't add a second controller (event ordering between two key controllers on one widget is
  a trap). Branch on "popover visible?" at the top: visible → Up/Down/Enter/Esc drive the
  popover and `return true`; not visible → fall through to today's history logic untouched.
- **Return `true` only when you actually consumed the key.** Returning `true` for Tab when
  there's no completion candidate will swallow focus-traversal and annoy.
- **Re-query `registeredNames()` on each popover-open, never cache.** `.ptc` commands are
  re-synced on every engine start/reload (`syncExtensionCommands()` is idempotent) — a cached
  snapshot goes stale (HC4). Cheap to re-pull; it's a vector of strings.
- **`notify::text` fires for programmatic `set_text` too** (Tab-complete, history nav). Guard
  the suggestion refresh with a re-entrancy flag or you'll recompute/re-show the popover from
  inside your own completion write.
- **Don't regress UI-05.** The command entry is a sibling of the gutter+TextView row, not
  part of it. Nothing here touches `engineLogView_`, the gutter, or `flushPending()`.
- **`shouldSuggest` must be false for raw lines.** A user typing a raw `YXBOARD` must see no
  popover and keep Tab/Enter (R2/A6). Test this explicitly.

## Do not touch

- `CommandDispatcher::executeLine()` routing, the dangerous-external-command block, pos-session.
- `builtinCommandNames()` in `protocol_extension.cpp` — that's the PROTO-03 loader's copy;
  adding a third name list is the exact thing R1 forbids.
- The RT-01/RT-02 flush cadence, the UI-05 gutter, the UI-10/UI-15 sticky-scroll logic.
- Argument-level completion — Q6 killed it. `usage` string as a static hint only.

## Testing

- Pure: `matches("an", {...})`, `longestCommonPrefix`, `shouldSuggest` truth table
  (`!an`|caret@3 → true; `an` → false; `!analyze foo`|caret@10 → false, caret past first token;
  `  !a` leading ws → true).
- Dispatcher: `registeredNames()` contains every `registerBuiltins()` name; grows after a
  `syncExtensionCommands()` with a stub extension table.
- UI (only if drivable headless, mirror `tests/test_anlz05_no_automove_action.cpp`): type into
  a real `BottomPanel`'s entry, assert popover model contents + that Tab rewrites the text.
  If it needs a display server, say so in the summary and ship the pure coverage.
