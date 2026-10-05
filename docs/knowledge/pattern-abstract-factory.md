---
name: pattern-abstract-factory
description: Abstract Factory pattern (GoF creational) - create whole families of related objects through one interface so variants are never mixed
domain: software-design
tags: gof,creational,product-families,portability
apply_when: "the code must work with several families of related products (for example UI toolkits or storage backends) and must never mix products from different families"
sources: "Gamma et al. - Design Patterns (1994), Abstract Factory (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); refactoring.guru - Abstract Factory (fetched 2026-09-25, re-verified 2026-09-26: intent, applicability, pros, cons, relations); Wikipedia - Abstract factory pattern (read 2026-09-26: intent, structure, drawbacks, relation to Factory Method)"
last_reviewed: 2026-09-26
confidence: high
---

# Abstract Factory

## Intent

An interface declares one creation method per product type (button, checkbox, ...). Each
concrete factory implements it for one variant (Windows, macOS; Postgres, in-memory) and
returns products that belong together. Client code receives a factory and never names a
concrete class.

## Use when

- There are two or more *variants*, each made of several cooperating product types, and
  products from different variants must not be combined.
- The variant is chosen once (config, platform, environment) and then applies everywhere.
- Test setups need a whole fake family (fake DB, fake clock, fake mailer) swapped together.

## Do not use when

- There is one product type: use Factory Method or a plain factory function.
- There is one variant, or products are independent of each other so mixing is harmless.
- The set of product types changes often. Adding a product type means editing the factory
  interface and every concrete factory.
- Step-by-step construction of one complex object is the problem: that is Builder.

## Trade-offs

| Gain | Cost |
|---|---|
| Products of one family are guaranteed compatible | Many interfaces and classes (N products x M variants) |
| Client is decoupled from concrete classes | Adding a new product type touches every factory |
| Whole variant swapped at one point | Overkill for small systems |

## Minimal example (Python)

```python
class UiFactory:                       # abstract factory
    def button(self): ...
    def checkbox(self): ...

class DarkUi(UiFactory):
    def button(self): return DarkButton()
    def checkbox(self): return DarkCheckbox()

def build_form(ui: UiFactory):         # client sees only the interface
    return [ui.button(), ui.checkbox()]

ui = {"dark": DarkUi, "light": LightUi}[config.theme]()
```

## Common mistakes

- Using it when only one creation method is needed.
- Letting client code import concrete products "just once", which breaks the isolation.
- Choosing the factory in many places instead of one composition root.
- Adding unrelated products to the factory until it becomes a service locator.

## Related

- Skills: `solid-dependency-inversion`, `hexagonal-ports-adapters`
- Notes: `pattern-factory-method` (each creation method is usually a factory method)
