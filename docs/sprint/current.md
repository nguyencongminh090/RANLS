# Current sprint

## Sprint 7

**Goal:** UI polish + release prep. Close out the post-UI-review follow-ups (drop the empty-state
placeholder text, stop engine auto-play from silently reverting to Off, make the win-rate graph
readable and its SingleSide mode always-Black) and stand up user-facing versioning (a
"Keep a Changelog" `CHANGELOG.md` backfilled through Sprint 6, a release checklist, and a
single-sourced version string) so `v0.1.0` can be tagged.
**Dates:** 2026-08-31 to — (open — no fixed end date set yet)

**Dependency graph:** UI-08 is independent. ENG-02 builds on UI-06 (shipped Sprint 6) and touches
the engine-lifecycle / `MatchConfig` path. UI-09 revisits UX-06's WinGraph work (drops the
follow-engine-side coupling, WCAG line pass) — independent of the others but should land after any
UI-08 panel churn to avoid conflicts in the analysis panel. **REL-02 depends on REL-01** (REL-01
establishes the SemVer 0.x scheme + changelog that REL-02's version string points at), so REL-01 is
done first and REL-02 after it.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| UI-08 | Remove empty-state placeholder text; keep panels clean/empty (partial reversal of UX-01) | — | — | Active |
| ENG-02 | Interrupting engine auto-play reverts "Engine plays" to Off instead of staying on the assigned side | UI-06 (done) | — | Done |
| UI-09 | WinGraph SingleSide always Black (drop UX-06's follow-engine-side coupling); thicker, higher-contrast win-rate line (WCAG pass) | — | — | Done |
| REL-01 | Root `CHANGELOG.md` ("Keep a Changelog", SemVer 0.x), backfill Sprints 1–6, "cut a release" checklist, tag `v0.1.0`; doc/process only | — | — | Done |
| REL-02 | Single-source the version string (`configure_file` → `version.h`), wire into About dialog + a pre-GTK `--version` flag | REL-01 | — | Done |

Points not yet estimated (consistent with Sprints 3–6).

**Lesson carried in from Sprint 6:** whenever a reported defect is about what the user sees on
screen, reach for the `rapfi-gui-ui-tests` target (links gtkmm, asserts the rendered widget tree) —
`rapfi-gui-tests` links no gtkmm and structurally cannot see widget-level bugs. Relevant to UI-08
and UI-09.

**Lesson carried from Sprints 4–6:** update `docs/sprint/burndown.md` as soon as an Active item's
status changes, and close the sprint as soon as its last item lands ✅.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
