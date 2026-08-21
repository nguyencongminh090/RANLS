# UX-04 — Board rendering never verified at the extremes of the supported 5–22 range

**Status:** ✅ DONE
**Area:** board renderer / board view
**Priority:** P3
**Source:** codebase review, 2026-08-21

## Context

`MainWindow::onBoardSize` allows 5–22 (`src/main_window.cpp:517-518`) and `MAX_BOARD_SIZE` is 22
(`src/model/board_state.h:7`), but the renderer's constants were tuned for the 15×15 default. The
`ui-ux-review` checklist calls for checking both ends of the range.

This item was scoped as **investigation first**. Full findings, screenshot evidence, and the code
changes are in the fix-log:
[docs/fix-log/2026-08-21-ux-04-board-geometry-and-overflow.md](../fix-log/2026-08-21-ux-04-board-geometry-and-overflow.md).
Screenshots are attached under `docs/fix-log/assets/2026-08-21-ux-04/`.

## Verdicts (see fix-log for full evidence/reasoning)

**At 22×22 (cells get small):**
- `kCoordMargin` duplication (`board_renderer.cpp:40` / `board_view.cpp:6`) — **CONFIRMED**
  (latent-drift risk, values hadn't yet diverged) — **fixed** via geometry unification.
- Coordinate label font at small cells — **REFUTED** at 22×22 (screenshot: labels fit fine, the
  existing floor clamps them). The real defect turned out to be the opposite extreme (see 5×5 below).
- Move-number text vs. stone radius — **CONFIRMED** (screenshot: 3-digit numbers 100-105 visibly
  overflow their stones on a 22×22 board) — **fixed** via measure-and-shrink-to-fit.
- Candidate-move / database label collisions — **not screenshot-verified** (no engine attachable
  in this sandbox to produce real analysis overlays); computed reasoning only, no defect confirmed,
  no fix applied. See fix-log for the reasoning.
- Click-target size at 22×22 — no separate defect; verified clicks land on the intended cell via a
  105-stone test placement, and this is now structurally guaranteed by the geometry unification.

**At 5×5 (cells get large):**
- No star points drawn — **REFUTED as a remaining gap**. PROTO-02 already generalized this to
  `bs % 2 == 1 && bs >= 9`, deliberately omitting stars below 9×9. Confirmed via screenshot this is
  working as intended, not a bug.
- Board size growing unboundedly — **REFUTED**. Hard-capped to [5, 22] in the size dialog, matching
  `MAX_BOARD_SIZE`.
- **New confirmed defect** (not explicitly predicted by the todo, but the same root cause as the
  22×22 label concern): coordinate label font size had no ceiling, so at 5×5 (large cells) it grew
  past the fixed margin and was visibly clipped by the menu bar. **Fixed** via
  `std::clamp(cellSize_ * 0.35, 9.0, 16.0)`.

**Both ends:**
- `BoardView::pixelToCoord` vs `BoardRenderer::draw` geometry duplication — **CONFIRMED**, **fixed**:
  both now call the single `BoardRenderer::computeGeometry()`.
- Window resize behaviour — shrinking then growing the window back does not restore the board
  pane's size (a `Gtk::Paned` divider-position issue in `main_window.cpp`, unrelated to board
  geometry). Filed separately as **UX-05** rather than fixed here (out of this item's scope).

## Acceptance criteria (met)

- ✅ Screenshots at 5×5, 15×15, 22×22, in both light and dark themes — real screenshots via Xvfb +
  xdotool + ffmpeg, attached under `docs/fix-log/assets/2026-08-21-ux-04/`. (Candidate-move/database
  label collisions could not be screenshot-verified — no engine available — computed reasoning used
  instead, clearly labeled as such in the fix-log.)
- ✅ Confirmed defects fixed (coordinate-label clipping at 5×5, move-number overflow at 22×22);
  everything checked-and-fine is recorded above so it isn't re-investigated.
- ✅ Board geometry computed in exactly one place (`BoardRenderer::computeGeometry`).

## Scope boundary

- Correctness of coordinate *values* at non-15 sizes is PROTO-02 (already done) — this investigation
  relied on that being correct rather than re-fixing it.

## Related

- PROTO-02 (hardcoded 15, done), UX-03 (contrast and colour-only meaning), CLEAN-01 (its
  "duplicated constant" item is satisfied by this fix's geometry unification), UX-05 (new, filed
  from this investigation's window-resize finding)
