---
name: style-ui-mvc-family
description: Presentation architectures - MVC's origin and intent, and Fowler's variants (Forms and Controls, Presentation Model, MVP with Supervising Controller or Passive View) with what each separates and what it costs in testability
domain: software-architecture
tags: architecture,style,ui,mvc,mvp,presentation-model,separated-presentation
apply_when: "structuring the UI layer of a desktop, web or mobile client, or choosing how much logic to keep out of widgets for testability"
sources: "Fowler - GUI Architectures, martinfowler.com/eaaDev (read via fetch summary); Reenskaug - MVC index page, Univ. of Oslo (read: origin at Xerox PARC 1978-79, component roles)"
last_reviewed: 2026-09-21
confidence: medium
---

# Presentation architectures (MVC family)

## MVC: origin and intent

Trygve Reenskaug developed MVC as a visiting scientist at Xerox PARC in 1978-79; the papers
are dated 1979 (May: Thing-Model-View-Editor; December: Models-Views-Controllers). His stated
purpose: bridge the gap between the user's mental model and the digital model in the
computer, letting users see the same model element in different contexts at the same time.
His roles: Model (domain data and logic), View (its visual representation), Controller
(creates and coordinates views), Editor (short-lived interface between a view and input).

Fowler's reading of the core idea is **Separated Presentation**: keep domain objects clearly
apart from the presentation that displays them; the domain never depends on the UI. Views and
controllers observe the model, so several presentations can show it without talking to each
other.

## Fowler's catalogue of variants

| Pattern | What it does | Note |
|---|---|---|
| Forms and Controls | Application-specific forms lay out generic controls; data binding syncs screen and records | Simple; logic tends to sit in the form |
| Model View Controller | Model holds domain behaviour; view and controller pairs handle display and input; observers keep views current | Observer flow is implicit and can be hard to understand and debug |
| Presentation Model | A model that belongs to the presentation layer holds presentation state and logic; widgets observe it instead of the domain object | Moves UI logic out of widgets into a testable class |
| Model View Presenter | Tries to combine Forms-and-Controls simplicity with MVC separation; user gestures go to a presenter that coordinates the model | Comes in the two forms below |
| Supervising Controller | The presenter handles complex updates; views do simple mappings declaratively | Balance of testability and presenter size |
| Passive View | The presenter manipulates all widgets; the view has no logic | Most logic testable, at the price of test doubles for the view |

## Use when

- Any interactive UI whose logic you want to test without the real widget toolkit: pick
  Presentation Model or MVP with a testable presenter.
- Several views of one model must stay consistent (MVC's observer idea).
- The UI toolkit is likely to change but the domain must survive it (Separated Presentation;
  compare `style-hexagonal`).

## Do not use when

- The screen is a trivial form or a throwaway tool: Forms and Controls with data binding is
  enough; presenters and models add files without benefit.
- You use a framework that already prescribes a variant: follow its conventions instead of
  layering another pattern on top, unless there is a testing or complexity problem to solve.
- You treat the names as precise: the same word means different things across frameworks
  (see below).

## Trade-offs

- More separation gives more testable logic but more classes and more wiring between view and
  presenter or model.
- Observer-style synchronization removes direct coupling but makes update flow implicit;
  explicit presenter calls are easier to trace but couple presenter to view API.
- Passive View maximizes the amount of logic testable without the UI; Supervising Controller
  reduces presenter size by letting simple binding handle simple mappings.

## Common mistakes

- Business rules in controllers or views: Separated Presentation is lost.
- A presentation model or presenter that depends on concrete widgets, defeating testing.
- Assuming a web framework's "MVC" is Reenskaug's MVC; frameworks reuse the name for
  request routing structures, which differ from the original observer-based desktop design.
  (This last point is a general caution, not a claim from the two sources read.)

## Verification note

Fowler's page was read through a fetch summary, not the full text; Reenskaug's page gave
origin and roles. MVVM, Flux/Redux and web-framework MVC are not covered by these sources and
are deliberately not described here. `confidence: medium`.

## Related

- Notes: `principle-layering-dependency-rule`, `style-hexagonal`, `quality-testability`
- Notes (software-design): `pattern-strategy` (a view or presenter behind an interface is a
  strategy-like seam)
- See also: `apptype-web-frontend`, `apptype-desktop-gui`
