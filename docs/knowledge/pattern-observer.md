---
name: pattern-observer
description: Observer pattern (GoF behavioral) - a publisher notifies a changing set of subscribers about events without knowing who they are
domain: software-design
tags: gof,behavioral,events,decoupling
apply_when: "one object's change must trigger reactions in other objects whose identity or number is unknown or changes at runtime; GUI events; domain events"
sources: "Gamma et al. - Design Patterns (1994), Observer (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); refactoring.guru - Observer (fetched 2026-09-25, re-verified 2026-09-26: intent, applicability, pros, cons, relations); Wikipedia - Observer pattern (read 2026-09-26: intent, problems solved, lapsed listener, relation to publish-subscribe); University of Toronto CSC207 (Winter 2017) - Design Patterns lecture slides (read 2026-09-26: Observer problem statement and Java Observable versus event-model note)"
last_reviewed: 2026-09-26
confidence: high
---

# Observer

## Intent

A *publisher* keeps a list of *subscribers* that share a small interface (usually one
`update`/`on_event` method). When something interesting happens, the publisher calls each
subscriber. The publisher knows only the interface, never the concrete subscriber types.

## Use when

- A state change in one object must cause work in others, and the set of others is not
  fixed: UI widgets and their click handlers, model and views, domain events, plugins.
- You want to add reactions later without editing the publisher (open-closed).
- Subscribers must be attachable and detachable at runtime.

## Do not use when

- The publisher has one known consumer that never changes: call it directly.
- Order of reactions matters. Subscribers are notified in no guaranteed order, so hidden
  ordering dependencies turn into bugs. Use an explicit pipeline or Chain of Responsibility.
- You need a reply from the receiver, or guaranteed delivery, retries or persistence across
  processes: that is a message broker, not in-process Observer.
- Many objects need to talk to each other in both directions: the web of subscriptions gets
  hard to follow; a Mediator centralises it.

## Trade-offs

| Gain | Cost |
|---|---|
| Publisher is decoupled from subscribers | Control flow is implicit; hard to trace by reading code |
| Subscribers added or removed at runtime | Unpredictable notification order |
| New reactions need no publisher edit | Forgotten unsubscribe keeps subscribers alive (memory leak) |
| | A slow or failing subscriber can block or break the publisher unless isolated |

## Minimal example (Python)

```python
class Publisher:
    def __init__(self):
        self._subs = []
    def subscribe(self, fn):
        self._subs.append(fn)
        return lambda: self._subs.remove(fn)      # unsubscribe handle
    def publish(self, event):
        for fn in list(self._subs):               # copy: subscribers may unsubscribe
            fn(event)

p = Publisher()
off = p.subscribe(lambda e: print("got", e))
p.publish("saved")
off()
```

Where the language has signals, events or observables (Qt signals, C# events, RxJS), use
those instead of hand-rolling the list.

## Common mistakes

- Never unsubscribing short-lived subscribers, so they leak (strong references in the list).
- Mutating the subscriber list while iterating it.
- Subscribers that change the publisher and re-trigger notifications (event loops).
- Putting heavy work in the notification call; hand it to a queue instead.
- Relying on the order in which subscribers were registered.

## Related

- Skills: `hexagonal-ports-adapters`, `solid-open-closed`
- Notes: `pattern-strategy` (Strategy swaps one behavior; Observer fans out to many)
