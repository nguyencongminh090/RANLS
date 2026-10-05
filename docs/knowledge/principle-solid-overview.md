---
name: principle-solid-overview
description: SOLID overview - five class-design heuristics, what each really says, how they interlock, and when applying them mechanically hurts
domain: software-design
tags: solid,principles,oop,design-review
apply_when: "reviewing or designing class and module structure and you need to decide which SOLID principle is relevant, or to judge whether a SOLID-driven refactor is worth its cost"
sources: "Martin - Design Principles and Design Patterns (2000) and 'The Single Responsibility Principle' (blog, 2014-05-08, read); Wikipedia - SOLID (read); Martin - Clean Architecture (2017), ch. 7-11 and the SOLID introduction (read 2026-09-26: SRP actor wording, Feathers 2004 acronym, OCP/LSP attributions to Meyer 1988 and Liskov 1988); Meyer OCP (1988) and Liskov substitution (1987/1994) originals not re-read"
last_reviewed: 2026-09-26
confidence: high
---

# SOLID overview

SOLID names five heuristics collected by Robert C. Martin around 2000; the acronym came
later (attributed to Michael Feathers, about 2004). They are guidelines that reduce the
cost of change, not laws. Each has its own skill with steps; this note is the map.

## The five, in one line each

| Letter | Principle | What it asks | Skill |
|---|---|---|---|
| S | Single Responsibility | Group what changes for the same reason; separate what changes for different reasons | `solid-single-responsibility` |
| O | Open-Closed | Add behavior by adding code, not by editing tested code | `solid-open-closed` |
| L | Liskov Substitution | A subtype must be usable wherever its base type is expected without surprising callers | `solid-liskov-substitution` |
| I | Interface Segregation | Do not force clients to depend on methods they do not use | `solid-interface-segregation` |
| D | Dependency Inversion | High-level policy depends on abstractions, and details depend on them too | `solid-dependency-inversion` |

Martin's later reading of SRP: a module should answer to one actor, that is one group of
people who would ask for changes. Bug fixes and refactoring are not "reasons to change" in
this sense.

## How to pick

- Class keeps changing for unrelated reasons, or one edit breaks an unrelated feature: SRP.
- Every new variant forces edits in the same `if/switch`: OCP (see `pattern-strategy`).
- Subclass throws "not supported", weakens a contract, or callers check its type: LSP.
- Implementers stub out methods, or a change in one method recompiles unrelated clients: ISP.
- Business logic imports framework, database or vendor code, and cannot be tested alone: DIP.

## Use when

- Reviewing a design that is hard to change or test, and you need a name for the pain.
- Deciding where an abstraction boundary should sit.

## Do not use when

- The code is small, stable or throwaway. Splitting it costs more than it saves.
- You would add an interface "for flexibility" with one implementation and no test need
  (speculative generality, see `cleancode-code-smells`).
- Applied as a checklist without a concrete change problem in view. Start from the pain,
  then choose the principle.
- In functional or data-oriented code the same ideas still hold, but as functions and
  modules; do not force class hierarchies to satisfy the letters.

## Trade-offs

More small units and more indirection buy easier change and testing. The price is more files
to read and more wiring. Over-applying SRP gives dozens of tiny classes with no cohesion;
over-applying OCP gives extension points nobody uses.

## Common mistakes

- Reading SRP as "a class does only one thing" (the method-level meaning) instead of "one
  reason to change".
- Treating LSP as only about signatures; it also covers behavior contracts.
- Applying DIP everywhere; invert only at boundaries that really vary or need faking.
- Using one principle to justify a refactor without checking it against the others.

## Related

- Skills: the five `solid-*` skills above, `oop-composition-over-inheritance`, `hexagonal-domain-isolation`
- Notes: `pattern-strategy`, `principle-composition-over-inheritance`, `principle-separation-of-concerns`
