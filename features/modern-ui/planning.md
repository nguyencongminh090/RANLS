# Modern UI — planning

Status: **resolved 2026-10-08** (user answered in chat). Size **L** (UI-wide, new asset pipeline).

| # | Question | Decision |
|---|---|---|
| Q1 | Scope | Chrome + icons + CSS tokens. Panel restructure (sidebar/status bar) deferred. |
| Q2 | Icon source | Bundled hand-drawn filled symbolic SVGs in the GResource (`icons/scalable/actions/ranls-*-symbolic.svg`); filled paths only, because GTK's symbolic recolouring styles `fill`, not `stroke`. |
| Q3 | libadwaita | No — stay plain GTK4 (keeps the MSYS2 build). Own CSS tokens instead. |

Sequencing: icons + gresource → hamburger menu replaces `PopoverMenuBar` row → icon header bar +
engine-status icons → CSS tokens (`@define-color`, spacing/radius scale, hover, notebook tabs).
Deferred: sidebar/tabbed layout, status bar, toasts (`ui-ux-review` already lists toast as a gap).
