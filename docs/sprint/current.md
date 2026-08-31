# Current sprint

## Sprint 8

**Goal:** Engine-log sticky-bottom + About-window rewrite. Fix the Engine Log so it follows the
newest streamed line during analysis (sticky-bottom, without yanking a user who has scrolled up),
and replace the stock `Gtk::AboutDialog` with a deliberate custom layout carrying developer credit,
tech/build info, and links — showing the correct app name `RANLS`.
**Dates:** 2026-08-31 to — (open — no fixed end date set yet)

**Dependency graph:** UI-10 and UI-11 are independent of each other. UI-10 lives in
`src/ui/bottom_panel.cpp` (`flushPending` / `isScrolledToBottom` / `scrollToEnd`, same path as
RT-02). UI-11 adds a new `src/ui/about_dialog.{h,cpp}` class and must keep `APP_VERSION`
single-sourced (must not regress REL-02); the app-wide `Rapfi Analysis → RANLS` rename is
explicitly out of scope (filed separately as NAME-01).

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| UI-10 | Engine Log doesn't stay scrolled to the end while the engine is analysing — new streamed lines land off-screen | — | — | ✅ landed on branch (manual live-engine check pending) |
| UI-11 | Rewrite the About window: custom layout (logo + info column), developer credit, tech/build info, links & protocol; correct app name to `RANLS`; keep `APP_VERSION` single-sourced (REL-02) | REL-02 (done) | — | Active |

Points not yet estimated (consistent with Sprints 3–7).

**Lesson carried in from Sprints 6–7:** whenever a reported defect is about what the user sees on
screen, reach for the `rapfi-gui-ui-tests` target (links gtkmm, asserts the rendered widget tree) —
`rapfi-gui-tests` links no gtkmm and structurally cannot see widget-level bugs. Relevant to both
UI-10 and UI-11.

**Lesson carried from Sprints 4–7:** update `docs/sprint/burndown.md` as soon as an Active item's
status changes, and close the sprint as soon as its last item lands ✅.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
