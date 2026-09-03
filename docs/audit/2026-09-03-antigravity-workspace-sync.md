# 2026-09-03 — Synchronize Antigravity workspace setup with Claude Code

## Decision

Harmonize the Antigravity agent harness (`.agents/`, `GEMINI.md`, skills, rules, lifecycle hooks) with the established Claude Code setup (`.claude/`, `CLAUDE.md`), establishing zero-drift parity while strictly preserving the boundary: **modify only Antigravity setup, do not touch Claude Code setup**.

Key changes:
- **Workspace Project Rules (`GEMINI.md`):** Created a root symlink `GEMINI.md -> CLAUDE.md`. Antigravity loads `GEMINI.md` alongside `AGENTS.md`, giving Antigravity full access to the project's Scrumban process model, CodeGraph guidance, 7-phase diagnosis pipeline, regression-test mandates, and GitHub workflow without duplicating content or causing drift.
- **Path-Scoped Rules (`.agents/rules/`):** Created `.agents/rules/tracking-files.md` as a relative symlink to `.claude/rules/tracking-files.md`, enforcing index + detail file sync rules (`TODO.md`, `instruction.md`, `docs/fix-log.md`, `docs/audit.md`) for Antigravity sessions.
- **Skills Parity (`.agents/skills/`):** Mirrored all 7 skills from `.claude/skills/` via relative symlinks into `.agents/skills/`:
  - `data-architecture`
  - `github`
  - `gtk-ui-design`
  - `perf-optimization`
  - `prompt-architect`
  - `software-architecture`
  - `ui-ux-review`
  (Joining `systematic-debugging`, which was already vendored under `.agents/skills/`).
- **Implement-Task Skill (`.agents/skills/implement-task/SKILL.md`):** Packaged the `/implement-task <CODE> [project-dir]` workflow from `.claude/commands/implement-task.md` into an Antigravity skill with YAML frontmatter, enabling slash-command invocation (`/implement-task`) and on-demand model activation in Antigravity.
- **Lifecycle Hooks (`.agents/hooks.json`):** Configured Antigravity lifecycle hooks with a `Stop` hook (`tracking-sync-guard`) powered by adapter `.agents/scripts/check-tracking-sync-hook.js`. It validates tracking file index/detail sync against `scripts/check-tracking-sync.js` before permitting agent turn completion.

## Rationale

Previously, Antigravity only discovered `AGENTS.md` (which covers only model tiers and subagent dispatch) and `systematic-debugging`. All other project skills, rules, tracking-file discipline, and the Stop hook were scoped only to Claude Code (`.claude/`, `CLAUDE.md`). By establishing symlinks from `.agents/` back to the canonical files, both agent harnesses operate on a single source of truth with zero maintenance overhead, while keeping the `.claude/` hierarchy completely untouched.

## Boundaries Honored

- **Strict Antigravity-only scope:** Zero modifications to `.claude/`, `CLAUDE.md`, or any Claude Code settings.
- **No changes to C++ source or build system:** `src/`, `tests/`, and `CMakeLists.txt` remain untouched.
- **Append-only audit log:** Single dated record added per `/CLAUDE.md` and `.agents/rules/tracking-files.md`.

## Follow-ups

- When new project skills or rules are added in either harness, use symmetric symlinking to keep both harnesses automatically aligned.
