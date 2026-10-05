---
name: cleancode-code-smells
description: Catalog of the 24 code smells from Fowler's Refactoring (2nd ed.) with the usual refactoring for each - use to review code and pick a targeted fix
domain: software-design
tags: clean-code,refactoring,code-smell,review,fowler
apply_when: "reviewing or refactoring existing code; code is hard to change, read or test; deciding which refactoring to apply"
sources: "Fowler - Refactoring 2nd ed. (2018), ch. 3 'Bad Smells in Code' and the refactoring catalog; Martin - Clean Code (2008), ch. 17 'Smells and Heuristics' (read 2026-09-26: G5 Duplication says repeated switch/if chains go to polymorphism and similar algorithms to Template Method or Strategy, G14 Feature Envy, G23 Prefer Polymorphism to If/Else or Switch/Case, F1 Too Many Arguments - agrees; note that Clean Code treats all duplication as something to remove, while the accidental-duplication caution here is a counterpoint not taken from it); re-verified 2026-09-26 against Refactoring 1st ed. (1999) ch. 3: 18 smells and their remedies match (2nd-ed names); 6 smells new in 2nd ed. (Mysterious Name, Global Data, Mutable Data, Repeated Switches, Loops, Insider Trading) not checked in a book"
last_reviewed: 2026-09-26
confidence: high
---

# Code smells (Fowler, 2nd edition)

## Intent

A smell is a surface symptom that *often* signals a deeper design problem. It is a prompt
to look, not a defect by itself. Each smell below maps to the refactorings Fowler usually
suggests.

## Catalog

| Smell | What you see | Usual refactoring |
|---|---|---|
| Mysterious Name | names that do not say what/why | Rename (Change Function Declaration, Rename Variable/Field) |
| Duplicated Code | same logic in 2+ places | Extract Function, Slide Statements, Pull Up Method |
| Long Function | needs comments to explain sections | Extract Function, Replace Temp with Query, Decompose Conditional |
| Long Parameter List | many params, often travelling together | Replace Parameter with Query, Preserve Whole Object, Introduce Parameter Object |
| Global Data | data changeable from anywhere | Encapsulate Variable |
| Mutable Data | state changed in scattered places | Encapsulate Variable, Split Variable, Replace Derived Variable with Query, Change Reference to Value |
| Divergent Change | one module changed for unrelated reasons | Split Phase, Extract Class, Move Function |
| Shotgun Surgery | one change touches many modules | Move Function/Field, Combine Functions into Class, Inline Function |
| Feature Envy | function uses another module's data more than its own | Move Function, Extract Function then move |
| Data Clumps | same 3-4 fields appear together | Extract Class, Introduce Parameter Object |
| Primitive Obsession | raw strings/ints for domain ideas (money, range) | Replace Primitive with Object, Replace Type Code with Subclasses |
| Repeated Switches | same switch/if-chain on a type in many places | Replace Conditional with Polymorphism |
| Loops | loops doing filter/map by hand | Replace Loop with Pipeline |
| Lazy Element | class/function that does too little | Inline Function, Inline Class, Collapse Hierarchy |
| Speculative Generality | hooks/params for needs that never came | Collapse Hierarchy, Inline Function/Class, Remove Dead Code |
| Temporary Field | field set only in some cases | Extract Class, Introduce Special Case |
| Message Chains | `a.b().c().d()` | Hide Delegate, Extract Function then move |
| Middle Man | class that only delegates | Remove Middle Man, Inline Function |
| Insider Trading | modules trading internals | Move Function/Field, Hide Delegate |
| Large Class | too many fields/responsibilities | Extract Class, Extract Superclass |
| Alternative Classes with Different Interfaces | same job, different signatures | Change Function Declaration, Move Function, Extract Superclass |
| Data Class | fields plus getters/setters, no behavior | Encapsulate Record/Collection, Move Function into it |
| Refused Bequest | subclass ignores inherited parts | Push Down Method/Field, Replace Subclass with Delegate |
| Comments | comment that excuses unclear code | Extract Function, Rename, Introduce Assertion |

## Use when

- Doing a code review or a refactoring pass on code you did not just write.
- A change is awkward and you need to name *why* (Shotgun Surgery, Divergent Change).
- Choosing among refactorings: start from the smell, then apply the listed move.

## Do not use when

- As a checklist to "fix everything found". Refactor only what blocks the current task;
  leave unrelated smells alone (boy-scout rule in small steps).
- On code without tests: first add characterization tests, then refactor.
- On throwaway or generated code.
- Treating a smell as proof of a bug or as a style veto; several are context-dependent
  (a Data Class is normal at a serialization boundary; a switch that appears once is fine).

## Trade-offs

- Naming smells gives reviewers a shared vocabulary and makes feedback specific.
- Over-eager smell removal produces needless indirection (creates Lazy Element and
  Speculative Generality, which are themselves smells).
- Some smells pull in opposite directions (Middle Man vs Message Chains): choose by
  which coupling costs more here.

## Common mistakes

- Refactoring and changing behavior in the same commit; keep them separate.
- Fixing a smell by moving it (splitting a Long Function into pieces with the same
  parameter list creates Long Parameter Lists).
- Removing duplication that is only accidental: two blocks that look alike but change for
  different reasons should stay separate.

## Related

- Skills: `clean-code-detect-smells`, `refactor-extract-method`, `refactor-rename`,
  `refactor-replace-conditional-with-polymorphism`, `clean-code-boy-scout-rule`
- Notes: `pattern-strategy` (fix for Repeated Switches when variants are behaviors)
