# UI-21 — Modern chrome: icon header bar, hamburger menu, CSS tokens

**Status:** ✅ DONE (2026-10-08, PR #55 squash `a5529c8`) — see below; light theme and Windows not verified.

Design: [features/modern-ui](../../features/modern-ui/planning.md). Size L.

- Bundled symbolic SVG icon set (12 icons) registered via `IconTheme::add_resource_path`.
- Menu-bar row removed; same `Gio::Menu` model in a header-bar `MenuButton` (F10 primary).
- Header bar: icon file/nav buttons with tooltip + accessible name, suggested-action Analyze, rule chip.
- Engine-status glyph buttons → icons.
- `style.css`: tokens, radius/spacing scale, hover states, notebook tabs, thin dividers.

Verification: full ctest (5/5) under Xvfb; manual screenshot on Xvfb (dark theme). Not verified: light theme, Windows.
