---
name: pattern-command
description: Command pattern (GoF behavioral) - turn a request into an object so it can be queued, logged, retried, undone or sent elsewhere
domain: software-design
tags: gof,behavioral,undo,queue
apply_when: "operations must be stored, queued, scheduled, logged, replayed, sent over a network or undone; or a sender must be decoupled from the code that performs the work"
sources: "Gamma et al. - Design Patterns (1994), Command (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); refactoring.guru - Command (fetched 2026-09-25, re-verified 2026-09-26: intent, applicability, pros, cons, relations); Wikipedia - Command pattern (read 2026-09-26: intent, components, uses, history)"
last_reviewed: 2026-09-26
confidence: high
---

# Command

## Intent

Wrap a request (what to do, on which receiver, with which arguments) in an object with a
single method such as `execute()`. The *invoker* holds and triggers commands without knowing
what they do; the *receiver* does the real work.

## Use when

- Undo and redo: keep a history of executed commands, each able to reverse itself (often
  with a saved snapshot of the previous state, see Memento).
- Queues, schedulers, job runners, macro recording: commands are stored and run later.
- Operations must be serialised and sent to another process or machine.
- One action is reachable from several places (menu, toolbar, shortcut) and should exist once.

## Do not use when

- A direct method call is enough and none of the above is needed; the extra layer is cost.
- The language has first-class functions and you only need to pass a callback: pass the
  function. Use command objects when you need history, undo, serialisation or naming.
- You want interchangeable algorithms for one job: that is Strategy.
- Reactions to an event should reach many unknown listeners: that is Observer.

## Trade-offs

| Gain | Cost |
|---|---|
| Sender decoupled from receiver | One class per operation; more code |
| Undo/redo, queues, logging, retry become possible | Undo must capture enough state, and correctness is on you |
| Commands can be composed into macros | An extra layer between UI and logic |
| New commands need no invoker change | Serialised commands need versioning if they outlive a release |

## Minimal example (Python)

```python
class InsertText:
    def __init__(self, doc, pos, text): self.doc, self.pos, self.text = doc, pos, text
    def execute(self): self.doc.insert(self.pos, self.text)
    def undo(self):    self.doc.delete(self.pos, len(self.text))

history = []
cmd = InsertText(doc, 0, "hi"); cmd.execute(); history.append(cmd)
history.pop().undo()
```

## Common mistakes

- Commands that keep no data needed to undo (or that undo by re-reading changed state).
- Commands holding UI widgets, so they cannot be queued or serialised.
- Putting all logic in the command instead of the receiver, leaving the receiver anemic.
- Building command classes for operations that never need history or queuing.

## Related

- Skills: `solid-open-closed`, `solid-single-responsibility`
- Notes: `pattern-strategy` (swap algorithm vs package a request), `pattern-observer`
