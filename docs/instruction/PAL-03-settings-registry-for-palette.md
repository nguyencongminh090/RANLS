# Instruction — PAL-03: settings registry

## Approach

Behavior-preserving refactor first (table + open-at-page/focus-by-id), then register entries. Add a test that every
dialog setting has a table entry so new settings can't silently be unsearchable.

## Do not touch

- Settings semantics, defaults, persistence format.
