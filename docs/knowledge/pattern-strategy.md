---
name: pattern-strategy
description: Strategy pattern (GoF behavioral) - swap interchangeable algorithms behind one interface instead of branching on a type flag
domain: software-design
tags: gof,behavioral,polymorphism,open-closed
apply_when: "several interchangeable ways to do one job; a growing if/else or switch that picks an algorithm; behavior must change at runtime or per configuration"
sources: "Gamma, Helm, Johnson, Vlissides - Design Patterns (1994), Behavioral Patterns: Strategy (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); Fowler - Refactoring 2nd ed. (2018), Replace Conditional with Polymorphism; University of Toronto CSC207 (Winter 2017) - Design Patterns lecture slides (read 2026-09-26: Strategy problem statement)"
last_reviewed: 2026-09-26
confidence: high
---

# Strategy

## Intent

Define a family of algorithms, put each behind the same interface, and let the caller
(the *context*) use whichever one it was given. The context depends on the interface,
never on a concrete algorithm.

## Use when

- One job can be done several ways (pricing rules, sorting order, compression, routing,
  retry policy, move-ordering in a game engine) and the choice varies by configuration,
  user, or runtime state.
- You see the same `if kind == ... elif kind == ...` chain in more than one place, or it
  grows each time a variant is added (violates open-closed).
- You want to test each algorithm in isolation, or swap a fake in tests.

## Do not use when

- There is only one algorithm, or the variants never change. The interface is pure cost.
- The variants differ by one value, not by behavior. Pass the value (a parameter or config).
- The language has first-class functions and the strategy is a single operation: pass a
  function/lambda instead of writing an interface plus classes.
- The branching happens once, at one place, with two or three stable cases. A plain
  conditional is clearer.
- The object's behavior changes because *its own state* changed and states trigger
  transitions: that is State, not Strategy.

## Trade-offs

| Gain | Cost |
|---|---|
| Adds variants without editing the context (open-closed) | More types and indirection to read |
| Removes conditionals; each algorithm testable alone | The caller (or a factory) must know which strategy to pick |
| Swap at runtime | Strategies needing much context data push a fat interface or many parameters |
| | Small overhead of a call through the interface (rarely matters) |

## Minimal example (Python, callable form)

```python
from typing import Callable

Discount = Callable[[float], float]

def no_discount(total: float) -> float: return total
def seasonal(total: float) -> float:    return total * 0.9

class Checkout:
    def __init__(self, discount: Discount = no_discount):
        self._discount = discount

    def total(self, subtotal: float) -> float:
        return self._discount(subtotal)

Checkout(seasonal).total(100.0)   # 90.0
```

Use a class with a shared interface (ABC/Protocol/interface) when a strategy has its own
state or several operations; use a plain callable when it is one operation.

## Common mistakes

- Creating a strategy interface for a single implementation "for flexibility"
  (speculative generality).
- Leaking concrete strategy types into the context (`isinstance` checks defeat the pattern).
- Strategies that reach back into the context's private fields; pass what they need
  explicitly instead.
- Selecting the strategy with another large conditional scattered around: centralise the
  choice in one factory or a lookup table (`{"seasonal": seasonal}`).
- Confusing with State (state changes itself) or Template Method (inheritance, not
  composition).

## Related

- Skills: `refactor-replace-conditional-with-polymorphism`, `solid-open-closed`,
  `oop-composition-over-inheritance`
- Notes: `cleancode-code-smells` (Repeated Switches is the usual trigger)
