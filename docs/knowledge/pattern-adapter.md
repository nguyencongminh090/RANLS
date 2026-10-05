---
name: pattern-adapter
description: Adapter pattern (GoF structural) - wrap an existing class so it presents the interface your code expects
domain: software-design
tags: gof,structural,integration,legacy
apply_when: "you must use a class, library or service whose interface does not match what your code expects and you cannot or should not change it"
sources: "Gamma et al. - Design Patterns (1994), Adapter (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); refactoring.guru - Adapter (fetched 2026-09-25, re-verified 2026-09-26: intent, applicability, pros, cons, relations); Wikipedia - Adapter pattern (read 2026-09-26: intent, object versus class adapter, relations)"
last_reviewed: 2026-09-26
confidence: high
---

# Adapter

## Intent

A small class implements the interface the client wants and translates each call to the
incompatible class it wraps (the *adaptee*). Client and adaptee stay unchanged.

## Use when

- Integrating a third-party library, legacy module or external API whose shape differs from
  your own interface, and you cannot edit it.
- You want to keep a vendor's types out of the core: the adapter is the only place that
  knows them (the "adapter" side of ports and adapters).
- Several similar services need one common interface (payment gateways, storage SDKs).

## Do not use when

- You own the adaptee and can simply change its interface; a wrapper adds indirection.
- The mismatch is one renamed method at one call site. A local function is enough.
- You need to change *behavior*, not the interface: use Decorator (same interface, added
  behavior) or Strategy.
- You want a simpler front for a whole subsystem: that is Facade.

## Trade-offs

| Gain | Cost |
|---|---|
| Conversion code isolated from business logic | Extra class and one more hop per call |
| New adapters need no client change | Leaky when the adaptee has features the target interface cannot express |
| Vendor lock-in confined to one spot | More types to maintain |

## Minimal example (Python, object adapter)

```python
class Notifier:                       # what our code expects
    def send(self, user: str, text: str): ...

class TwilioSmsAdapter(Notifier):
    def __init__(self, client): self._c = client   # adaptee
    def send(self, user, text):
        self._c.messages.create(to=lookup_phone(user), body=text)
```

Object adapters (composition) work in every language. Class adapters (inheriting the
adaptee) need multiple inheritance and couple you to its internals; prefer composition.

## Common mistakes

- Putting business rules inside the adapter; it should only translate.
- Exposing adaptee types in the adapter's signatures, so the leak remains.
- Wrapping a class you own instead of fixing it.
- Confusing with Proxy (same interface) and Facade (new simplified interface).

## Related

- Skills: `hexagonal-ports-adapters`, `hexagonal-domain-isolation`, `solid-dependency-inversion`
- Notes: `pattern-decorator` (wraps too, but keeps the interface)
