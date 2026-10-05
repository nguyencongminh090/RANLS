---
name: quality-performance
description: Performance efficiency as an architecture concern - how to characterize it (stimuli, latency and throughput responses), set targets, plan capacity, sustain it, and what it costs other qualities
domain: software-architecture
tags: quality-attributes,performance,latency,throughput,capacity
apply_when: "setting performance targets, choosing between designs with different latency or throughput profiles, or deciding whether an optimization is worth its side effects"
sources: "Kazman, Klein, Clements - ATAM (CMU/SEI-2000-TR-004), Appendix A.1; Bachmann, Bass, Nord - Modifiability Tactics (CMU/SEI-2007-TR-002), section 2 (queuing-model view of tactics); Azure Well-Architected Framework, Performance Efficiency design principles and tradeoffs pages; AWS Well-Architected Framework, performance efficiency design principles; ISO/IEC 25010:2023"
last_reviewed: 2026-09-21
confidence: high
---

# Performance

## Intent

Meet stated latency, throughput and capacity targets using resources efficiently, and keep
meeting them as load and code change.

## What it is made of

- ISO 25010:2023 "performance efficiency": time behavior, resource utilization, capacity.
- ATAM characterization: **stimuli** are events that start computation (messages,
  interrupts, keystrokes), described by mode (regular or overload) and arrival pattern
  (periodic, aperiodic, sporadic, random); **responses** are measurable, mainly latency
  (with best, average, worst case, jitter) and throughput; **architectural decisions** that
  affect them include resource arbitration and queuing policy, concurrency structure
  (processes, threads, processors), priorities and execution times.
- SEI view of tactics: performance can be modeled with a queuing model (arrival rate, queue
  size and discipline, service time, scheduling, routing, bandwidth). A tactic is a
  transformation that changes a model parameter, for example reducing service time by
  improving an algorithm, pre-allocating threads or database connections, or co-locating
  computations in one process to cut inter-process communication cost.

## How to apply (Azure guidance, checked against AWS)

1. **Negotiate realistic targets** with business stakeholders per critical user flow, not
   only technical metrics. You cannot measure what you have not defined, and you cannot
   define without measuring: iterate until the threshold is agreed.
2. **Design to capacity**: measure baselines early, choose and right-size resources, do
   capacity planning from a performance model, validate with a proof of concept.
3. **Sustain**: run automated performance tests in the pipeline, make them quality gates,
   monitor business transactions and technical metrics (CPU, latency, requests per second)
   with real and synthetic traffic.
4. **Optimize later, with data**: delay major optimizations until production data exists.
   AWS adds experimenting often and "mechanical sympathy" (match technology to access
   patterns).

## Use when

- Writing the response measure of a performance scenario (see
  `quality-scenarios-utility-tree`).
- Comparing designs by latency or throughput, or sizing capacity.
- Reviewing a proposed optimization for side effects.

## Do not use when

- You have no target and no measurement. Tuning without either is guesswork; measure first.
- Fine-tuning components in early design. Azure advises examining the system as a whole and
  avoiding granular tuning, which creates trade-offs elsewhere.
- Adding scale-out or caches for a bottleneck you have not located (see
  `quality-scalability`).

## Trade-offs (Azure, performance efficiency vs other pillars)

| Optimization | Cost to another quality |
|---|---|
| Delay scaling, consolidate, scale down | less redundancy, larger blast radius (reliability) |
| Autoscaling, sharding, denormalization, caches, message buses | more moving parts and consistency work (reliability, operations) |
| Remove encryption, scanning, validation or firewall rules | weaker security controls |
| Long cache TTLs, CDN | stale authorization or data (security, correctness) |
| More components, premium SKUs, perf test environments | higher cost |
| Less telemetry to save time | reduced observability |

## Common mistakes

- Optimizing average latency when the requirement is a tail or worst-case bound.
- Performance tests only before launch; regressions arrive with later features.
- Treating overload as a failure mode to ignore: ATAM lists overload as a distinct stimulus
  mode, so specify behavior under it.

## Related

- Notes: `quality-scalability`, `quality-scenarios-utility-tree`, `quality-tradeoff-analysis`
- Notes (software-design): `cleancode-code-smells`
