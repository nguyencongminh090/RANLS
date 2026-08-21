# 2026-08-21 — UX-04: board geometry unification + confirmed overflow fixes at board-size extremes

## Prompt

UX-04 was explicitly scoped as investigation-first: confirm or refute each concern read from the
code for board sizes 5–22 (the renderer was tuned for the 15×15 default), fix anything confirmed,
and unify the duplicated board-geometry computation regardless of what the investigation found.

## Method

Built the app (`bash build.sh`, clean) and ran it under Xvfb (`:99`, 1280×800) with `xdotool` driving
real mouse clicks and `ffmpeg -f x11grab` taking real screenshots — this is genuine screenshot
verification, not simulated. Forced the light theme with `GTK_THEME=Adwaita:light` since no desktop
session/portal was available to toggle it live. Screenshots are attached under
`docs/fix-log/assets/2026-08-21-ux-04/`.

## Findings — one verdict per "things to check" item

### 1. `kCoordMargin` duplication (`board_renderer.cpp:40` vs `board_view.cpp:6`)
**CONFIRMED** (as a latent-drift risk, not yet actually diverged): both files defined the same
`24.0` constant and the same cell-size/margin formula independently. Nothing enforced they stay
equal. **Fixed** — see "Unification" below.

### 2. Coordinate label font sizing at small cells (22×22)
**REFUTED at the small-cell end.** Screenshot at 22×22 (`board-22x22-dark.png`,
`board-22x22-light.png`) shows labels A–V / 1–22 fully legible, comfortably inside the 24px margin.
At cellSize≈24px the old formula `max(9.0, cellSize*0.35)` floors to 9.0, which fits.

**CONFIRMED at the large-cell end (not what the todo predicted, but the same root cause).** The old
formula had no *ceiling*. At 5×5 (cellSize≈106px), font size becomes `cellSize*0.35≈37px` — far
bigger than the fixed 24px margin. Screenshot `coordlabel-clip-before.png` shows column labels "A"/
"B" and row label "5" visibly clipped by the menu bar / left edge of the window; only the bottom
sliver of each glyph is visible above the board.

**Fixed**: `board_renderer.cpp` now uses `std::clamp(cellSize_ * 0.35, 9.0, 16.0)` for the coordinate
label font — a ceiling was added alongside the existing floor. Verified via
`coordlabel-clip-after.png`: labels A–E / 1–5 are now fully visible and legible in both light and
dark themes (`board-5x5-dark-fixed.png`, `board-5x5-light-fixed.png`).

### 3. Move-number text vs. stone radius (22×22, 3-digit move numbers)
**CONFIRMED.** Placed 105 stones on a 22×22 board (scattered pattern, `showMoveNumbers` on by
default) and captured `movenum-overflow-before.png`: move numbers 100–105 are visibly wider than
their stones, spilling into the wood background and over grid lines on both sides. The old formula
`max(8.0, cellSize_ * 0.45)` had no relationship to how many digits the label needed to fit.

**Fixed**: `drawStones()` now measures the label with `get_text_extents()` and shrinks the font
(down to a 6.0 floor) if the measured width exceeds `2 * stoneRadius() * 0.85`, so any digit count
fits inside the stone. Verified via `movenum-overflow-after.png`: numbers 98–105 all sit inside
their stones.

### 4. Candidate-move / database label collisions
**Computed, not screenshot-verified** — no engine binary is available in this sandbox to produce
real MultiPV/candidate-move or database-overlay analysis data, so `drawCandidateMoves()` /
`drawDatabaseMarkers()` never actually render in a live run here. By the same reasoning as #3
(measuring against `stoneRadius() * 0.55` and font `max(8, cellSize*0.28..0.32)`), these markers
scale roughly proportionally with their own (smaller) radius rather than the full stone radius, so
they are not obviously worse than the move-number case pre-fix, but this was **not directly
observed**. Filed as a follow-up to actually verify once a headless-attachable engine is available
(see Backlog note below) — no fix applied since no defect was confirmed.

### 5. Click-target size at 22×22 in a small window
No separate defect. Placing the 105 test stones produced a move log (`A22 B22 2. C22 D22 3. ...`)
that exactly matched the intended raster coordinates, confirming clicks land on the intended cell
even at the smallest cell size in the supported range. This is also now structurally guaranteed by
the geometry unification (#8): rendering and hit-testing share one calculation, so they cannot
silently disagree regardless of cell size.

### 6. No star points at 5×5
**REFUTED as a remaining gap.** PROTO-02 already generalized star-point drawing to
`bs % 2 == 1 && bs >= 9` (`board_renderer.cpp:123`), deliberately omitting them below 9×9 because
there isn't room for corner/center stars to stay visually distinct. Screenshots
(`board-5x5-dark-fixed.png`, `board-5x5-light-fixed.png`) confirm no star points at 5×5, and this is
correct/intended, not a bug.

### 7. Board size growing unboundedly at 5×5
**REFUTED.** `MainWindow::onBoardSize()` (`main_window.cpp:517-518`) hard-caps the spin button to
`[5, 22]`, matching `MAX_BOARD_SIZE = 22` (`board_state.h:7`). Not unbounded.

### 8. `BoardView::pixelToCoord` vs `BoardRenderer::draw` geometry duplication
**CONFIRMED** (same as #1 — this is the same duplication, viewed from the hit-testing side). Fixed
per "Unification" below, independent of whether size testing found visual bugs (as the todo file
required).

### Window resize behaviour (both ends) — bonus finding, filed separately
Not one of the enumerated "things to check" line items, but covered by "window resize behaviour at
both ends." Shrinking the window very small (500×400) squeezes the board pane down to a sliver (no
crash — GTK just allocates what's left). Growing the window back to 1280×800 **does not restore**
the board pane's size: the `Gtk::Paned` divider stays at its last absolute pixel position rather
than rescaling proportionally, so the board stays tiny until the user manually drags the divider
back. This is a `main_window.cpp` Paned-configuration concern, not a `board_renderer`/`board_view`
geometry defect, so it's out of scope for this fix — filed as **UX-05** in the Backlog instead of
bundled in here.

## Action — code changes

- **`src/ui/board_renderer.h`**: added public `BoardRenderer::Geometry` struct and
  `computeGeometry(int width, int height) const` — the single source of truth for cell size and
  margins, now the only place that formula is written.
- **`src/ui/board_renderer.cpp`**: `draw()` now calls `computeGeometry()` instead of inlining the
  formula. Coordinate-label font size gained a ceiling (`std::clamp(..., 9.0, 16.0)`). Move-number
  font size now shrinks to fit the stone when `get_text_extents()` measures it too wide.
- **`src/ui/board_view.cpp`**: removed its own `kCoordMargin` constant and the duplicated formula;
  `pixelToCoord()` now calls `renderer_.computeGeometry(width, height)` (the `BoardView` already
  owned a `BoardRenderer` instance as a member, so no new coupling was introduced).

This also satisfies CLEAN-01's "Duplicated constant" acceptance criterion ("`kCoordMargin` defined
once") — CLEAN-01 itself is not being closed here since its other items (dialog leaks, dead
`std::cerr`, unused local) are untouched and out of scope for UX-04.

## Verification

- `bash build.sh` — clean build, no new warnings, both binaries link.
- Manual run under Xvfb + xdotool + ffmpeg screenshots at 5×5, 15×15, 22×22, in both the default
  (dark, system `prefer-dark`) and a forced-light (`GTK_THEME=Adwaita:light`) theme — all 6 stored
  under `docs/fix-log/assets/2026-08-21-ux-04/`.
- Before/after crops for both confirmed defects (`movenum-overflow-*.png`,
  `coordlabel-clip-*.png`) show the fix taking effect.
- Confirmed geometry is now computed in exactly one place (`BoardRenderer::computeGeometry`) and
  both `BoardRenderer::draw()` and `BoardView::pixelToCoord()` call it.

## Follow-ups filed

- **UX-05** (Backlog): `Gtk::Paned` divider position doesn't rescale when the window is resized
  back up after being shrunk — board can stay squeezed indefinitely.
- Candidate-move / database-marker label sizing (item 4 above) should be re-checked with a real
  screenshot once a headless-attachable engine is available in a test environment; no action item
  filed separately since it's already covered by UX-04's own text and no defect was confirmed.
