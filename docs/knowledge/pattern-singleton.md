---
name: pattern-singleton
description: Singleton pattern (GoF creational) - one instance with global access; mostly a liability today, with safer alternatives
domain: software-design
tags: gof,creational,global-state,anti-pattern,testing
apply_when: "you are tempted to make a class a global single instance (config, logger, connection pool, cache) or are reviewing code that does"
sources: "Gamma et al. - Design Patterns (1994), Singleton (read 2026-09-26 in the owner's copy: Intent, Applicability, Consequences, Related Patterns); refactoring.guru - Singleton (fetched 2026-09-25, re-verified 2026-09-26: intent, applicability, pros, cons, relations); Wikipedia - Singleton pattern (read 2026-09-26: intent, criticisms, lazy initialisation, dependency injection alternative)"
last_reviewed: 2026-09-26
confidence: high
---

# Singleton

## Intent

Guarantee that a class has exactly one instance and give everyone a global way to reach it
(a private constructor plus a static `instance()` accessor, created lazily).

It solves two separate problems at once: "only one" and "reachable from anywhere". Almost
all of its problems come from the second one.

## Use when

- The one-instance rule is a real constraint of the domain or the hardware (one handle to a
  device, one process-wide registry) and duplicates would be wrong.
- The language or framework already provides it cleanly (module-level object in Python,
  a DI container's singleton scope), so you do not hand-write the pattern.

## Do not use when

- You just want convenient access to a shared object. Create it once at startup and pass it
  to the code that needs it (dependency injection). This is the default choice.
- Tests need to substitute or reset the object: a global instance carries state between tests
  and is hard to fake.
- Code runs on several threads and the instance is created lazily without care: a race can
  create two instances (use eager creation or a language-safe initialiser).
- The instance mutates shared state that many parts read: it hides coupling and makes
  behavior depend on call order.

## Trade-offs

| Gain | Cost |
|---|---|
| Single instance guaranteed | Hidden dependencies: the class signature does not show what it uses |
| Lazy creation, easy access | Global mutable state; hard to test and to parallelise |
| | Breaks single responsibility (lifecycle plus real job) |
| | Needs thread-safety care |

## Alternative (Python)

```python
# Instead of Config.instance():
config = load_config()            # built once in main()
service = Service(config)         # passed in, easy to fake in tests
```

## Common mistakes

- Using it as a global variable with a nicer name.
- Lazy initialisation with no lock in multithreaded code.
- Singletons that depend on other singletons; initialisation order becomes fragile.
- Refusing to reset it in tests, so tests pass or fail depending on order.
- Calling any single-instance object a Singleton: "created once by the app" is not the
  pattern unless global access is enforced.

## Related

- Skills: `solid-dependency-inversion`, `solid-single-responsibility`, `hexagonal-domain-isolation`
- Notes: `pattern-facade` (a facade is often single, but need not be global),
  `cleancode-code-smells`
