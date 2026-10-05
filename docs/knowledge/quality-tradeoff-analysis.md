---
name: quality-tradeoff-analysis
description: How to reason about quality trade-offs in architecture - the first law of trade-offs, ATAM's risks, sensitivity points and trade-off points, tactics vs patterns, and a short procedure to record a decision
domain: software-architecture
tags: quality-attributes,trade-offs,atam,decision-making,sensitivity-point
apply_when: "two or more designs each favor a different quality, and you must choose and justify"
sources: "Kazman, Klein, Clements - ATAM (CMU/SEI-2000-TR-004), sections 3, 7 (risks, sensitivity and tradeoff points); Bass, Clements, Kazman - Software Architecture in Practice, 3rd ed., ch. 4.5 (tactics vs patterns); Azure Well-Architected Framework, Performance Efficiency tradeoffs; Google SRE book ch. 3; Richards and Ford - Fundamentals of Software Architecture (first law, via multiple book summaries)"
last_reviewed: 2026-09-21
confidence: high
---

# Quality trade-off analysis

## The principle

Richards and Ford's first law of software architecture: everything in software
architecture is a trade-off. If you think you found something that is not, you have not
looked hard enough. Hence the standard answer "it depends": on business drivers,
environment, budget, time, skills. The work is to say *on what* and to decide anyway.

## ATAM vocabulary (SEI)

- **Risk**: an architecturally important decision not yet made, or made without understood
  consequences (example: an OS portability layer whose contents are unknown). ATAM records a
  risk as the decision, the quality response affected with its consequences, and the rationale.
- **Non-risk**: a good decision that relies on assumptions which are often implicit. Record
  the assumptions: if they change, the decision must be re-justified.
- **Sensitivity point**: a parameter to which a measurable quality response is strongly
  correlated (example: throughput and availability both depend on one communication channel).
- **Trade-off point**: a parameter that hosts more than one sensitivity point whose qualities
  move in different directions (speeding up the channel raises throughput but lowers
  reliability, so its speed is a trade-off point).
- ATAM does not predict exact values. It finds where architectural decisions affect a quality
  so those decisions get deeper analysis or prototyping.

## Tactics vs patterns (SAiP)

A tactic targets one quality-attribute response and does not consider trade-offs; the
designer must weigh them. An architectural pattern bundles decisions, with trade-offs built
in. Choose tactics to move one response; choose patterns knowing the bundle.

## Known tensions with sources

| Pair | Example | Source |
|---|---|---|
| Performance vs security | dropping encryption or validation speeds a flow | Azure WAF |
| Performance vs reliability | consolidation and delayed scaling enlarge blast radius | Azure WAF |
| Performance vs cost | extra components, premium SKUs, test environments | Azure WAF |
| Reliability vs cost and speed | 100x cost per extra increment; slows features | Google SRE ch. 3 |
| Throughput vs reliability | faster channel, less reliable (ATAM example) | ATAM |

## Procedure

1. Write the competing scenarios (see `quality-scenarios-utility-tree`) with measures.
2. List candidate designs; for each, walk each high-priority scenario and note the
   outcome and the responsible decision.
3. Record risks, non-risks, sensitivity points and trade-off points.
4. Choose; write the decision, the qualities you gave up, and the trigger to revisit
   (record it as an architecture decision record, `evolve-adr` when available).
5. Prototype or measure the riskiest sensitivity point before committing.

## Use when

- The choice is hard to reverse, affects several teams, or several qualities pull apart.
- A stakeholder asks for two qualities that conflict (fast and cheap and always up).

## Do not use when

- The decision is cheap and reversible. Decide, note it in one line, and move on.
- As a way to avoid deciding. Analysis ends with a choice and a review trigger.
- To rationalize a decision made earlier; if the scenarios were written after the choice,
  say so.

## Common mistakes

- Comparing options on features instead of scenarios.
- Hiding the cost of a choice ("no downside").
- Analyzing one quality at a time: optimizing a single attribute ignores the others
  (the ATAM authors' own warning).
- Treating "it depends" as the final answer.

## Related

- Notes: `quality-scenarios-utility-tree`, `quality-overview-25010`, `quality-performance`,
  `quality-availability-reliability`, `quality-modifiability`, `evolve-adr`, `evolve-architecture-review-atam`
