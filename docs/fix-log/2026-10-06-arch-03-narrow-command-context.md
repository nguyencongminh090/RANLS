# ARCH-03: narrow CommandContext, split registerBuiltins() (2026-10-06)

**Status:** Active
**Related:** ARCH-03

## Prompt

Implement ARCH-03 (architecture review 2026-10-05): `CommandContext` exposes `EngineProcess&` to command handlers (DIP breach: `command/` reaches the lowest engine layer) and `registerBuiltins()` registers every command in one ~420-line function.

## Root cause

Not a bug fix; a layering/OCP refactor. Handlers only ever used `ctx_.engine.isRunning()` (8 call sites incl. `executeLine` and `syncExtensionCommands`); all real engine I/O already went through `EngineController`. The registrar grew by accretion into one function.

## Fix

- `src/command/command_dispatcher.{h,cpp}`: `registerBuiltins()` now calls seven private registrars along the help groups (`registerInfoCommands`, `Board`, `Analysis`, `Engine`, `Config`, `Debug`, `Database`). Every `CommandSpec` and handler body is moved verbatim. `EngineProcess&` removed from `CommandContext`; forward declaration and `engine/engine_process.h` include dropped. `grep -rn EngineProcess src/command` is empty.
- `src/engine/engine_controller.{h,cpp}`: new `bool isRunning() const` forwarding to `EngineProcess::isRunning()` (same semantics the handlers had; no raw-send API was needed since all sends already used `controller.sendRawCommand`).
- `src/main_window.cpp` (one-line CommandContext construction) and the three tests that build a context updated.
- Deliberately left alone: no new commands, no handler behaviour change, no abstract `IEngineIO` (no second implementation), ARCH-02 areas.

## Verification

- Regression test `tests/test_arch03_help_pinned.cpp` (`ranls-gui-ui-tests`, 2 cases / 2 assertions): pins the full `!help` output and sorted `registeredNames()` + `commandUsage()`; golden captured from the PRE-refactor code and passing before and after.
- Release `RUN_TESTS=1 ./build.sh`: ctest 5/5; `ranls-gui-tests` 228 cases / 2625 assertions (identical to main); `ranls-gui-ui-tests` 54 cases / 393 assertions (main 52 / 391, +2 new); `test_cons*`, `test_proto07_yxanalz_console` green.
- Note: registrars are grouped, so the internal registration order in `specs_` changed (it was interleaved across groups). Unobservable: `!help` sorts by group then name, `registeredNames()` sorts, and names are unique (pinned by the test).

## Knowledge

`docs/knowledge/pattern-command.md`, `docs/knowledge/principle-solid-overview.md`, `docs/audit/2026-10-05-architecture-review.md`.
