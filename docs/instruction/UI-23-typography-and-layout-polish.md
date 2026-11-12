# UI-23 — how

- `ui` layer only (`src/ui/`, `src/resources/style.css`); no model/engine change.
- No bundled font files: that would be an asset-pipeline change (size L). Use CSS stacks with generic fallbacks.
- No CSS `line-height` on `textview`: the Engine Log gutter (`BottomPanel::drawGutter`) measures the font,
  not line spacing, and would drift.
- Do not switch stone numbers / overlay tags to Pango: `test_ui20_stone_number_centered` pins the toy-API
  ink-box centring. Coordinate labels only change family.
- A height-for-width child in the analysis panel requires BoardView to keep a non-zero minimum size
  (GtkPaned under-measures the end child by 1px when the start child's minimum is 0).
