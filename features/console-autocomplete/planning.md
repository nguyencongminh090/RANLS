# Console AutoComplete + AutoCorrect — planning

See [user_story.md](user_story.md) · [diagram/flow.md](diagram/flow.md).

## Status

Design draft 2026-09-07. **Open questions resolved with the user the same day** — see the
Resolution column. Formalised into `docs/todo/CONS-01` (AutoComplete) + `docs/todo/CONS-02`
(AutoCorrect) + matching `docs/instruction/` entries, both filed to `TODO.md` Backlog.

## Resolution (2026-09-07)

| # | Resolution |
|---|---|
| Q1 | **Both** — popover list *and* inline ghost-text completion. |
| Q2 | Default — live on every `notify::text` when trimmed text starts with `!`/`！` and the caret is in the first token; Tab also completes; Esc hides. |
| Q3 | Default — B1 (missing bang) auto-fixes on Enter *only* when the first token is an exact case-insensitive match to a registered name; visible rewrite then dispatch. |
| Q4 | Default — B4 "did you mean" is a log line only for v1, no auto-run. |
| Q5 | Default — Levenshtein ≤ 2 and ≤ ⌊len/2⌋; up to 3 names listed on a tie. |
| Q6 | **No argument-level completion.** Command-name completion + the `usage` string as the arg hint is enough. No follow-up task. |
| Q7 | Default — pure matcher in `src/command/command_completer.{h,cpp}` (no GTK), unit-tested; UI glue in `bottom_panel.cpp`. |
| Q8 | Default — popover open: Up/Down move selection, Enter accepts into entry (no submit), Esc closes. Popover closed: Up/Down are history, unchanged. Tab always completes-to-LCP then cycles. |
| Q9 | Default — visible rewrite in the completer/UI *and* case-insensitive dispatcher name lookup as defense-in-depth. |
| Q10 | Default — one `MESSAGE`-tag log line when a correction changed the input (`corrected: !analyze`). |
| Q11 | **Split by feature:** `CONS-01` = Feature A (AutoComplete), `CONS-02` = Feature B (AutoCorrect). The shared `CommandCompleter` module + `CommandDispatcher::registeredNames()` land with whichever ships first; the other then depends on it. |

## Context found in code

- Command entry + key controller: [src/ui/bottom_panel.cpp:109-156](../../src/ui/bottom_panel.cpp#L109-L156).
  Up/Down already bound to `commandHistory_` navigation; `signal_activate` pushes history and
  emits `signal_command_sent`.
- Dispatch: [src/main_window.cpp:582](../../src/main_window.cpp#L582) → `commandDispatcher_->executeLine(cmd)`.
- Registry: `CommandDispatcher::registerBuiltins()` + `syncExtensionCommands()`; `CommandSpec`
  carries `usage`/`summary`. There is **no** public accessor for the full name list today — needs
  adding (read-only, HC3).
- Missing-bang and unknown-command are currently hard errors in
  [src/command/command_dispatcher.cpp:100-166](../../src/command/command_dispatcher.cpp#L100-L166).
- `！` fullwidth bang already normalised in `executeLine()` — B3 is "don't regress", not new work.
- GTK4: `Gtk::EntryCompletion` does not exist. No libadwaita.

## Open questions

| # | Question | Proposed default (for user to accept/change) |
|---|---|---|
| Q1 | Suggestion UI form: `Gtk::Popover` list under the entry, or inline grey ghost-text completion, or both? | **Popover list** anchored to the entry, showing `name — summary`, max ~8 rows, keyboard-navigable. Ghost-text is a nice-to-have deferred to a follow-up. |
| Q2 | Trigger: every keystroke while input starts with `!`, or only on Tab? | **Live** on every `notify::text` when trimmed text starts with `!` and caret is in the first token; Tab also completes. Esc hides. |
| Q3 | Does AutoCorrect B1 (missing bang) fire automatically on Enter, or require accepting an inline hint first? | **Automatic on Enter** *only* when the first whitespace-delimited token is an exact (case-insensitive) match to a registered name; rewrite the entry text visibly, then dispatch. Otherwise unchanged (raw line). |
| Q4 | B4 "did you mean": show as a log line only, or an interactive accept? | **Log line only** for v1 (`ERR: Unknown internal command 'anlyze'. Did you mean !analyze?`). No auto-run. Interactive accept deferred. |
| Q5 | Edit-distance threshold + tie-breaking for B4? | Levenshtein ≤ 2, and ≤ ⌊len/2⌋; if 2+ names tie, list up to 3 ("Did you mean: !analyze, !about?"). |
| Q6 | Argument-level completion (e.g. `!rule ` → `freestyle/standard/renju`, coord args)? | **Out of scope for v1.** Command-name completion + `usage` hint only. File a Backlog follow-up (`CONS-02`) for arg completion driven by `ArgSpec` types. |
| Q7 | Where does the pure matcher live — `src/command/` or `src/ui/`? | New `src/command/command_completer.{h,cpp}` (or header-only like `engine_log_model.h`), pure, no GTK, unit-tested in `tests/`. UI glue stays in `bottom_panel.cpp`. |
| Q8 | Does the popover consume Up/Down (HC2) when open? | Yes: popover open → Up/Down move selection, Enter accepts selection into the entry (does not submit), Esc closes. Popover closed → Up/Down are history exactly as today. Tab always completes-to-LCP then cycles. |
| Q9 | Case normalisation (B2) — registered names are all lowercase today; is a case-insensitive `handlers_` lookup simpler than a UI rewrite? | Do the visible rewrite in the completer/UI (keeps `executeLine` untouched, HC3) **and** additionally make the dispatcher's name lookup case-insensitive as defense-in-depth. Confirm both. |
| Q10 | Should corrections be echoed to the Engine Log (not just the entry) for an audit trail? | The SEND line already logs the final dispatched text. Add one `MESSAGE`-tag line when a correction changed the input (`corrected: !analyze`). Confirm. |
| Q11 | One `TODO.md` CODE or split A / B? | One prefix `CONS`, split into `CONS-01` (AutoComplete, Feature A) and `CONS-01b`/`CONS-03` (AutoCorrect, Feature B) so they can ship independently. Completer module is shared and lands with whichever goes first. |

## Implementation sequencing (once questions resolved)

1. `CommandDispatcher::registeredNames()` + (Q9) case-insensitive lookup — small, testable.
2. Pure `CommandCompleter` (matches / LCP / didYouMean / normalizeCase) + `tests/test_cons01_*`.
3. `SuggestionPopover` widget + wire into `commandEntry_` key controller, preserving HC2 history nav.
4. AutoCorrect pre-dispatch hook in `BottomPanel::signal_command_sent` path (or in the activate
   handler before emit), with visible rewrite + Q10 echo.
5. Fix-log/audit only if it surfaces a bug; otherwise this is feature work → `docs/todo/` + PR.
