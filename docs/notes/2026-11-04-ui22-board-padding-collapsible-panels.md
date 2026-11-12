# 2026-11-04 — UI-22: board padding/style + collapsible panels

**Size:** M (UI only, 2 layers touched: ui + main_window; reversible)
**Expires:** sprint close
**Related:** UI-22; src/ui/board_geometry.h, board_renderer.cpp, main_window.cpp, vi.tsv

- Coordinate margin 24→38 px, label gap 6→10 px (user: coords hugged the header bar). Wood now a rounded panel with soft shadow, vertical gradient, thin border (cosmetic; grid geometry unchanged, hit-testing shares `computeGeometry`).
- View menu: "Analysis Panel" / "Move/Engine Log Panel" bool actions hide/show the Paned end children. Session-only (not persisted — avoids a settings-schema change). Paned-fraction tracking/reapply skip a hidden panel so the saved divider fraction isn't corrupted.
- `test_ui20` measured ink *centroid* with an un-centred pixel index; changed to ink-bbox midpoint (what the renderer centres) because the margin change exposed ~0.5px measurement bias + glyph-shape bias. Threshold unchanged.
- Not done / ideas: keyboard shortcuts + palette entries for the toggles, persisting panel visibility, per-panel collapse buttons on the dividers.
