# UI-19: best-move ring covered the winrate tag text (2026-10-07)

**Report:** "The blue ring of best move covered text (winrate percent)."

**Root cause:** `BoardRenderer::drawSearchOverlay` drew the cyan best ring (UI-18) at
`rTag + max(1.5, 0.05*cell)`, sized from the tag *disc* only. The label is drawn at
`0.32*cell` font size and a wide label (e.g. "100.0%") is wider than the disc
(diameter `0.48*cell`), so the ring — drawn after the text — crossed the label ends.

**Fix:** record the label's half-diagonal while drawing the tag and use
`max(rTag, halfDiagonal) + gap` as the ring radius. Single change, `src/ui/board_renderer.cpp`.

**Regression test:** `tests/test_ui19_best_ring_clears_tag_text.cpp` (real Cairo render; counts
near-white label pixels with/without the best flag). Failed before the fix (6 vs 8), passes after.
Release ctest 5/5.

**Not verified:** in the live window (offscreen render only).
