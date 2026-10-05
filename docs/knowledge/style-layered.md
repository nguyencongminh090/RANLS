---
name: style-layered
description: Layered (n-tier) architecture - horizontal layers with closed or open access, layers of isolation, the sinkhole warning, layers vs tiers, and what its scorecard says about agility, scaling and testing
domain: software-architecture
tags: architecture,style,layered,n-tier,layers-of-isolation
apply_when: "starting a conventional business application, migrating a layered system, or reviewing whether a layered design is a good default for the quality goals at hand"
sources: "Richards - Software Architecture Patterns, O'Reilly report, 2015, ch. Layered Architecture and Pattern Analysis (read, official O'Reilly page via web archive copy); Microsoft - Azure Architecture Center, N-tier architecture style and Architecture styles overview (read); Bachmann, Bass, Nord - Modifiability Tactics, CMU/SEI-2007-TR-002, section 8 (read); Fowler - Presentation Domain Data Layering (read)"
last_reviewed: 2026-09-21
confidence: high
---

# Layered (n-tier) architecture

## Intent

Split the application into horizontal layers, each with one kind of responsibility, so a
change stays inside a layer and each layer can be tested with the others replaced by
stubs. Richards calls it the most common pattern and a sensible starting point when you are
not sure which style fits; it matches how many companies split skills (UI, business,
database), which is Conway's law at work.

## Structure

- Typical layers (Richards): presentation, business, persistence, database. Small apps may
  merge business and persistence into three layers; larger ones have five or more.
- **Closed layer**: a request must pass through the layer directly below. This gives *layers
  of isolation*: a change in one layer does not ripple into others (Richards' example:
  swapping the UI framework leaves the business layer untouched if the contract between
  them stays the same).
- **Open layer**: requests may skip it. Richards' example is a shared-services layer placed
  below the business layer so the presentation layer cannot use it, but marked open so the
  business layer can still reach persistence directly. Document which layers are open and
  why; undocumented openness produces brittle, tightly coupled code.
- Azure describes the same choice as closed vs open layer architecture and, for tiers, strict
  vs relaxed communication: strict means more latency and overhead, relaxed means more
  coupling and harder change.
- **Layers vs tiers**: layers are logical groupings and a dependency order (higher layers use
  lower ones, not the reverse); tiers are physical separations on different machines. You may
  host several layers in one tier.

## Scorecard (Richards, typical implementation)

Ratings describe the natural tendency of a typical implementation, one author's judgement,
not a measurement.

| Characteristic | Rating | Reason given |
|---|---|---|
| Overall agility | Low | usually monolithic, components tightly coupled |
| Ease of deployment | Low | a small change may redeploy most of the application |
| Testability | High | other layers can be mocked or stubbed |
| Performance | Low | requests cross many layers |
| Scalability | Low | coarse granularity; replicate the whole app or whole layers |
| Ease of development | High | well known, low complexity |

## Use when

- A conventional business application with a modest domain, a mixed-skill team and no strong
  scalability or independent-deployment goal.
- Migrating an existing layered on-premises application with minimal change (Azure lists
  this as a main use).
- Requirements are still evolving and you need a cheap, understood starting structure.

## Do not use when

- Independent deployment or fine-grained scaling of features is a top goal; the style leans
  monolithic even if UI and business layers are deployed separately (Richards).
- Most requests are pure pass-through (see below); layers then add code and latency but no
  isolation.
- You would organize teams by layer for a feature-heavy product: each feature then needs
  every team (see `principle-conways-law`, `principle-layering-dependency-rule`).
- Latency-critical paths where several hops per request are not acceptable.

## Sinkhole warning

Richards' *architecture sinkhole anti-pattern*: requests pass through layers that add no logic
(the middle tier only does CRUD, which Azure also lists as a challenge). His rule of thumb
is that about 20 percent pass-through and 80 percent real logic is normal; if the ratio is
reversed, consider opening some layers, accepting weaker isolation. The 80/20 split is a
heuristic from one practitioner, not a measured threshold.

## Trade-offs

| Choice | Gain | Cost |
|---|---|---|
| Closed layers | isolation of change | pass-through code, extra latency |
| Open layers | fewer hops | more coupling to lower layers |
| Separate tiers | independent scaling and security boundaries per tier | network latency, more to operate |
| Layer-oriented top-level modules | familiar, matches team skills | features cut across all layers; Fowler advises domain-oriented top-level modules with layering inside |

## Common mistakes

- Calling the presentation layer straight into persistence "for speed", then discovering SQL
  changes break screens.
- A shared layer that every other layer depends on and that depends on all of them.
- Equating tiers with layers and paying network cost for no scaling benefit.
- Treating the style as neutral: it fixes a dependency order and a team split.

## Related

- Notes: `principle-layering-dependency-rule`, `style-hexagonal`, `style-modular-monolith`,
  `style-choosing`, `principle-conways-law`
- Skills: `software-architecture` (project-specific layering example)
