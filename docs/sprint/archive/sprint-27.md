# Sprint 27 (closed 2026-11-12)

**Goal:** Fix remaining vi UI gaps
**Dates:** 2026-10-08 to 2026-11-12.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| I18N-07 | Confirm dialogs show English Yes/No under `vi` | ✅ FIXED |
| UX-09 | Settings Apply disabled on every tab while the engine path is invalid | ✅ FIXED |
| UI-23 | Typography and layout polish | ✅ DONE |
| UI-24 | Engine Log gutter clipped the "MESSAGE" tag | ✅ FIXED |
| PROTO-09 | Stable realtime winrate on the board in every Rapfi message mode | ✅ FIXED |

I18N-07 and UX-09 were committed at sprint open; UI-23, UI-24 and PROTO-09 were pulled in mid-sprint
on user request (2026-11-11/12). Untracked board-padding / collapsible-panels work also landed as PR #66
(`d951543`, no CODE; note promoted to `docs/audit/2026-11-log.md` at this close). Points not estimated
(consistent with Sprints 3–26).

## What shipped

- **UI-23** (PR #67 squash `76bb3ab`): CSS type roles (em scale, tabular numerals, monospace only for
  logs), wrapping engine stats, Pango win-graph labels, UI font for board coordinates. Two in-branch
  fixes: a GtkPaned 1px under-measure warning (BoardView 240px minimum, `test_ui23_paned_min_measure.cpp`)
  and a leftover `.monospace` rule overriding the log font stack (`test_ui23_monospace_stack.cpp`).
- **UI-24** (PR #68 squash `0bb5cbb`): the Engine Log gutter clipped "MESSAGE" because
  `set_size_request` includes CSS padding+border; now `set_content_width`. Regression
  `test_ui24_gutter_tag_fits.cpp`.
- **PROTO-09** (PR #69 squash `7a1f06e`): realtime winrate/PV flicker fixed — one PV-round model for
  INFO+MESSAGE (no PV-list collapse per depth), MCTS ucilike fields parsed (draw rate `d` no longer read as
  depth), round-based stale-tag cleanup, learned cp scale, eased board overlay. +20 regression cases
  (`test_proto09_stable_rounds.cpp`, `test_proto09_overlay_animator.cpp`).
- **I18N-07** (PR #70 squash `f400aa0`): the discard-game confirmation used GTK stock `YES_NO` labels;
  now built by `makeDiscardConfirmDialog` (`src/ui/confirm_dialog`) with `add_button(tr("No"/"Yes"))`,
  same responses, no default. vi `Có`/`Không`. Regression `test_i18n07_confirm_buttons.cpp`. Follow-up
  not fixed: `showErrorDialog` still uses stock `OK`.
- **UX-09** (PR #71 squash `eb5c42e`): design call option (a) — Settings Apply always enabled; with an
  invalid engine path it applies everything else and keeps the saved path, tooltip explains. Regression
  `test_ux09_apply_invalid_engine_path.cpp` (3 cases). Known limitation: the engine path cannot be
  cleared via the dialog.

## Lessons

- Re-run build + ctest and look at the screenshots on an agent's branch instead of trusting its report;
  run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset.
- GTK stock button labels (`ButtonsType::YES_NO`, `OK`) bypass the app catalog and carry a mnemonic
  `_`; every user-visible label must go through `i18n::tr`.
- Write the `docs/instruction/` file when a task is filed: I18N-07 and UX-09 had none, so
  `/implement-task` stopped until one was written.
- In GTK4 a size request includes CSS padding/border; use `set_content_width` when drawing to a
  measured width.
- Board sync is owed: the `gh` token lacks `read:project`, so cards for PRs #67–#71 were not moved.

## Rolled over to Backlog

Nothing rolled over — all committed items finished. Backlog holds PROTO-08 (design gate, Q10–Q18 open).

## Next sprint

Sprint 28 — run `/sprint open 28 …` to commit Backlog items. Release `v0.12.0` cut with this close.
