# ARCH-03 — Narrow CommandContext and split registerBuiltins()

**Status:** 🔲 OPEN (Backlog)
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
