---
name: principle-layering-dependency-rule
description: Layering and the dependency rule - order modules so dependencies run one way; covers classic layers (SEI), presentation-domain-data (Fowler), and the inward dependency rule (Martin)
domain: software-architecture
tags: principle,layering,dependency-rule,dependency-inversion,boundaries
apply_when: "organizing a codebase into layers or rings, deciding which module may depend on which, or untangling cyclic or upward dependencies"
sources: "Bachmann, Bass, Nord - Modifiability Tactics, CMU/SEI-2007-TR-002, section 8 (Layers pattern) (read); Fowler - Presentation Domain Data Layering, martinfowler.com bliki (read); Martin - The Clean Architecture, blog.cleancoder.com, 13 Aug 2012 (read)"
last_reviewed: 2026-09-21
confidence: high
---

# Layering and the dependency rule

## Intent

Constrain which modules may depend on which, so a change in one part cannot force changes
in parts that should not know about it, and each part can be understood, replaced and
tested separately.

## Three formulations

**1. Layers (SEI report, from the Layers pattern).** Responsibilities are grouped into
layers with semantic coherence, ordered as an abstraction ladder; a layer uses the services
of the adjacent lower layer only. Restricting communication paths reduces the possible paths
to the number of layers minus one and limits the side effects of replacing a layer. Each
layer exposes a public interface and hides private responsibilities. Variants: a *relaxed*
layered system (layer N may call any lower layer) increases coupling with layers N-2 and
below; *layering through inheritance* breaks encapsulation and raises coupling because lower
layers become extensions of inherited code.

**2. Presentation - Domain - Data (Fowler).** The common split is UI/HTTP handling, business
logic (validations, calculations), and persistence. Reasons: narrower focus reduces
cognitive load. Dependencies normally run top to bottom (presentation on domain on data),
but a mapper can invert domain-to-data so the domain does not depend on the data source,
which Fowler ties to hexagonal architecture. Cautions: apply the split at a relatively small
granularity; as a system grows, top-level modules should be domain-oriented with layering
inside them; do not organize teams by layer ("developers don't have to be full-stack but
teams should be").

**3. The Dependency Rule (Martin).** Source code dependencies can only point inward.
Concentric rings from the center: entities (enterprise business rules), use cases
(application rules), interface adapters (controllers, presenters, gateways), frameworks and
drivers (web, database, devices). Nothing in an inner ring may name anything declared in an
outer ring, and data formats produced by outer-ring frameworks must not leak inward. Data
crosses boundaries as simple structures (DTOs, function arguments), not entities or
database rows. Control flow may go outward at run time: the inner layer defines an interface
that the outer layer implements (dependency inversion), so dependencies still point inward.
Claimed benefits: framework independence, testability without UI or database, replaceable UI
and database.

## Use when

- Setting the dependency direction for a new codebase or a package structure.
- A layer has upward or cyclic dependencies and changes ripple.
- You want tests that run without a UI or database (inward rule plus ports).

## Do not use when

- The app is small and short-lived: two or three layers as folders are enough; ring
  ceremony (interfaces at every boundary) costs more than it saves.
- You would split teams or the whole system by layer only; Fowler warns this creates
  friction. Prefer vertical, domain-oriented top-level modules.
- Strict layering forces pass-through methods that add no logic in most call chains; relax
  to skip-layer calls where justified, accepting the extra coupling the SEI report describes
  for relaxed layers.

## Trade-offs

| Choice | Gain | Cost |
|---|---|---|
| Strict layers (N calls N-1) | few dependency paths, easy layer replacement | pass-through code, some overhead |
| Relaxed layers | fewer indirections | more coupling to lower layers |
| Inward dependency rule with inverted interfaces | isolated domain, testable | more interfaces and mapping between ring models |

## Common mistakes

- Domain entities carrying ORM or framework annotations, which pull outer-ring names inward.
- A "shared" or "common" layer that everything depends on and that depends on everything.
- Layer per team, then features that need changes in every team.
- Confusing layers (logical dependency order) with tiers (physical deployment).

## Related

- Notes: `style-hexagonal`, `principle-coupling-cohesion`, `principle-information-hiding`,
  `principle-conways-law`, `style-layered`
- Skills: `hexagonal-ports-adapters`, `solid-dependency-inversion`, `software-architecture`
  (project example)
