# ARCH-03 — Narrow CommandContext and split registerBuiltins()

**Status:** ✅ DONE
**Area:** `src/command/command_dispatcher.{h,cpp}`
**Priority:** P3
**Source:** architecture review, 2026-10-05 — [docs/audit/2026-10-05-architecture-review.md](../audit/2026-10-05-architecture-review.md)
**Design:** none — scoped directly
**Depends on / relates to:** ARCH-01

## Problem

`CommandContext` exposes `EngineProcess&` to handlers, so `command/` reaches the lowest engine layer (DIP). `command_dispatcher.cpp` (887 lines) registers every command in one function.

## Scope (in order)

1. Route handler needs through `EngineController`; drop `EngineProcess&` from the context (or hide it behind a narrow interface).
2. Split `registerBuiltins()` into per-group registrars (engine / board / game / …).

## Reasoning

Handlers should depend on the controller, not the raw process (DIP). Rejected: an abstract `IEngineIO` interface now (no second implementation exists); leaving the 887-line registrar (hurts OCP and review).

## Knowledge

- `docs/knowledge/pattern-command.md`, `docs/knowledge/principle-solid-overview.md` (DIP/OCP)
- `.claude/skills/solid-dependency-inversion`, `.claude/skills/solid-open-closed`
- Audit: `docs/audit/2026-10-05-architecture-review.md`

## Acceptance criteria

- No `EngineProcess` in `command/` headers (or a justified narrow interface).
- `test_cons*` and `test_proto07_yxanalz_console` pass; `!help` output byte-identical.

## Scope boundary

- No new commands; no handler behavior change.

## Outcome (2026-10-06)

- `registerBuiltins()` split into seven per-help-group registrars; `EngineProcess&` removed from `CommandContext` (handlers use new `EngineController::isRunning()`); `grep -rn EngineProcess src/command` empty. No abstract interface, no new commands, no handler behaviour change.
- Verification: new `tests/test_arch03_help_pinned.cpp` pins `!help` + registered names/usage (golden from pre-refactor code); Release ctest 5/5; `ranls-gui-tests` 228/2625 (= main); `ranls-gui-ui-tests` 54/393 (main 52/391, +2); `test_cons*` and `test_proto07_yxanalz_console` green.
- Deviation: registration order inside `specs_` is now grouped (was interleaved); unobservable, covered by the pinned test. Fix-log: [2026-10-06-arch-03-narrow-command-context](../fix-log/2026-10-06-arch-03-narrow-command-context.md).
