---
name: pattern-decorator
description: Decorator pattern (GoF structural) - wrap an object in same-interface wrappers to add behavior at runtime without subclassing
domain: software-design
tags: gof,structural,composition,wrapper
apply_when: "responsibilities must be added or combined per object at runtime, subclassing would explode into combinations, or the class cannot be subclassed"
sources: "Gamma et al. - Design Patterns (1994), Decorator (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); refactoring.guru - Decorator (fetched 2026-09-25, re-verified 2026-09-26: intent, applicability, pros, cons, relations); Wikipedia - Decorator pattern (read 2026-09-26: intent, subclass explosion, structure, relations)"
last_reviewed: 2026-09-26
confidence: high
---

# Decorator

## Intent

A wrapper implements the same interface as the object it holds, forwards calls to it and
adds behavior before or after. Wrappers can be stacked, so features combine by composition
instead of a subclass per combination.

## Use when

- Optional features can be mixed freely (logging, caching, retry, compression, encryption
  around a stream or a service client).
- Subclassing would need a class per combination, or the class is final or third-party.
- Behavior should be added or removed per instance at runtime.

## Do not use when

- The feature is always on: put it in the class.
- Order-dependence would be confusing and there are only one or two fixed variants.
- The language offers a lighter tool that fits (Python function decorators, middleware,
  aspect or interceptor support): use it for cross-cutting calls.
- You must change the interface, not just add behavior: use Adapter.
- Clients need to inspect or remove one wrapper from the middle of a stack; that is hard
  with Decorator.

## Trade-offs

| Gain | Cost |
|---|---|
| No subclass explosion; combine features freely | Many small objects; stack traces get deep |
| Single-responsibility wrappers | Behavior depends on wrapping order |
| Add or remove behavior at runtime | Removing one wrapper from a stack is awkward |
| | Setup code can look cluttered (use a builder or factory) |

## Minimal example (Python)

```python
class Repo:                                    # component interface
    def get(self, key): ...

class DbRepo(Repo):
    def get(self, key): return db.query(key)

class CachedRepo(Repo):                        # decorator
    def __init__(self, inner: Repo): self._inner, self._cache = inner, {}
    def get(self, key):
        if key not in self._cache:
            self._cache[key] = self._inner.get(key)
        return self._cache[key]

repo = CachedRepo(DbRepo())
```

## Common mistakes

- Wrapper changes the contract (returns different types), so callers break.
- Ignoring order: encrypt-then-compress differs from compress-then-encrypt.
- Testing for the concrete wrapped type (`isinstance`).
- Using it where Strategy fits: Decorator adds around the object, Strategy swaps the
  object's inner algorithm.
- Confusing with Proxy: a proxy usually controls access or lifecycle of its target.

## Related

- Skills: `solid-open-closed`, `oop-composition-over-inheritance`, `solid-liskov-substitution`
- Notes: `pattern-adapter`, `pattern-strategy`
