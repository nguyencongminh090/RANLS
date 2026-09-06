# PROTO-03 — Open Protocol Extension: user-defined `.ptc` commands for Gomocup-family engines

**Status:** ✅ DONE (2026-09-06, branch `proto-03/open-protocol-extension`). Design resolved with the
user 2026-09-05/2026-09-06 (`features/protocol-extension/planning.md` Q1–Q9 all accepted); Sprint 13.

**Implementation summary:**
- New standalone `src/engine/protocol_extension.{h,cpp}` — hand-rolled strict TOML-subset parser
  (chosen over vendoring toml++: the schema is narrow — top-level scalars, `[[command]]` /
  `[[command.on_reply]]` arrays-of-tables, string + string-array values, `"""` blocks — and a
  ~120-line strict parser keeps the trust boundary minimal and is fully covered by the
  malformed-input tests; no `third_party/` dir added). Fail-closed validation: Q3 collision against
  the full built-in registry (`protoext::builtinCommandNames()`, mirrors
  `CommandDispatcher::registerBuiltins` — 18 names), Q5 limits (if-depth ≤ 4, sink calls/action ≤ 16,
  commands/file ≤ 32, args/command ≤ 8, file ≤ 64 KB), `group` restricted to the 6 non-`analysis`
  kinds, `ptc_version = 1` / `extends = "gomocup"`.
- Mini-DSL: recursive-descent parser for exactly the Q2 grammar (`if/elif/else … end`, comparisons +
  `and`/`or`/`not`, no arithmetic/loops/free variables), 3 whitelisted sinks
  (`set_status_field`/`toast`/`log`) via a `map<string, fn>` dispatch table, bounded interpreter.
- `GomocupProtocol` now also implements the new minimal `ICustomCommandSource` (NOT a method on
  `IEngineProtocol`); `parseLine()` runs built-in parsing first and unchanged, extension `on_reply`
  patterns only on no match (PROTO-01 hardening untouched). Type-mismatched reply lines are skipped
  with a Debug log, never UB.
- `EngineController::loadExtensionTable()` (load-once at start/reload, Q6), `sendCustomCommand()`,
  `signal_custom_action`; `customSource_` stays null on a `.ptc`-less run.
- `CommandDispatcher::syncExtensionCommands()` registers each command into the same `!` registry as
  built-ins (Q7), `[extension]` help group; declared `group` is informational only (Q9).
- UI: `set_status_field` → new dynamic name→`Gtk::Label` container in `EngineStatusView` (6 fixed
  members untouched); `toast` → crash-banner widget (`AnalysisPanel::showInfoBanner`, no libadwaita);
  `log` → `EngineLogModel`. No `BoardRenderer` / `BoardViewModel` change.
- `EngineConfig::protocolExtensionPath` persisted via `SettingsStorage`; `*.ptc` file-chooser row in
  `SettingsDialog` (`onApply` merges from base config, STATE-02-safe).

**Verification:** `./build.sh` clean (no new warnings in touched files). `ctest`: `ranls-gui-tests`
209/209 cases (2466 assertions), including 14 new `test_proto03_*` cases covering valid load + command
registered, name-collision rejection, each Q5 limit individually, single-line + block `send`
(`repeat()` arg and `$currentPath`, `i.color` alternation), on_reply sink firing, and
type-mismatch-skip. The `rel02-version` script test passes. `ranls-gui-ui-tests` has one **pre-existing**
failure (`test_anlz05_no_automove_action`, needs a real/fake engine's timing — fails identically on
the branch base `8b010ed`); all other 25 UI cases pass. The live-engine + display "Manual smoke"
tier (instruction §"Manual smoke") still needs a human — no engine binary / display server on this host.

**Summary (design, 2026-09-06):** let an engine developer declare new console-triggered commands
in a `.ptc` (TOML) file, loaded by `GomocupProtocol` at runtime — no new `IEngineProtocol`
implementation, no YixinBoard rebuild. Each command declares a `group` (one of the console's
existing categories — `board`/`config`/`database`/`debug`/`engine`/`info`; `analysis`-kind real
search integration is explicitly out of scope, see below), typed `args`, a `send` (single line or
an open/repeat/close block mirroring the real `YXBOARD`/`BOARD ... DONE` pattern, repeat source
either a console `repeat(...)` arg or the live `$currentPath`), and one `on_reply` (single line,
typed captures, a bounded `if/elif/else` DSL — no loops/arithmetic/free variables — calling one of
three whitelisted sinks: `set_status_field`, `toast`, `log`). Full rationale for every choice below
is in the linked planning doc; this file is the formal backlog entry, not a restatement.

**Area:** new `src/engine/protocol_extension.{h,cpp}` (`.ptc` loader + DSL interpreter), a new
small `ICustomCommandSource` interface (`src/engine/`) implemented only by `GomocupProtocol` when
a `.ptc` is loaded, `EngineController::sendCustomCommand()` + a new `signal_custom_action`,
`CommandDispatcher` (register loaded command names into the existing `!`-namespace),
`EngineConfig`/`SettingsDialog` (`.ptc` path field), a small new dynamic-row addition to
`EngineStatusView` (arbitrary-named `set_status_field`). `IEngineProtocol`, `EngineProcess`, and
all existing Gomocup parsing/hardening (PROTO-01, PROTO-02) stay untouched — see Hard Constraints
in `features/protocol-extension/user_story.md`.

**Priority:** P3 (new capability, no reported bug/regression forcing it; no sprint pressure).

**Source:** User request 2026-09-05 ("open environment for open protocol" — let engine developers
add commands without a YixinBoard rebuild), refined over a multi-session design discussion through
2026-09-06.

**Design:** `features/protocol-extension/` (`user_story.md`, `diagram/flow.md`, `planning.md`,
`examples/yxAnalyzeOne.ptc` — a worked, non-loadable illustration of the finalized schema).

**Depends on / relates to:** `IEngineProtocol`/`GomocupProtocol` (`src/engine/i_engine_protocol.h`,
`gomocup_protocol.{h,cpp}`) — the protocol abstraction this feature extends without modifying;
PROTO-01 (trust-boundary precedent the DSL sandboxing follows); `CommandDispatcher`'s existing
`!`-namespace and `PosSession` `done`-terminated block convention (schema for `repeat(...)` args
mirrors it).

## Problem / motivation

Today, adding any new engine-specific command (e.g. a Rapfi/Yixin extension like
`YXANALYZEONE x,y` — analyze one user-chosen candidate move instead of searching for best move)
requires a C++ change to `GomocupProtocol`/`IEngineProtocol` and a full YixinBoard rebuild. This
blocks engine developers who want to expose custom directives through the GUI without depending on
YixinBoard's own release cycle.

## Scope (v1, per Q1–Q9)

1. **`.ptc` schema + loader** (`src/engine/protocol_extension.{h,cpp}`): `ptc_version`, `extends`,
   `[[command]]` (`name`, `group`, `args`, `send` — single-line or open/repeat/close block, repeat
   source = a `repeat(...)` arg or `$currentPath`), `[[command.on_reply]]` (`pattern`, `action`).
   Fail-closed validation at load time: name collision against the full `CommandDispatcher`
   built-in registry (Q3), load-time limits (Q5: if-depth ≤ 4, sink calls/action ≤ 16,
   commands/file ≤ 32, args/command ≤ 8, file ≤ 64 KB).
2. **DSL interpreter**: `if/elif/else` with comparisons + `and`/`or`/`not` only (Q2) — no loops, no
   arithmetic, no free variables outside typed `on_reply` captures. Dispatches to exactly 3
   whitelisted sinks (Q9-adjacent decision, revised 2026-09-06: `highlight_cell`/`clear_highlights`
   dropped from v1): `set_status_field(name, value)`, `toast(message)`, `log(message)`.
3. **`GomocupProtocol` integration**: optional extension table loaded at construction;
   `parseLine()` tries built-in parsing first, falls back to loaded `on_reply` patterns only on no
   match.
4. **`ICustomCommandSource`** (new, minimal interface) + `EngineController::sendCustomCommand()`
   reached via `dynamic_cast` — `IEngineProtocol` itself stays untouched (Interface Segregation).
5. **`CommandDispatcher` wiring**: register each loaded command name into the same `!`-prefixed
   namespace as built-ins (Q7); `!help` lists them under a new `[extension]` group.
6. **UI wiring**: `EngineController::signal_custom_action` → (a) a new small dynamic row container
   in `EngineStatusView` for arbitrary-named `set_status_field` (real new UI, but one widget, no
   `BoardRenderer`/`BoardViewModel` change), (b) the existing crash-banner pattern for `toast`, (c)
   `EngineLogModel` passthrough for `log`.
7. **`EngineConfig` + `SettingsDialog`**: `.ptc` path field, file-chooser row. Load-once at
   `startEngine()`/`reloadEngine()` — no hot-reload (Q6).
8. **Regression tests**: schema parsing (valid + malformed `.ptc`), DSL interpreter (bounds,
   type mismatches, sink whitelist), name-collision rejection, end-to-end fake-engine-line →
   sink-call assertions.

## Acceptance criteria

- With no `.ptc` loaded, the app behaves exactly as today (built-in Gomocup/Yixin behavior
  unchanged, no extension commands registered).
- A valid `.ptc` command (single-line `send`, e.g. `yxAnalyzeOne`) is invocable as `!yxAnalyzeOne
  <args>` from the console, and a matching `on_reply` correctly drives its declared sink(s).
- A block-`send` command (e.g. `yxDbSync` using `$currentPath`) emits the exact
  open/repeat/close line sequence the schema declares, with per-item `{i.x}`/`{i.y}`/`{i.color}`
  matching the real `generateAnalyzeRequest`/`generateMoveRequest` color-alternation formula.
- A malformed or malicious `.ptc` (name collision, over a Q5 limit, unparseable TOML) is rejected
  at load time with a clear error — the engine still starts with only its built-in behavior.
- A malformed/unexpected engine reply line (type mismatch against a declared `pattern`) is skipped
  with a Debug log line, never UB, matching PROTO-01's precedent.
- `./build.sh` clean; `ctest` green including the new regression tests.

## Scope boundary (explicit follow-ups, not part of this task)

- **`group = "analysis"` commands** — real search integration with `EngineState`/Stop/
  `pendingStopFlush_`/`SearchIntent`. Deferred whole (Q9) — touching this state machine is real
  surgery on PROTO-04/ANLZ-06 territory and was explicitly not accepted into v1.
- **Board-cell visualization sinks** (`highlight_cell`/`clear_highlights`) — would need a new
  `BoardViewModel` field + `BoardRenderer` draw layer; dropped 2026-09-06, no concrete use case
  needs it over `set_status_field`.
- **A brand-new, non-Gomocup protocol family via `.ptc`** — every engine YixinBoard targets already
  speaks Gomocup/Yixin; not a real near-term need.
- **UI-triggered custom commands** (context menu, generated button) — console-only for v1.
- **Hot-reload of `.ptc` mid-session** — load-once only (Q6).
- **Multi-line/streaming `on_reply`** — v1 is strictly one-line-in/one-action-out (Q8); no
  `INFO`/PV-style accumulation state machine.
