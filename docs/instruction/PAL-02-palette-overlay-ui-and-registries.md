# Instruction — PAL-02: palette overlay + registries

## Approach

Depends on PAL-01. Reuse the single key-controller rule from CONS-01 (don't stack two controllers on one widget).
Ctrl+K as a window-level accel. Re-query command specs on every open. Keep all GTK in `src/ui/`.

## Pitfalls

- Restore previous focus on Esc/after run. Return `true` from key handlers only when consumed.
- Verify no existing accel/entry already uses Ctrl+K before wiring.
- Disabled actions must never fail silently — dim or hide.

## Do not touch

- Ranking code (PAL-01); `executeLine()`.
