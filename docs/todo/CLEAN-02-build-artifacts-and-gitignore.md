# CLEAN-02 — Uncommitted build.sh mode change and ungitignored build output

**Status:** ✅ DONE
**Area:** build/repo hygiene
**Priority:** P4
**Source:** surfaced during a 2026-08-30 leftover-task sweep of `git status`

## Context

- `build.sh` has an uncommitted mode change (`100644` → `100755`, i.e. it was made executable but
  the mode-bit change was never committed).
- `build/` (directory) and `build_dist/rapfi-gui.settings` are untracked, and neither is covered by
  `.gitignore` — only `build_cmd/` is currently ignored. If these are build-output paths (same
  pattern as `build_cmd/`), they should be ignored too; if `build_dist/rapfi-gui.settings`
  specifically is meant to be a tracked template/sample file, it should be added instead of left
  perpetually untracked.

## Scope boundary

- Confirm what `build/` and `build_dist/` actually are (build-output dirs vs. something meant to be
  tracked) before deciding gitignore vs. commit — don't blindly ignore something that should ship.
- Commit the `build.sh` executable-bit change if it's intentional (it likely is — a build script
  usually should be executable).
- Out of scope: any broader build-system changes.

## Acceptance criteria

- `git status` is clean with respect to these three items: either committed or gitignored,
  deliberately, not just left sitting in the working tree.

## Related

- None.

## Summary

**Committed:**
- `build.sh` executable mode change (100644 → 100755) — the file has a shebang and is meant to be executable

**Gitignored:**
- `build/` — CMake build output directory containing CMakeCache.txt, CMakeFiles, build.ninja, compile_commands.json, etc.
- `build_dist/` — Build output directory containing compiled executables, generated settings (rapfi-gui.settings), and CMake artifacts

**Final status:**
- `.gitignore` updated to add `build/` and `build_dist/` alongside the existing `build_cmd/` entry
- `git status` is clean: no untracked build directories, no uncommitted mode changes on build.sh
