# 2026-08-21 — RT-02: engine log grows unbounded and desyncs its gutter labels

**Task:** [docs/todo/RT-02-engine-log-unbounded.md](../todo/RT-02-engine-log-unbounded.md)

## Problem

`BottomPanel`'s Engine Log tab appended every engine line immediately with no cap, one GTK buffer
transaction per line, and a forced `scroll_to` per line — the second-largest realtime-lag
contributor after RT-01. Separately, the gutter (`[SEND]`/`[RECV]` labels, `WrapMode::NONE`) and
content (`WrapMode::WORD_CHAR`) were two different `TextBuffer`s synced only by pixel offset, so a
wrapped long RECV line permanently desynced every later label from its actual line.

## Fix

- Replaced the dual-TextView gutter+content pair with a single `Gtk::TextView` (`engineLogView_`)
  whose buffer inlines the colored prefix tag directly before the line text. One wrapped text flow
  cannot desync from itself, so this removes the gutter/content bug outright rather than patching
  the pixel-offset sync.
- Added `src/ui/engine_log_model.h` (`EngineLogModel`): a GTK-independent bounded `std::deque` (cap
  defaults to 5000 lines) that is the single source of truth for which lines survive.
- `appendSend`/`appendRecv` now only enqueue into `pendingAppend_`; a 50ms `Glib::signal_timeout`
  tick (`flushPending`) drains the queue in one `begin_user_action`/`end_user_action` transaction,
  pushes each line through `EngineLogModel::push` to get the authoritative drop count, and trims
  that many lines off the front of the GTK buffer to match — buffer and model can never disagree on
  line count.
- Auto-scroll only happens in `flushPending`, and only if `isScrolledToBottom()` was true *before*
  the insert — a user scrolled up to read earlier output is no longer yanked to the bottom.

Chose "single TextView with inline tagged prefix" over a `ColumnView` migration: the wrap-mode
mismatch, not the two-buffer design per se, was the actual cause of the desync, and a `ColumnView`
would still need its own answer for keeping wrapped-row heights aligned across columns — more
churn for the same fix.

## Files touched

- `src/ui/bottom_panel.h`, `src/ui/bottom_panel.cpp` — single-view rewrite, batching, cap, and
  conditional auto-scroll.
- `src/ui/engine_log_model.h` (new) — bounded, GTK-independent line-cap model.
- `docs/todo/RT-02-engine-log-unbounded.md` — status + resolution notes.
- `TODO.md` — marked `RT-02` done (added `✅` in place; it had been filed to the Backlog section in
  this worktree, not the Active section — see verification notes below on why sprint files here
  are behind).

## Verification

- `./build.sh` — clean build. Only pre-existing, unrelated `-Wunused-function` warnings in
  `gomocup_protocol.cpp`.
- `RUN_TESTS=1 ./build.sh` — this worktree has no test infrastructure at all: no `tests/`
  directory, no CMake test target, and `build.sh` does not read `RUN_TESTS`. `TODO.md`/
  `docs/sprint/current.md` in this worktree list `TEST-01` as "Active" but it has not actually been
  implemented/committed here (this worktree branched from `main` at `640a208`, before whatever
  uncommitted TEST-01/PROTO-01/STATE-01/RT-01 work exists elsewhere landed). Per `CLAUDE.md`'s
  bug-fix workflow, this is called out explicitly rather than skipped silently. No ad hoc test
  harness was built as part of this fix — that would preempt `TEST-01`'s own scope (framework
  choice, CMake wiring). `EngineLogModel` was deliberately written GTK-free so it can be dropped
  into `TEST-01`'s harness verbatim once it exists.
- Correctness of the cap logic was checked with a throwaway (not committed) local compile: fed
  12,000 lines into a cap-5000 `EngineLogModel` and confirmed size stays at 5000 with the
  surviving window being exactly the most recent 5000 lines; a cap-3 case confirmed exactly one
  line drops per push once full.
- **Not verified in this environment:** the task's own acceptance criterion — "run a deep search
  and confirm memory stays flat and the log stays responsive" — needs a live engine subprocess and
  a display server, neither available in this sandboxed session. The bound is enforced by
  construction (every push routes through `EngineLogModel`, which pops from the front once over
  cap; the GTK buffer is trimmed by the same count each flush) and batching demonstrably reduces
  per-line transactions to one transaction per 50ms tick, but live memory/responsiveness under a
  real deep search was not measured.
- **Post-merge addition:** `tests/test_engine_log_model.cpp` — a real, committed regression suite
  for `EngineLogModel` (cap enforcement, oldest-first drop, `setMaxLines` shrink, `clear`), added
  once `tests/` was reconciled onto `main`. Confirms the exact behavior the throwaway local check
  above only spot-verified: 12,000 pushes into a cap-5000 model leaves the most recent 5000 intact.

## Status

✅ FIXED — marked in `TODO.md` and `docs/todo/RT-02-engine-log-unbounded.md`.

## Scope notes

- Move Log tab (`appendMoveLog`) and the tag/color scheme were left untouched, per the task's scope
  boundary.
