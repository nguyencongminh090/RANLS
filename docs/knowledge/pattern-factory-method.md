---
name: pattern-factory-method
description: Factory Method pattern (GoF creational) - let a subclass or hook decide which concrete product a shared algorithm creates
domain: software-design
tags: gof,creational,inheritance,extension-point
apply_when: "a class must create objects whose concrete type is decided by a subclass, a framework user, or configuration; creation logic should be isolated from use"
sources: "Gamma et al. - Design Patterns (1994), Factory Method (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); refactoring.guru - Factory Method (fetched 2026-09-25, re-verified 2026-09-26: intent, applicability, pros, cons, relations); Wikipedia - Factory method pattern (read 2026-09-26: intent, structure, simple factory versus Abstract Factory, drawback); University of Toronto CSC207 (Winter 2017) - Design Patterns lecture slides (read 2026-09-26: Factory Method problem statement and when-to-use)"
last_reviewed: 2026-09-26
confidence: high
---

# Factory Method

## Intent

A base class declares a method that returns a product through a common interface; subclasses
override it to return different concrete products. The base class's other code calls that
method and works with the interface only.

## Use when

- A framework or base class needs to create objects but cannot know their concrete type;
  users extend it by subclassing (document editors creating their own document type,
  dialogs creating their own button).
- You want creation code in one place, apart from the code that uses the product.
- Product creation is costly and the factory method can hand out pooled or cached objects.

## Do not use when

- One concrete type exists. Call the constructor.
- The only variation is a value or a small conditional at one site. A plain function or a
  lookup table (`{"pdf": PdfDoc, "txt": TxtDoc}[kind]()`) is simpler.
- You would need a new subclass for every product just to change what gets created and the
  language can pass a factory function or class instead (composition, dependency injection).
- Whole families of related products must stay consistent: see `pattern-abstract-factory`.

## Trade-offs

| Gain | Cost |
|---|---|
| Creator is decoupled from concrete products | Extra creator subclasses; class count grows |
| Creation logic centralised (single responsibility) | Extension is by inheritance, which is rigid |
| New products without editing existing creators | Can be over-engineering when a function suffices |

## Minimal example (Python)

```python
class Exporter:                       # creator
    def make_writer(self): raise NotImplementedError
    def export(self, rows):
        w = self.make_writer()        # factory method
        for r in rows: w.write(r)
        w.close()

class CsvExporter(Exporter):
    def make_writer(self): return CsvWriter()
```

Where inheritance is unwanted, pass the factory in: `Exporter(make_writer=CsvWriter)`.

## Common mistakes

- Calling a "static factory" or `create_x(kind)` function with a big switch and calling it
  Factory Method; that is a simple factory, fine but a different thing.
- Returning products that do not share a real interface, forcing `isinstance` checks.
- Creating a hierarchy only to vary construction when a constructor parameter would do.

## Related

- Skills: `solid-open-closed`, `solid-dependency-inversion`, `oop-composition-over-inheritance`
- Notes: `pattern-abstract-factory` (often built from several factory methods),
  `pattern-strategy` (composition alternative for varying behavior)
