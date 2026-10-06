# UI-18: best-move mark hidden whenever a winrate tag is shown (2026-10-06)

**Status:** Active
**Related:** UI-18 (audit: `docs/audit/2026-10-06-board-marks-ux-review.md`, finding 1)

## Prompt

User asked to start the board-UI bugs from UI-18. Symptom (from the code review, not yet seen on
screen): with winrate tags on, the best root move shows no "best" highlight.

## Root cause

`BoardViewModel::update()` resolved exactly one `SearchOverlayMark::Kind` per cell with the priority
`tag > lost > best > examined > examining`. The best root move almost always carries a winrate tag, so
it resolved to `Kind::Tag` and the `Best` disc/ring was never produced — "best" was modelled as a
rank competing with the tag instead of an attribute of the cell. Reproduced first with a failing
ViewModel test (3 `isBest` CHECKs false, 9 others passing).

## Fix

- `src/model/board_view_model.{h,cpp}`: `SearchOverlayMark::isBest` flag, set for the cell equal to
  `AnalysisOverlay::bestMove` (also for the "BEST arrived before any POS/tag" cell). `kind`
  resolution unchanged, so the tag text and every other mark behave as before.
- `src/ui/board_renderer.cpp` (`drawSearchOverlay`): when `isBest` and `kind != Best`, draw the same
  cyan ring just outside the tag disc. `Kind::Best` cells (no tag) still draw their own disc + ring.
- Left alone: palette, DB/variant marks, hover/ghost (UI-16), Cairo state (UI-17), redesign (UX-07).

## Verification

- Regression test: `tests/test_proto06_analysis_overlay.cpp` — "UI-18: best root move is flagged
  isBest even when it carries a winrate tag" (tagged best keeps `Kind::Tag` + label + `isBest`;
  non-best not flagged; still flagged with `showSearchWinrate` off; flagged with no map entry).
- Release (`./build.sh <dir>` + `ctest`): 5/5; `ranls-gui-tests` 246 cases, `ranls-gui-ui-tests`
  61 cases / 472 assertions (+1 / +12).
- The default `build/` dir is a Debug build; there `port02-style-css-bundled` fails because the
  binary embeds absolute source paths (the very thing that guard rejects) — unrelated to this change
  and green in Release.
- **Not verified:** the ring's appearance on a real board (no screenshot taken). Geometry is
  `rTag + max(1.5, 0.05·cell)`, line width `max(2, 0.07·cell)`; check visually before closing the
  UX-07 redesign.

## Knowledge

`docs/audit/2026-10-06-board-marks-ux-review.md`; `features/board-marks/planning.md` (Q1 — Best as a flag).
