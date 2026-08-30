# Current sprint

## Sprint 5

**Goal:** Clear the leftover items surfaced by a 2026-08-30 sweep of the codebase and `docs/notes/`
after closing Sprint 4: an unimplemented Load/Save Game feature, a stale README claim, an
unfinished tooling follow-up, and uncommitted/ungitignored build-tree state.
**Dates:** 2026-08-30 to — (open — no fixed end date set yet)

**Dependency graph:** all four items are independent of each other — no item's detail file declares
a blocking dependency on another. TOOL-01 is the exception to note: its own scope boundary says it
cannot be completed by an agent editing hook config directly (needs the user's explicit action), so
treat it as blocked-on-user rather than freely dispatchable like the other three.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| IO-01 | `onLoadGame()`/`onSaveGame()` are empty stubs — Load/Save Game silently do nothing | — | — | Active |
| DOC-01 | README.md claims GTK3; project actually targets GTK4 | — | — | Active |
| TOOL-01 | `check-tracking-sync.js` still isn't wired as a `Stop` hook | — | — | Active (needs user action) |
| CLEAN-02 | Uncommitted `build.sh` mode change; `build/`/`build_dist/` untracked and ungitignored | — | — | Active |

Points not yet estimated. Dispatch each with `/implement-task <CODE>` (except TOOL-01 — see above).

**Lesson carried over from Sprint 4** (see `docs/sprint/archive/sprint-4.md`'s "Process note"):
update `docs/sprint/burndown.md` as soon as an Active item's status changes, and close a sprint as
soon as its last item lands ✅ — don't let it sit open until an unrelated task prompts the review.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
