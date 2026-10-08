# Current sprint

## Sprint 26

**Goal:** Harden the About dialog's markup against bad translations, and visually verify the Vietnamese UI.

**Dates:** 2026-10-08 to — (open — no fixed end date set yet)

**Dependency graph:**

- **I18N-05** — `ui` layer: `src/ui/about_dialog.cpp` + `tests/test_ui11_about_dialog.cpp`. Must not touch the plain-text path, other About sections or `vi.tsv`. Hardening (no live defect), but failing-test-first like UI-22. No design call.
- **I18N-06** — verification only (Xvfb, `GDK_BACKEND=x11`, screenshots). Must not fix anything it finds; defects become new CODEs. Independent of I18N-05; best run after it so the About dialog is covered.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| I18N-05 | Escape translated text before Pango markup in `makeValueLabel` | — | — | ✅ Done (PR #65) |
| I18N-06 | Visual verification pass of the Vietnamese UI | — (after I18N-05 preferred) | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–25).

**Lesson carried in from Sprint 25:** Run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset; re-run build, ctest and the benchmark on an agent's branch instead of trusting its report. A bug found during feature work is cheapest filed as its own CODE immediately.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `.claude/rules/sprint-cadence.md`.
