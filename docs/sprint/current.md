# Current sprint

## Sprint 27

**Goal:** Fix remaining vi UI gaps

**Dates:** 2026-10-08 to — (open — no fixed end date set yet)

**Dependency graph:**

- **I18N-07** — `ui` layer: `src/main_window.cpp` (the `Gtk::MessageDialog` with `ButtonsType::YES_NO`, ~line 849) + `src/resources/lang/vi.tsv` + a UI test. Must not touch the dialogs' message text or response semantics; other dialogs only if they use the same stock labels (list, don't expand). Failing-test-first. Vietnamese strings: Claude drafts, user reviews.
- **UX-09** — `ui` layer: `src/ui/settings_dialog.cpp` (`btnApply_` sensitivity = `enginePathValid_`) + test. **Design call needed from the user before coding:** (a) apply non-engine settings while the path is invalid, or (b) keep blocking but explain the reason on every tab. Must not weaken the guard for launching the engine or change the settings file format. Independent of I18N-07.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| I18N-07 | Confirm dialogs show English Yes/No under `vi` | — | — | 🔲 Not started |
| UX-09 | Settings Apply disabled on every tab while the engine path is invalid | — (user picks option a/b first) | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–26).

**Lesson carried in from Sprint 26:** A visual pass is worth doing even when tests are green: it found a half-translated dialog (stock GTK buttons) and a settings-flow trap that no unit test pins. Run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset; re-run build and ctest on an agent's branch instead of trusting its report.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `.claude/rules/sprint-cadence.md`.
