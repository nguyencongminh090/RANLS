---
name: style-hexagonal
description: Hexagonal architecture (ports and adapters) - keep domain logic free of frameworks, I/O and UI by putting it behind ports, so it can be driven and tested without them
domain: software-architecture
tags: architecture,hexagonal,ports-adapters,clean-architecture,dependency-inversion
apply_when: "domain rules must outlive framework/database/UI choices; business logic is hard to test without I/O; several entry points (UI, CLI, API, tests) or several backends share one core"
sources: "Cockburn - Hexagonal Architecture, alistair.cockburn.us, v0.9 4 Sep 2005 (read); Martin - The Clean Architecture, blog.cleancoder.com, 13 Aug 2012 (read); Evans - Domain-Driven Design Reference, 2015, CC BY 4.0, Anticorruption Layer (read); Martin - Clean Architecture book, 2017, ch. 22 (read 2026-09-26: Dependency Rule, shared objective of separation of concerns via layers, independence of framework/UI/database, testability; the book's list names hexagonal, DCI and BCE, while onion and screaming come from the 2012 blog)"
last_reviewed: 2026-09-26
confidence: high
---

# Hexagonal architecture (ports and adapters)

## Intent

Cockburn's stated intent: allow an application to be driven equally by users, programs,
automated tests or batch scripts, and to be developed and tested in isolation from its
eventual run-time devices and databases. His problem statement: business logic leaks into
the presentation code, which blocks automated testing, batch use and technology substitution;
the same happens with databases.

Structure: the application core talks to the outside only through **ports** (defined
interfaces for a purposeful conversation), and **adapters** convert between a port and a
specific technology (UI, database, test harness). Inside and outside are treated asymmetrically
rather than as a stack of layers.

## Vocabulary

- **Core:** domain model plus use cases. No framework, database driver, HTTP or UI imports
  (this is the usual reading of the idea; Cockburn does not prescribe a core's internals).
- **Driving (primary) port:** what the outside calls to use the app (a use-case interface).
  Driving adapters: web controller, CLI, GUI, test harness (Cockburn substitutes automated
  test fixtures for the primary actor).
- **Driven (secondary) port:** what the core needs from the outside (repository, clock,
  mail sender, engine process). Driven adapters: SQL repository, HTTP client, in-memory fake
  (Cockburn substitutes mock objects for secondary actors such as databases). Cockburn uses
  both pairs of terms, primary/secondary and driving/driven.
- **Number of ports:** the hexagon is not special; Cockburn says its shape only gives room to
  draw ports, and he sees applications with two, three or four natural ports.
- **Dependency Rule (Martin):** Martin lists Cockburn's hexagonal architecture among several
  (onion, screaming, DCI, BCE) that he says share one objective, separation of concerns by
  layers, and yield systems independent of frameworks, UI and database, and testable. His
  version: source dependencies point inward through concentric rings; the rings are
  schematic and there can be more than four. Where control flows outward, the inner ring
  declares an interface the outer ring implements (dependency inversion). The idea that
  the core itself owns the driven port comes from this dependency rule, not from Cockburn's
  article.

## Use when

- Domain logic is non-trivial and would otherwise be tangled with persistence or UI.
- You need fast tests of business rules using in-memory adapters instead of real I/O.
- The same core is exposed through more than one entry point, or the backend may change
  (database, external service, GUI toolkit).
- A legacy or third-party model must not leak into your domain: wrap it in an
  anti-corruption layer adapter. This is Evans's pattern, not Cockburn's: a downstream
  system creates an isolating layer that offers the upstream system's functions in terms of
  its own model, translating in one or both directions, needed when a large upstream
  interface would otherwise bend the downstream model.

## Do not use when

- The app is basically CRUD with little logic: ports add files but no isolation value.
- Prototypes, throwaway scripts, or a single small module with one entry point.
- The team cannot sustain the discipline; half-applied layering (core importing an ORM
  "just once") is worse than an honest simple layered design.
- Performance-critical inner loops where an extra indirection per call matters; keep the
  hot path inside the core and put the port at a coarser boundary.

## Trade-offs

| Gain | Cost |
|---|---|
| Core testable with no I/O; fast, deterministic tests | More interfaces, mapping code and files |
| Backends/UI replaceable with local changes | Data mapping between core and adapter models |
| Business rules readable in domain terms | Overkill for simple apps; easy to over-layer |
| Clear place for each dependency | Need a composition root that wires adapters to ports |

## Minimal shape

```
adapters/in/    http_controller.py, cli.py        -> call use cases
core/           order.py (domain), place_order.py (use case)
core/ports/     order_repository.py (interface the core needs)
adapters/out/   sql_order_repository.py, in_memory_order_repository.py
main.py         composition root: builds adapters, injects into use cases
```

```python
class OrderRepository(Protocol):          # driven port, owned by the core
    def save(self, order: Order) -> None: ...

class PlaceOrder:                         # use case, sees only the port
    def __init__(self, orders: OrderRepository): self._orders = orders
    def __call__(self, cmd) -> None: self._orders.save(Order.create(cmd))
```

## Common mistakes

- Ports shaped like the database or framework (`save_to_table`) instead of the domain's
  language; the core still depends on the outside in spirit.
- Domain entities carrying ORM or serialization annotations.
- Business rules creeping into controllers or repositories.
- One port per class "just in case": define ports where a real boundary or a test seam
  exists.
- Wiring adapters inside the core instead of a single composition root.

## Verification note

Re-verified on 2026-09-21 against Cockburn's original article (intent, ports/adapters,
terminology, hexagon arbitrariness), Martin's 2012 post (shared objective, dependency rule,
ring count) and Evans's DDD Reference (anti-corruption layer). The earlier text attributed the
"inward dependency" and anti-corruption ideas to the wrong sources; that is corrected above.
The 2017 Clean Architecture book was not read. The Python sketch and the "Use when / Do not use
when" advice are practical guidance, not quotations.

## Related

- Skills: `hexagonal-ports-adapters`, `hexagonal-domain-isolation`,
  `solid-dependency-inversion`, `software-architecture` (project-specific layering example)
- Notes (software-design domain): `pattern-strategy` (a driven port with swappable implementations is Strategy at
  architecture scale)
