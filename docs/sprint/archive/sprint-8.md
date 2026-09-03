# Sprint 8 (closed 2026-09-03)

**Goal:** Engine-log sticky-bottom + About-window rewrite. Fix the Engine Log so it follows the
newest streamed line during analysis (sticky-bottom, without yanking a user who has scrolled up),
and replace the stock `Gtk::AboutDialog` with a deliberate custom layout carrying developer credit,
tech/build info, and links — showing the correct app name `RANLS`.
**Dates:** 2026-08-31 to 2026-09-03.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| UI-10 | Engine Log doesn't stay scrolled to the end while the engine is analysing — new streamed lines land off-screen | ✅ FIXED |
| UI-11 | Rewrite the About window: custom layout (logo + info column), developer credit, tech/build info, links & protocol; correct app name to `RANLS`; keep `APP_VERSION` single-sourced (REL-02) | ✅ DONE |
| UI-12 | Move Log doesn't auto-scroll to the newest move — same `Gtk::Overlay`-breaks-`Gtk::Scrollable` no-op UI-10's second pass fixed for the Engine Log | ✅ FIXED |

UI-12 was pulled from Backlog into Active mid-sprint (2026-08-31) once UI-10's second pass surfaced
the same latent no-op in the Move Log. Points were never estimated this sprint (same as Sprints 3–7).

## What shipped

- **UI-10** (PR #2 `c9248e6`, then PR #4 `5b0bb13`): PR #2's persistent-mark scroll was still a
  silent no-op — the Engine Log `TextView` sat inside `EmptyStateOverlay` (a `Gtk::Overlay`, not
  `Gtk::Scrollable`), so GTK interposed an implicit `GtkViewport` that swallowed every `scroll_to`.
  PR #4 made `scrolledEngineLog_.set_child(engineLogView_)` direct and added a `programmaticScroll_`
  guard on the stick-tracking handler. Decision logic is pure in `src/ui/sticky_scroll.h`; the
  regression tests were upgraded from pure-helper cases to real-widget behavioural cases
  (`test_ui10_engine_log_scroll_target.cpp`, all fail pre-fix).
- **UI-11** (PR #6 squash `b9cacfd`): stock `Gtk::AboutDialog` → custom `AboutDialog` class
  (`src/ui/about_dialog.{h,cpp}`) — two-pane layout (themed stock icon + info column), `RANLS`
  heading, `Version <APP_VERSION>`, developer credit (Nguyen Minh), a tech/build-info grid
  (GTK/gtkmm/Cairo runtime versions, build date, git commit, license) and clickable repo +
  Gomocup/Yixin protocol links. New `src/build_info.h.in` → `build/generated/build_info.h`
  (`APP_BUILD_DATE`, `APP_GIT_COMMIT`, guarded to `"unknown"` for a no-`.git` build). `APP_VERSION`
  still the single CMake `project(VERSION)` literal (no REL-02 regression). +3 real-widget
  regression cases incl. an Adwaita/Adwaita-dark build.
- **UI-12** (PR #5 squash `a47e343`): `scrolledMoveLog_.set_child(moveLogView_)` direct + non-static
  `scrollMoveLogToEnd()` (persistent right-gravity `moveLogEndMark_`, re-issued once on idle so a
  burst append — e.g. a saved-game replay — doesn't land short). Removed the now-dead
  `moveLogOverlay_`/`engineLogOverlay_` members, `updateMoveLogEmptyState()`/
  `updateEngineLogEmptyState()`, and the unused `empty_state.h` include. +2 real-widget regression
  cases. "Don't yank a scrolled-up user" deliberately out of scope (burst-append log).
- Side fix (PR #3 `1b31861`, no CODE): Engine Log gutter tags now paint in the log `TextView`'s own
  resolved font instead of a hardcoded `"Monospace 11"`.

## Lessons

- Carried from Sprints 4–7 and reinforced hard this sprint: **a defect about what the user sees on
  screen needs a real-widget test** (`rapfi-gui-ui-tests`, links gtkmm, asserts the rendered tree).
  UI-10's PR #2 shipped green with only pure-helper unit tests and a wrong GTK assumption
  (`Gtk::Overlay` child ⇒ implicit viewport) went straight through — cost a full reopen + refix.
- Carried from Sprints 4–7: update `docs/sprint/burndown.md` as soon as an Active item's status
  changes, and close the sprint as soon as its last item lands ✅.
- New: **every "sticky-bottom" scroll in this codebase must scroll a `Gtk::Scrollable` directly** —
  if a `TextView`/`ListView` is wrapped in a non-scrollable container as a `ScrolledWindow` child,
  GTK silently interposes a viewport and all `scroll_to` calls become no-ops. UI-10 and UI-12 were
  the same bug; audit any future scroll-follow code against this.
- Manual live-engine streaming verification for UI-10/UI-12 was never run — no engine binary on the
  build host. The scroll plumbing rests half on GTK4 API semantics (unit-tested) and half on real
  streaming behaviour (not). Flagged in both fix-log details.

## Rolled over to Backlog

Nothing rolled over — all three committed items finished.

## Next sprint

Sprint 9 (goal "WinGraph coverage + app-wide RANLS rename") pulls UI-13 and NAME-01 from Backlog
into Active — see `docs/sprint/current.md`. Release `v0.1.1` cut at close (fix/polish bundle:
Engine Log + Move Log auto-scroll, About window rewrite).
