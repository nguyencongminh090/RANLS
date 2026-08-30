# Sprint 5 (closed 2026-08-30)

**Goal:** Clear the leftover items surfaced by a 2026-08-30 sweep of the codebase and `docs/notes/`
after closing Sprint 4: an unimplemented Load/Save Game feature, a stale README claim, an
unfinished tooling follow-up, and uncommitted/ungitignored build-tree state.
**Dates:** 2026-08-30 to 2026-08-30 (opened and closed same day — all four items landed on `main`).

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| IO-01 | `onLoadGame()`/`onSaveGame()` were empty stubs — Load/Save Game silently did nothing | ✅ DONE |
| DOC-01 | README.md claimed GTK3; project actually targets GTK4 | ✅ DONE |
| TOOL-01 | `check-tracking-sync.js` wasn't wired as a `Stop` hook | ✅ DONE |
| CLEAN-02 | Uncommitted `build.sh` mode change; `build/`/`build_dist/` untracked and ungitignored | ✅ DONE |

Points were never estimated this sprint, same as Sprints 3 and 4.

## What shipped

- **IO-01:** New gtkmm-free `src/model/game_io.{h,cpp}` versioned `key=value` + `move=x,y` serializer
  (settings_storage style, no JSON); `Gtk::FileDialog` open/save pickers; discard-confirmation on
  Load; self-deleting ERROR dialog on corrupt input; model rebuilt via
  newGame/setRule/replay/sendConfig. See `docs/fix-log/2026-08-30-io01-load-save-game.md`.
- **DOC-01:** README.md corrected from GTK3 to GTK4. Commit `27b511c`.
- **TOOL-01:** `Stop` hook wired into `.claude/settings.local.json` running
  `check-tracking-sync.js --hook`; verified via manual run. Commit `420d664`.
- **CLEAN-02:** `build.sh` made executable; `build/` and `build_dist/` added to `.gitignore`.
  Commits `8941241`, `c841800`.

## Rolled over to Backlog

Nothing rolled over — all four committed items finished within the sprint.

## Process note

Followed Sprint 4's carried-over lesson: closed the sprint the same day its last Active item landed,
rather than leaving it open administratively.

## Next sprint

Sprint 6 pulls the four UI-review items filed 2026-08-30 (UI-04, UI-05, UX-06, UI-06) into Active —
see `docs/sprint/current.md`.
