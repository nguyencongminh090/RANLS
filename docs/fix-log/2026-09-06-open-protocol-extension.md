# 2026-09-06 — Open Protocol Extension (`.ptc` user-defined commands) — PROTO-03

This is a **feature**, not a bug fix, but this repo's convention logs tracked engine/model work
here alongside fixes (no separate feature-log convention exists). Full design lives in
`features/protocol-extension/` and the formal entry in `docs/todo/PROTO-03-open-protocol-extension.md`.

## Prompt

Implement PROTO-03 (Sprint 13): let an engine developer declare new console-triggered commands in a
`.ptc` (TOML) file loaded by `GomocupProtocol` at runtime — no C++ change, no rebuild. Q1–Q9 in
`features/protocol-extension/planning.md` were all resolved with the user 2026-09-05/06. Follow the
8-step build order in `docs/instruction/PROTO-03-open-protocol-extension.md`; honour every "Do not"
as a hard constraint. Work on branch `proto-03/open-protocol-extension`; do not touch `main`.

## Action

1. **`src/engine/protocol_extension.{h,cpp}` (new, standalone).** Hand-rolled strict TOML-subset
   parser (top-level scalars, `[[command]]` / `[[command.on_reply]]` arrays-of-tables, string +
   string-array values, `"""` multi-line strings, dotted keys). Chosen over vendoring toml++: the
   schema is narrow enough that a small strict parser keeps the trust boundary minimal and is fully
   covered by the malformed-input tests; no `third_party/` directory was added. Fail-closed
   validation: Q3 name collision against the full built-in registry
   (`protoext::builtinCommandNames()` — mirrors `CommandDispatcher::registerBuiltins`, 18 names),
   Q5 limits, `group` restricted to `board|config|database|debug|engine|info` (no `analysis` in v1),
   `ptc_version = 1` / `extends = "gomocup"`.
2. **Mini-DSL.** Recursive-descent parser for exactly Q2's grammar (`if/elif/else … end`,
   comparisons + `and`/`or`/`not`, no arithmetic/loops/free variables). Exactly 3 whitelisted sinks
   via a `std::map<std::string, fn>` dispatch table. Bounded interpreter (depth + statement count)
   as defense-in-depth.
3. **`GomocupProtocol`** additionally implements the new minimal `ICustomCommandSource`
   (`src/engine/i_custom_command_source.h`) — NOT a method on `IEngineProtocol`. `parseLine()` runs
   built-in parsing first and unchanged; extension `on_reply` patterns are tried only on no match.
   A type-mismatched reply line is skipped with a Debug log (PROTO-01 precedent), never UB.
4. **`EngineController`**: `loadExtensionTable()` (load-once at `startEngine()`/`reloadEngine()`,
   fail-closed), `sendCustomCommand()`, `signal_custom_action`. `customSource_` stays null on a
   `.ptc`-less run — that path is byte-identical to before. `$currentPath` is resolved here at send
   time from the read-only game path (never mutated).
5. **`CommandDispatcher::syncExtensionCommands()`** registers each loaded command name into the same
   `!`-prefixed registry as built-ins (Q7); new `[extension]` help group; declared `group` is
   informational only (Q9) — every extension command is instant fire-and-forget.
6. **UI** (`main_window.cpp`): `set_status_field` → a new small dynamic name→`Gtk::Label` container
   in `EngineStatusView` (the 6 fixed stat members untouched); `toast` → the existing crash-banner
   widget via `AnalysisPanel::showInfoBanner()` (no libadwaita); `log` → `EngineLogModel`. No
   `BoardRenderer` / `BoardViewModel` change.
7. **`EngineConfig::protocolExtensionPath`** persisted via `SettingsStorage`; `*.ptc` file-chooser
   row in `SettingsDialog` (`onApply` merges from the base config — STATE-02-safe).
8. **`tests/test_proto03_protocol_extension.cpp`** (14 doctest cases) in `ranls-gui-tests`.

## Summary

All 8 build-order steps implemented on branch `proto-03/open-protocol-extension`. No `IEngineProtocol`
/ `EngineProcess` / existing Gomocup-parsing change; no board-drawing sink; DSL has no
arithmetic/loops; no hot-reload / multi-line `on_reply`; `group` never wired to
`EngineState`/`SearchIntent`/`pendingStopFlush_`.

**Verification:** `./build.sh` clean, no new warnings in touched files. `ctest` —
`ranls-gui-tests` 209/209 cases (2466 assertions), 14 new PROTO-03 cases green; `rel02-version`
green. `ranls-gui-ui-tests` carries one **pre-existing** failure
(`test_anlz05_no_automove_action`, real/fake-engine timing — reproduced identically on branch base
`8b010ed`), all other 25 UI cases pass. "No `.ptc` loaded" acceptance criterion verified by test
(`builtin registry` / non-matching-line cases) and by code reading: `customSource_` is only set
after a successful load, `syncExtensionCommands()` iterates an empty `customCommandNames()`, and
`GomocupProtocol::tryExtensionReply()` early-returns when `ext_` is null.

**Still pending a human:** the live-engine + display "Manual smoke" tier (instruction §"Manual
smoke") — no engine binary and no display server on this build host.
