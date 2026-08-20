---
description: Dispatch a subagent to implement a tracked TODO.md task end-to-end, following the project's V-Model + tracking discipline (see <project-dir>/CLAUDE.md).
argument-hint: <CODE> [project-dir]
allowed-tools: Read, Grep, Glob, Bash, Agent
---

## Inputs

Parse `$ARGUMENTS` as `<CODE> [project-dir]`:
- `CODE` — the task code, e.g. `WALL-01`. Required.
- `project-dir` — repo-relative directory holding `TODO.md`/`instruction.md`/`docs/`. Default:
  `Rapfi` if omitted.

## What to do

1. **Locate the task.** In `<project-dir>/TODO.md`, find the line referencing `CODE`. Read
   `<project-dir>/docs/todo/<CODE>-*.md` and `<project-dir>/docs/instruction/<CODE>-*.md`. If
   either is missing, or the code isn't in `TODO.md`'s Active section, **stop and report** — do
   not guess which task the user means or invent scope that isn't written down.
2. **Follow the linked design doc(s)** referenced from the todo/instruction files (e.g.
   `docs/design/*.md`) — read them too before dispatching, so the agent prompt below can cite
   concrete file:line anchors instead of vague instructions.
3. **Dispatch one Agent** (subagent_type: general-purpose, unless the todo file names a more
   specific one) with a self-contained prompt that includes:
   - The absolute paths of the todo/instruction/design files to read first, and an instruction to
     treat the instruction file's "Boundaries — do not touch" and "Pitfalls" sections as hard
     constraints, not suggestions.
   - The concrete scope steps from the todo file, in order.
   - The exact "Verification before marking this task done" criteria from the instruction file —
     the agent must run/confirm all of them itself (build, tests, self-play/regression check as
     specified) before reporting success. Passing unit tests alone is not sufficient if the
     instruction file specifies more tiers — say so explicitly in the dispatch prompt.
   - An instruction to update the todo file's `Status:` field (and the corresponding `TODO.md`
     line, moving it out of "Active" if fully done) once verification passes — but only if it
     actually passes; a partially-done task stays `Active` with a note on what's left.
   - An instruction to report back: what changed (files touched), what verification was actually
     run and its result, and anything it deliberately left out of scope per the boundaries.
4. **Do not perform the implementation yourself in this session** — the point of this command is
   to hand the bounded, already-designed task to a subagent so it runs isolated from this
   session's context. Launch it and let it run (background by default, per standard Agent tool
   behavior) rather than narrating what it will probably find.
5. When the agent's result lands, relay its actual findings to the user — do not summarize before
   it has responded, and do not fabricate verification results it hasn't reported yet.

## Notes

- This command assumes the target project already went through the discussion→design→todo
  formalization described in `<project-dir>/CLAUDE.md`. If `CODE` is still only a
  `features/<slug>/planning.md` entry with open questions, this command is the wrong tool —
  resolve planning first.
- Re-running this command on a `CODE` already marked done in `TODO.md` should ask for
  confirmation before dispatching again (likely means either a regression or a follow-up task
  deserving its own new `CODE`).
