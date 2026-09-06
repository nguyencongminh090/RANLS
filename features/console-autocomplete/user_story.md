# Console AutoComplete + AutoCorrect — user stories

Scope: **Engine Log tab, user command input** (`BottomPanel::commandEntry_`,
[src/ui/bottom_panel.h:131](../../src/ui/bottom_panel.h#L131) / wiring in
[src/ui/bottom_panel.cpp:109-156](../../src/ui/bottom_panel.cpp#L109-L156)). Nothing outside the
command entry and its immediate feedback surface is in scope.

See also: [diagram/flow.md](diagram/flow.md) · [planning.md](planning.md).

## Actors

- **User** — types into the Engine Log command entry. Ranges from "knows every `!` command" to
  "saw `!help` once". Also pastes raw protocol lines.
- **CommandDispatcher** — owns the canonical command registry (built-ins in
  `registerBuiltins()`, `.ptc` extensions via `syncExtensionCommands()`), the `CommandSpec`
  metadata (`group`/`name`/`usage`/`summary`), and the `!`-prefix / raw-line routing rules in
  `executeLine()`.
- **BottomPanel** — owns the `Gtk::Entry`, its key controller, and command history.

## Feature A — AutoComplete (command suggestion)

- **A1** As a user, when I type the start of a command name into the entry, I want to see the
  matching commands so I don't have to remember exact spelling or run `!help`.
- **A2** As a user, I want to press **Tab** to complete the current token to the longest common
  prefix of the matches, and press Tab again to cycle through candidates.
- **A3** As a user, once a command name is complete, I want to see its `usage` string (argument
  hint) so I know what parameters follow.
- **A4** As a user, I want completion to cover **both** built-in commands and `.ptc` extension
  commands, since they share one `!` namespace.
- **A5** As a user, I want a way to dismiss the suggestion UI (Esc) without clearing what I typed.
- **A6** As a user typing a raw protocol line (no `!`), I want completion to stay out of my way —
  at most a quiet hint, never a popup stealing my Enter/Tab.

## Feature B — AutoCorrect (syntax fix-ups)

- **B1** As a user who typed a known command name **without** the leading `!`, I want it corrected
  to `!<name>` (this today is a hard error at submit — see `executeLine()` "This is an internal
  command" branch), either automatically on submit or via a one-key accept.
- **B2** As a user who typed a command in the wrong case (`!ANALYZE`, `!Rule`), I want it
  normalised to the registered spelling.
- **B3** As a user who used the fullwidth IME bang `！`, I want it silently accepted as `!` (this
  already works in `executeLine()` — AutoCorrect must not regress or double-handle it).
- **B4** As a user who made a near-miss typo on a command name (`!anlyze`, `!undoo`), I want a
  "did you mean `!analyze`?" suggestion rather than a bare "Unknown internal command".
- **B5** As a user, I want every automatic correction to be **visible** (the entry text updates
  before send, or an echoed note in the log) and **undoable** (Ctrl+Z / one keystroke), never a
  silent rewrite of my intent.

## Rules

- **R1 — Registry is the single source of truth.** The completion/correction candidate set is
  exactly what `CommandDispatcher` exposes (built-ins + currently-synced `.ptc` extensions). No
  second hardcoded list. `builtinCommandNames()` in
  [src/engine/protocol_extension.cpp:12](../../src/engine/protocol_extension.cpp#L12) already
  duplicates the built-in names once for the loader; this feature must not add a third copy.
- **R2 — Suggestion only for `!` lines.** Raw protocol lines are engine-specific and unbounded;
  we do not know the engine's grammar. AutoComplete activates only when the trimmed input begins
  with `!` / `！`. AutoCorrect's B1 (missing bang) is the sole exception and only fires when the
  first token exactly matches a registered name.
- **R3 — Never change submit semantics without consent.** Tab/Esc/arrows may be repurposed while
  the suggestion UI is open, but Enter must still submit the current entry text. A correction that
  rewrites the line happens *before* dispatch and the rewritten line is what gets logged as SEND.
- **R4 — No new engine traffic.** Completion and correction are pure string work over the local
  registry. Nothing queries the engine subprocess.
- **R5 — Degrade quietly.** Empty registry (engine never started) → built-ins still complete.
  No match → no popup, no error until submit.
- **R6 — Keep the model/UI split.** The candidate-matching + longest-common-prefix +
  did-you-mean logic is GUI-toolkit-independent and unit-testable without a display server
  (mirror `EngineLogModel`'s "pure logic, no GTK type" note). Only the popup/entry glue is in
  `src/ui/`.

## Hard constraints

- **HC1** GTK4 / gtkmm-4 only. The suggestion UI must not require GTK types unavailable in this
  build (no libadwaita). `Gtk::EntryCompletion` is **removed in GTK4** — a custom popover or
  inline ghost-text is required; this is an open question (planning Q1).
- **HC2** Command history navigation (Up/Down over `commandHistory_`, currently in the key
  controller at [src/ui/bottom_panel.cpp:124-153](../../src/ui/bottom_panel.cpp#L124-L153)) must
  keep working. If the suggestion popup is open, Up/Down may move the selection instead — but with
  the popup closed, Up/Down are history, unchanged.
- **HC3** No behavioural change to `CommandDispatcher::executeLine()` routing. AutoCorrect may
  pre-transform the string the UI passes to `executeLine`, but the dispatcher's own rules
  (`!` routing, dangerous-raw-command block, pos-session capture) are untouched. If B1/B4 want
  dispatcher-side help (e.g. a "nearest name" query), that is a new **read-only** method on the
  dispatcher, not a change to `executeLine`.
- **HC4** `.ptc` extension commands are re-synced on every engine start/reload; the completion
  candidate set must pick up the current set each time it opens, not cache a stale snapshot.
- **HC5** Doc-only until `planning.md` open questions are resolved with the user, then formalise
  into `docs/todo/` + `docs/instruction/` before any code.
