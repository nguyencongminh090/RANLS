# Instruction — ARCH-03

Detail: [docs/todo/ARCH-03-narrow-command-context-split-dispatcher.md](../todo/ARCH-03-narrow-command-context-split-dispatcher.md)

## Approach

Do after ARCH-01. Move per-group registration into functions; shrink the context last.

## Pitfalls

Some handlers may genuinely need process-level access (e.g. raw engine send) — add a controller method rather than keep `EngineProcess&`.

## Verification before done

`!help` output diff is empty; `test_cons*` green.

## Boundaries

No new commands.
