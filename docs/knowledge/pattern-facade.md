---
name: pattern-facade
description: Facade pattern (GoF structural) - one simple entry point in front of a complex subsystem, so callers stop depending on its internals
domain: software-design
tags: gof,structural,layering,simplification
apply_when: "callers use only a small part of a complex library or subsystem and keep repeating the same multi-step calls; you want a clean boundary between layers or modules"
sources: "Gamma et al. - Design Patterns (1994), Facade (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); refactoring.guru - Facade (fetched 2026-09-25, re-verified 2026-09-26: intent, applicability, pros, cons, relations); Wikipedia - Facade pattern (read 2026-09-26: intent, structure, relations)"
last_reviewed: 2026-09-26
confidence: high
---

# Facade

## Intent

A class offers a few high-level operations and does the coordination of many subsystem
classes behind them. Clients call the facade; the subsystem stays usable directly for the
rare caller that needs full control.

## Use when

- A library or subsystem is large, and most callers need the same three or four workflows
  (encode a video, place an order, initialise a payment).
- You want a module boundary: other layers talk to one public class, not to its internals.
- Wrapping a messy legacy area so new code has a clean surface while the mess is refactored.

## Do not use when

- The subsystem is already simple; the facade only renames calls.
- Callers need fine-grained control of the parts; a facade would either hide it or copy the
  whole API.
- You need to make one incompatible interface fit another: that is Adapter.
- The facade would need to know about everything in the app. It becomes a god object; split
  it into several smaller facades by use case.
- The goal is to coordinate two-way interaction among peers: that is Mediator.

## Trade-offs

| Gain | Cost |
|---|---|
| Clients are coupled to one class, not many | Risk of a god object that knows every class |
| Simple, readable calling code | Hides options; features must be added to the facade |
| Clear layer boundary, easier to change internals | An extra layer to maintain and test |

## Minimal example (Python)

```python
class OrderFacade:
    def __init__(self, stock, payments, shipping):
        self._stock, self._pay, self._ship = stock, payments, shipping

    def place_order(self, cart, card):
        self._stock.reserve(cart.items)
        receipt = self._pay.charge(card, cart.total)
        self._ship.schedule(cart, receipt)
        return receipt
```

## Common mistakes

- Making the facade the only allowed path and hiding features clients legitimately need.
- Letting business rules migrate into the facade instead of the subsystem.
- One facade per application ("AppManager") that grows without bound.
- Confusing with Adapter (translates an interface) and Abstract Factory (hides creation).

## Related

- Skills: `hexagonal-domain-isolation`, `solid-interface-segregation`, `oop-encapsulation`
- Notes: `pattern-adapter`, `pattern-abstract-factory`
