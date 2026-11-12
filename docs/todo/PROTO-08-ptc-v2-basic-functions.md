# PROTO-08 — `.ptc` v2: basic functions for extension authors (design gate)

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/engine/protocol_extension.{h,cpp}` (expression parser, function table, version gate), later `src/ui/` board sinks; `tests/test_proto03_protocol_extension.cpp`
**Priority:** P3
**Source:** user question 2026-11-04 ("is the PTC command set optimal? can we design basic functions for users?"); review of PROTO-03
**Design:** `features/protocol-extension-v2/` (planning.md Q10–Q18 **OPEN**)
**Depends on / relates to:** PROTO-03 (baseline), PROTO-04/ANLZ-06 (state machine — deliberately not touched), UI-16/17/18 + UX-07 (board marks, for the board sinks)

## Problem

PROTO-03's `.ptc` can only display values: 3 sinks, comparisons only, one reply line → one action, no
command chaining. Authors cannot format numbers (`0.93` → `93%`), do arithmetic, mark the board, or
trigger a follow-up command. See `features/protocol-extension-v2/user_story.md`.

## Scope (in order)

1. Resolve `planning.md` Q10–Q18 with the user (design gate, size L). **No code before this.**
2. Then write `docs/instruction/PROTO-08-*.md` and split into 08a (pure functions + arithmetic + `ptc_version = 2`),
   08b (board sinks), 08c (`run` chaining, optional).

## Scope boundary

- Do not touch `group = "analysis"` / Stop / multi-line replies / unsolicited-line patterns (still deferred).
- Do not weaken fail-closed loading or the Q5 limits; any new surface gets its own bound and tests.
- Reversing v1 Q2 ("no arithmetic") needs an audit entry when decided.

## Reasoning

Review 2026-11-04: v1 is safe but display-only. Pure whitelisted functions + narrow arithmetic give most of
the value at the lowest risk; board sinks reuse the existing marks layer; `run` is split out because it
introduces loop/storm risk.

## Knowledge

`features/protocol-extension/planning.md` (Q1–Q9); `docs/knowledge/principle-layering-dependency-rule.md`;
`python3.13 scripts/audit-search.py "protocol extension"` for past decisions.

## Acceptance criteria

- planning.md Q10–Q18 resolved and recorded; instruction file written; audit entry for any reversed decision.
- (Implementation phases get their own acceptance criteria in the instruction file.)
